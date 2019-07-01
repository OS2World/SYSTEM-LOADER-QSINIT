//
// QSINIT "start" module
// CPU low level functions (msr, mtrr, etc)
//
#include "qsbase.h"
#include "cpudef.h"
#include "syslocal.h"
#include "seldesc.h"
#include "qsint.h"
#include "qsinit.h"
#include "efnlist.h"
#include "qecall.h"
#include "errno.h"
#include "ioint13.h"
#include "filetab.h"

#define FIXED_RANGE_REGS    11
#define MTRR_SAVE_BUFFER    32
#define MAX_VAR_RANGE_REGS  10
#define ACPI_RSDP_LENGTH    20

/** disable interrupts, cache and turn off MTRRs.
    @param [out]     state  State to save - for hlp_mtrrmend() call */
void _std hlp_mtrrmstart(volatile u32t *state);
/** enable interrupts, cache and turn on MTRRs.
    @param [in,out]  state  State, returned from hlp_mtrrmstart(),
                            clear it on exit to prevent second call */
void _std hlp_mtrrmend  (volatile u32t *state);

void _std fpu_nmhandler (void);

static u32t       mtrr_regs = 0,
                 mtrr_flags = 0,
              cpu_phys_bits = 0,
                cpu_lim8000 = 0,        // supported 8000xxxx cpuid limit
                   xcr0bits = 0,
               fpu_savesize = 0;        // save of XSAVE buffer
static u64t  PHYS_ADDR_MASK = 0,        // supported addr mask for this CPU
                  apic_phys = 0;
static u32t      *apic_data = 0;
static u32t      acpi_table = 0;        // EFI host only
qshandle             mhimux = 0;        // mutex for sys_memhicopy()
u8t            fpu_savetype = 0;

extern char            _std   aboutstr[];
extern boot_data       _std    boot_info;
extern struct Disk_BPB _std      BootBPB;
extern mt_proc_cb      _std mt_exechooks;

// fixed range MTRR registers
static u16t fixedmtrregs[FIXED_RANGE_REGS] = { MSR_IA32_MTRR_FIX64K_00000,
   MSR_IA32_MTRR_FIX16K_80000, MSR_IA32_MTRR_FIX16K_A0000,
   MSR_IA32_MTRR_FIX4K_C0000, MSR_IA32_MTRR_FIX4K_C8000,
   MSR_IA32_MTRR_FIX4K_D0000, MSR_IA32_MTRR_FIX4K_D8000,
   MSR_IA32_MTRR_FIX4K_E0000, MSR_IA32_MTRR_FIX4K_E8000,
   MSR_IA32_MTRR_FIX4K_F0000, MSR_IA32_MTRR_FIX4K_F8000};

u32t _std hlp_getmsrsafe(u32t index, u32t *ddlo, u32t *ddhi) {
   u32t rc;
   _try_ {
      if (ddlo) *ddlo=0;
      if (ddhi) *ddhi=0;
      rc = hlp_readmsr(index,ddlo,ddhi);
   }
   _catch_(xcpt_all) {
      log_it(2,"hlp_getmsrsafe(%d,%X,%X) - exception %d\n",
         index, ddlo, ddhi, _currentexception_());
      rc = 0;
   }
   _endcatch_
   return rc;
}

u32t _std hlp_setmsrsafe(u32t index, u32t ddlo, u32t ddhi) {
   u32t rc;
   _try_ {
      rc = hlp_writemsr(index,ddlo,ddhi);
   }
   _catch_(xcpt_all) {
      log_it(2,"hlp_setmsrsafe(%d,%X,%X) - exception %d\n",
         index, ddlo, ddhi, _currentexception_());
      rc = 0;
   }
   _endcatch_
   return rc;
}

static void init_baseinfo(void) {
   u32t data[4];
   if (cpu_phys_bits) return;
   // called after at least one sys_isavail(), so limit value is ready
   if (cpu_lim8000>=0x80000008) {
      // query MAXPHYWID
      _try_ {
         hlp_getcpuid(0x80000008,data);
      }
      _catch_(xcpt_all) {
         data[0] = 36;
      }
      _endcatch_
   } else
      data[0] = 36;
   cpu_phys_bits  = data[0] & 0x7F;
   // it is 0 on PPro, at least -> force to 36
   if (!cpu_phys_bits) cpu_phys_bits = 36;
   PHYS_ADDR_MASK = ((u64t)1<<cpu_phys_bits) - 1;

   // has Local APIC?
   if (sys_isavail(SFEA_LAPIC)) {
      if (hlp_getmsrsafe(MSR_IA32_APICBASE, data+0, data+1)) {
         if ((data[0]&0x800)==0) log_it(0, "APIC disabled!\n");
         apic_phys  = (u64t)data[1]<<32|data[0];
         apic_phys &= PHYS_ADDR_MASK & ~(u64t)PAGEMASK;
         // just as second chance
         if (!apic_phys) apic_phys = 0xFEE00000;

         apic_data  = (u32t*)pag_physmap(apic_phys, PAGESIZE, 0);
         if (!apic_data) log_it(3, "APIC map err: %LX\n", apic_phys); else {
            u32t  tmrdiv;
            log_it(3, "APIC: %X (%08X %08X %08X %08X %08X %08X)\n", apic_data[APIC_APICVER]&0xFF,
               apic_data[APIC_LVT_TMR], apic_data[APIC_SPURIOUS], apic_data[APIC_LVT_PERF],
                  apic_data[APIC_LVT_LINT0], apic_data[APIC_LVT_LINT1], apic_data[APIC_LVT_ERR]);

            tmrdiv = apic_data[APIC_TMRDIV];
            tmrdiv = (tmrdiv&3 | tmrdiv>>1&4) + 1 & 7;
            log_it(3, "APIC timer: %u %08X %08X\n", 1<<tmrdiv, apic_data[APIC_TMRINITCNT],
               apic_data[APIC_TMRCURRCNT]);
         }
      }
   }
   log_it(3, "cr0=%X cr4=%X\n", getcr0(), getcr4());
   // does not use XSAVE in safe mode
   if (sys_isavail(SFEA_XSAVE) && !hlp_insafemode()) {
      _try_ {
         sys_setcr3(FFFF, getcr4()|CPU_CR4_OSXSAVE);
         //set_xcr0(CPU_XCR0_FPU|CPU_XCR0_SSE);
         // no way to zero ecx for hlp_getcpuid, so call it here
         __asm {
            xor     ecx,ecx
            mov     eax,0Dh
            cpuid
            mov     xcr0bits,eax
            mov     fpu_savesize,ecx
         }
      }
      _catch_(xcpt_all) {
         xcr0bits     = 0;
         fpu_savesize = 0;
      }
      _endcatch_
      if (fpu_savesize) fpu_savetype = 2;
   }
   if (!fpu_savetype) {
      fpu_savetype = sys_isavail(SFEA_FXSAVE)?1:0;
      fpu_savesize = fpu_savetype?512:108;
   }
   // turn on SSE support!
   if (sys_isavail(SFEA_FXSAVE|SFEA_SSE1)==(SFEA_FXSAVE|SFEA_SSE1)) {
      // actually, it is on EFI already
      _try_ {
         sys_setcr3(FFFF, getcr4()|CPU_CR4_OSFXSR|CPU_CR4_OSXMMEXCPT);
      }
      _catch_(xcpt_all) {
         log_it(0, "SSE on - exception %d\n", _currentexception_());
      }
      _endcatch_
   }
   log_it(3, "fxsave %u %04X %u, cr4=%08X\n", fpu_savetype, xcr0bits, fpu_savesize, getcr4());

   if (hlp_hosttype()==QSHT_EFI) {
      // xcpt64 has special handling for us
   } else {
      u64t addr = (u64t)get_flatcs()<<32 | (u32t)&fpu_nmhandler;
      if (!sys_setint(7,&addr,SINT_INTGATE)) log_it(0, "unable to setup #NM!\n");
   }
   mt_exechooks.mtcb_rmcall = fpu_rmcallcb;
}

u32t _std fpu_statesize(void) { return fpu_savesize; }

void* _std sys_getlapic(void) {
   // it must be mapped by init_baseinfo() above
   return apic_data;
}

u32t _std hlp_getcputemp(void) {
   static int inited = 0;
   static u32t TMax  = 100;
   u32t data[4];

   // there is nothing critical here, run it without any MT locks
   if (!inited) {
      u32t model, stepping;
      inited = 1;
      // no cpuid
      if (!hlp_getcpuid(0,data)) return 0;
      // non-intel
      if (data[2]!=MAKEID4('n','t','e','l')) return 0;
      // digital temperature sensor is supported if set
      if (!hlp_getcpuid(6,data)) return 0;
      if ((data[0]&1)==0) return 0;
      // termal mon. present?
      if (!hlp_getcpuid(1,data)) return 0;
      if ((data[3]&CPUID_FI2_ACPI)==0) return 0;

      model    = data[0]>>4&0xF;
      stepping = data[0]&0xF;

      _try_ {
         // a long way of reading TMax, taken from BSD 8.0 code
         // it hangs on 1156 socket i5 at least ;)
         if (model==15&&stepping>=2||model==14) {
            hlp_readmsr(MSR_IA32_EXT_CONFIG,data+0,data+1);
            if (data[0] & 0x40000000) TMax = 85;
         } else
         if (model==0x17) {
            if (stepping==6) TMax = 105;
         } else
         if (model==0x1C) {
            TMax = stepping==10?100:90;
         } else {
            u32t value;
            hlp_readmsr(MSR_IA32_TEMPERATURE_TARGET,data+0,data+1);
            value = data[0]>>16&0xFF;
            if (value>=70&&value<=110) TMax = value;
         }
      }
      _catch_(xcpt_all) {
         log_it(2,"Exception in detecting TMax\n");
      }
      _endcatch_
      // mark as inited in any case
      inited = 2;
   }
   if (inited==2) {
      _try_ {
         hlp_readmsr(0x19C,data+0,data+1);
         if (data[0]&0x80000000) return TMax - (data[0]>>16&0x7F);
      }
      _catch_(xcpt_all) {
         inited=1;
         log_it(2,"There is no actual access to MSR 019Ch\n");
      }
      _endcatch_
   }
   return 0;
}

/** query mtrr info.
    @param [out] flags     ptr to mtrr info flags (can be 0)
    @param [out] state     ptr to mtrr current state (can be 0)
    @param [out] addrbits  ptr to physaddr bits, supported by processor
    @return number of variable range MTRR registers in processor */
u32t _std hlp_mtrrquery(u32t *flags, u32t *state, u32t *addrbits) {
   static int inited = 0;
   u32t data[4];

   if (flags) *flags = 0;
   if (state) *state = 0;
   if (addrbits) *addrbits = 32;

   mt_swlock();

   if (!inited) {
      inited = 1;
      // no cpuid/MTRR
      if (!hlp_getcpuid(1,data) || (data[3]&CPUID_FI2_MTRR)==0) {
         mt_swunlock();
         return 0;
      }
      hlp_getmsrsafe(MSR_IA32_MTRRCAP, data+0, data+1);
      mtrr_regs  = data[0] & 0x7F;
      mtrr_flags = (data[0]&0x100?MTRRQ_FIXREGS:0) | (data[0]&0x400?MTRRQ_WRCOMB:0) |
                   (data[0]&0x800?MTRRQ_SMRR:0);
      // this should never be called here, actually, but much earlier
      if (!cpu_phys_bits) init_baseinfo();
   }

   if (state) {
      hlp_getmsrsafe(MSR_IA32_MTRR_DEF_TYPE, data+0, data+1);
      *state = data[0];
   }
   if (flags) *flags = mtrr_flags;
   if (addrbits) *addrbits = cpu_phys_bits;
   mt_swunlock();
   return mtrr_regs;
}

/** read mtrr register.
    @param [in]  reg       variable range register index
    @param [out] start     start address
    @param [out] length    block length
    @param [out] state     block flags (see MTRRF_*)
    @return success flag (1/0) */
int  _std hlp_mtrrvread(u32t reg, u64t *start, u64t *length, u32t *state) {
   int  rc = 0;
   if (start)  *start  = 0;
   if (length) *length = 0;
   if (state)  *state  = 0;
   mt_swlock();
   if (!mtrr_regs) hlp_mtrrquery(0,0,0);
   if (reg<mtrr_regs && reg<MAX_VAR_RANGE_REGS) {
      u64t base, len;
      _try_ {
         hlp_readmsr(MSR_IA32_MTRR_PHYSBASE0+reg*2, (u32t*)&base, (u32t*)&base+1);
         hlp_readmsr(MSR_IA32_MTRR_PHYSMASK0+reg*2, (u32t*)&len , (u32t*)&len +1);
         rc = 1;
      }
      _catch_(xcpt_all) {
         log_it(2,"Exception in MTRR_PHYS_%d\n",reg);
         base = 0; len = 0;
      }
      _endcatch_

      // log_it(2,"%u: %012LX %012LX\n", reg, base, len);
      if (rc) {
         if (state) *state = (u32t)base & MTRRF_TYPEMASK |
                             ((u32t)(len)&0x800?MTRRF_ENABLED:0);
         base = base & ~0xFFFLL & PHYS_ADDR_MASK;
         if (start) *start = base;
         if (length) {
            u64t mask = len & ~0xFFFLL & PHYS_ADDR_MASK, ebase;
            int  left, right;
            // make mask without holes
            left  = bsr64(mask);
            right = bsf64(mask);

            if (left>0) {
               ebase = (u64t)1<<right;
               mask = 0;
               while (right<=left) mask |= (u64t)1<<right++;
               *length = ebase;
            }
         }
      }
   }
   mt_swunlock();
   return rc;
}

/** read fixed range mtrr register.
    Function merge neighboring fixed blocks with the same cache types
    and return list in readable format.
    @param [out] start     array of [MTRR_FIXEDMAX] start adresses
    @param [out] length    array of [MTRR_FIXEDMAX] block lengths
    @param [out] state     array of [MTRR_FIXEDMAX] cache types
    @return number of filled entries */
u32t _std hlp_mtrrfread(u32t *start, u32t *length, u32t *state) {
   u32t rc = 0;
   if (!start || !length || !state)  return 0;
   mt_swlock();
   if (!mtrr_regs) hlp_mtrrquery(0,0,0);
   if ((mtrr_flags&MTRRQ_FIXREGS)) {
      u32t ii, idx, ctype = FFFF, mempos = 0;

      for (ii=0; ii<FIXED_RANGE_REGS; ii++) {
         u64t bits;
         u32t step = !ii?_64KB:(ii<3?_16KB:_4KB);

         hlp_getmsrsafe(fixedmtrregs[ii], (u32t*)&bits, (u32t*)&bits+1);

         for (idx=0; idx<8; idx++) {
            u32t ct = (u32t)(bits>>(idx<<3)) & MTRRF_TYPEMASK;
            if (ct!=ctype) {
               start [rc] = mempos;
               length[rc] = step;
               state [rc] = ct;
               ctype = ct;
               rc++;
            } else
               length[rc-1] += step;
            mempos += step;
         }
      }
   }
   mt_swunlock();
   return rc;
}

static u8t biosmtrrs[MTRR_SAVE_BUFFER*10];
static int resetdata = 0,
        mtrr_changes = 0;

static int check_save(u32t cbit) {
   mt_swlock();
   if (!mtrr_regs) hlp_mtrrquery(0,0,0);
   // we have MTRR & write first time -> make backup of BIOS state
   if (mtrr_regs && !resetdata) {
      hlp_mtrrbatch(biosmtrrs);
      mtrr_changes |= cbit;
      resetdata = 1;
   }
   mt_swunlock();
   return mtrr_regs?1:0;
}

u32t _std START_EXPORT(hlp_mtrrstate)(u32t state) {
   // incorrect mask
   if ((state&MTRRS_DEFMASK)>MTRRF_WB ||
      (state&~(MTRRS_DEFMASK|MTRRS_FIXON|MTRRS_MTRRON|MTRRS_NOCALBRT))!=0)
         return MTRRERR_STATE;
   // save original state
   if (!check_save(1)) return MTRRERR_HARDW;

   _try_ {
      u32t low, high = 0;
      volatile u32t sysstate = 0;
      int  calibrate = 0;
      // disable interrupts and cache
      hlp_mtrrmstart(&sysstate);
      // read current state
      hlp_readmsr(MSR_IA32_MTRR_DEF_TYPE,&low,&high);
      // save rc in variable
      low = hlp_writemsr(MSR_IA32_MTRR_DEF_TYPE,state&~MTRRS_NOCALBRT,high);
      // need to call i/o delay calibrate
      calibrate = state&MTRRS_MTRRON^sysstate&MTRRS_MTRRON;
      // restore system state
      if (state&MTRRS_MTRRON) sysstate|=MTRRS_MTRRON; else sysstate&=~MTRRS_MTRRON;
      hlp_mtrrmend(&sysstate);
      // update i/o delay
      if ((state&MTRRS_NOCALBRT)==0 && calibrate) tm_calibrate();
      // return ok if MSR was written without error
      if (low) _ret_in_try_(MTRRERR_OK);
   }
   _catch_(xcpt_all) {
      log_it(2,"Exception in hlp_mtrrstate()\n");
   }
   _endcatch_
   return MTRRERR_ACCESS;
}

u32t _std START_EXPORT(hlp_mtrrvset)(u32t reg, u64t start, u64t length, u32t state) {
   int enable = state&MTRRF_ENABLED;
   // save before first modification
   if (!check_save(4)) return MTRRERR_HARDW;
   if (reg>=mtrr_regs) return MTRRERR_INDEX;

   // allow zero length with disabled status only
   if (length || !enable) {
      volatile u32t sysstate = 0;
      int  right = bsf64(length);
      // must be a power of two
      if (length) {
         if ((u64t)1<<right!=length) return MTRRERR_LENGTH;
         if (start) {
            int st_right = bsf64(start);
            if (st_right<right) return MTRRERR_ADDRTRN;
         }
      }
      if ((start&0xFFF)!=0 || start>PHYS_ADDR_MASK)
         return MTRRERR_ADDRESS;
      // incorrect state
      if ((state&MTRRF_TYPEMASK)>MTRRF_WB ||
         (state&~(MTRRF_ENABLED|MTRRF_TYPEMASK))!=0) return MTRRERR_STATE;
      state &= MTRRF_TYPEMASK;
      length = ~(length-1);
      length&= PHYS_ADDR_MASK;

      log_it(2,"set MTRR_%d: %012LX-%012LX\n",reg, start+state, length|(enable?0x800:0));

      _try_ {
         hlp_mtrrmstart(&sysstate);
         // disable register
         hlp_writemsr(MSR_IA32_MTRR_PHYSMASK0+reg*2,length,length>>32);
         // set start address
         hlp_writemsr(MSR_IA32_MTRR_PHYSBASE0+reg*2,start+state,start>>32);
         // enable register
         if (enable)
            hlp_writemsr(MSR_IA32_MTRR_PHYSMASK0+reg*2,length|0x800,length>>32);
         // restore system state
         hlp_mtrrmend(&sysstate);
         // return
         _ret_in_try_(MTRRERR_OK);
      }
      _catch_(xcpt_all) {
         log_it(2,"Exception in MTRR_PHYS_%d set\n",reg);
      }
      _endcatch_

      _try_ {
         hlp_mtrrmend(&sysstate);
      }
      _catch_(xcpt_all) {
         log_it(2,"Exception in hlp_mtrrmend(%08X)\n",sysstate);
      }
      _endcatch_
      return MTRRERR_ACCESS;
   }
   return MTRRERR_LENGTH;
}

u32t _std START_EXPORT(hlp_mtrrfset)(u32t addr, u32t length, u32t state) {
   // save before first modification
   if (!check_save(2)) return MTRRERR_HARDW;
   // no fixed regs?
   if ((mtrr_flags&MTRRQ_FIXREGS)==0) {
      mtrr_changes &= ~2;
      return MTRRERR_HARDW;
   }
   if (length) {
      u32t ii, start, allow, rc=1;
      volatile u32t sysstate = 0;

      // check fixed borders align
      for (ii=0; ii<2; ii++) {
         u32t ca=ii?addr+length:addr, err=1;
         if (ca<0x80000) {
            if ((ca&0xFFFF)==0) err=0;
         } else
         if (ca<0xC0000) {
            if ((ca&0x3FFF)==0) err=0;
         } else
         if (ca<=0x100000) {
            if ((ca& 0xFFF)==0) err=0;
         }
         if (err) return ii?MTRRERR_LENGTH:MTRRERR_ADDRESS;
      }

      // disable cache, interrupts and MTRR!
      hlp_mtrrmstart(&sysstate);
      // iterate registers list
      for (ii=0, start=0, allow=0; ii<FIXED_RANGE_REGS; ii++) {
         u32t step = !ii?_64KB:(ii<3?_16KB:_4KB),
             csize = step*8;
         if (!allow)
            if (start+csize>addr) allow=1;
         if (allow) {
            int partial = allow==1 || start+csize>addr+length;
            u64t   bits;
            // only part of fixed reg must be changed, so read and replace bits
            if (partial) {
               if (!hlp_getmsrsafe(fixedmtrregs[ii],(u32t*)&bits,(u32t*)&bits+1))
                  rc=0;
               else {
                  int ps=0, pe=8;
                  if (allow==1) ps=(addr-start)/step;
                  if (start+csize>addr+length) pe=(addr+length-start)/step;
                  while (ps<pe)
                     ((u8t*)&bits)[ps++] = state&7;
               }
            } else
               memset(&bits,state&7,8);
            // change register
            if (rc)
               if (!hlp_setmsrsafe(fixedmtrregs[ii],bits,bits>>32)) rc=0;
            allow++;
         }
         start+=csize;
         if (!rc || start>=addr+length) break;
      }
      hlp_mtrrmend(&sysstate);

      return rc?MTRRERR_OK:MTRRERR_ACCESS;
   }
   return MTRRERR_LENGTH;
}


u32t _std hlp_mtrrsum(u64t start, u64t length) {
   u32t ctype = FFFF, m_flags, m_state;
   /* drop any args, which cross 1Mb border (because of BIOS on F000
      and this split/simplify processing of fixed and variable ranges) */
   if (start<_1MB && start+length>_1MB) return MTRRF_TYPEMASK;
   // validate it a bit
   if (!length || start>PHYS_ADDR_MASK || start+length>PHYS_ADDR_MASK ||
      start+length<start) return MTRRF_TYPEMASK;
   // current MTRR state
   if (!hlp_mtrrquery(&m_flags, &m_state, 0)) return MTRRF_UC;
   if ((m_state&MTRRS_MTRRON)==0) return MTRRF_UC;

   // process fixed type
   if (start<_1MB && (m_flags&MTRRQ_FIXREGS) && (m_state&MTRRS_FIXON)) {
      u32t  c_st = start, c_len = length, ii, idx,
          mempos = 0,   inrange = 0;

      for (ii=0; ii<FIXED_RANGE_REGS; ii++) {
         u32t step = !ii?_64KB:(ii<3?_16KB:_4KB),
             csize = step*8;
         if (!inrange)
            if (mempos+csize>c_st) inrange=1;
         if (inrange) {
            u64t bits;
            if (!hlp_getmsrsafe(fixedmtrregs[ii], (u32t*)&bits, (u32t*)&bits+1))
               return MTRRF_TYPEMASK;
            else
            for (idx=0; idx<8; idx++) {
               u32t ct = (u32t)(bits>>(idx<<3)) & MTRRF_TYPEMASK;
               mempos += step;
               // skip bits before requested range
               if (mempos<=c_st) continue;
               if (ct!=ctype)
                  if (ctype==FFFF) ctype = ct; else return MTRRF_TYPEMASK;
               if (mempos>=c_st+c_len) break;
            }
         } else
            mempos += csize;
         if (mempos>=c_st+c_len) break;
      }
   } else {
      u32t   ii, defct = m_state&MTRRS_DEFMASK, lctype;
      u64t   ab[MAX_VAR_RANGE_REGS],
             am[MAX_VAR_RANGE_REGS], lmask;
      u8t   act[MAX_VAR_RANGE_REGS];
      int  rmax, lhi;
      // align args to page
      length += start&PAGEMASK;
      start >>= PAGESHIFT;
      length  = PAGEROUNDLL(length) >> PAGESHIFT;
      // bit range of length changes (0..lhi)
      lhi   = bsr64(length-1);
      lmask = (1LL<<lhi+1) - 1;

      for (ii=0, rmax=-1; ii<mtrr_regs; ii++) {
         u64t base, mask, ct;
         int   err = 0;
         _try_ {
            hlp_readmsr(MSR_IA32_MTRR_PHYSBASE0+ii*2, (u32t*)&base, (u32t*)&base+1);
            hlp_readmsr(MSR_IA32_MTRR_PHYSMASK0+ii*2, (u32t*)&mask, (u32t*)&mask+1);
         }
         _catch_(xcpt_all) { err=1; }
         _endcatch_
         if (err) return MTRRF_TYPEMASK;
         // register is off
         if ((mask&0x800)==0) continue;
         ct   = (u8t)base & MTRRF_TYPEMASK;
         base = (base & PHYS_ADDR_MASK) >> PAGESHIFT;
         mask = (mask & PHYS_ADDR_MASK) >> PAGESHIFT;
         // our block fits into mask
         if ((lmask&mask)==0)
            if ((start&mask)==base && (start+length-1&mask)==base) {
               if (ctype==FFFF || ctype==MTRRF_WB) ctype = ct; else
                  ctype = MTRRF_UC;
               continue;
            }
         if (++rmax>MAX_VAR_RANGE_REGS) return MTRRF_TYPEMASK;
         // collect used regs only
         act[rmax] = ct;
         ab [rmax] = base;
         am [rmax] = mask;
      }
      if (rmax<0) return ctype==FFFF?defct:ctype;

      // process every page if block crosses mask borders or mask is non-solid
      for (ii=0, lctype=ctype; ii<length; ii++) {
         u64t addr = start+ii;
         u32t ti, tct = ctype;

         for (ti=0; ti<=rmax; ti++)
            if ((addr&am[ti])==ab[ti])
               if (tct==FFFF || tct==MTRRF_WB) tct = act[ti]; else
                  tct = MTRRF_UC;
         if (tct==FFFF) tct=defct;
         if (!ii) lctype = tct; else
            if (lctype != tct) return MTRRF_TYPEMASK;
      }
      ctype = lctype==FFFF?defct:lctype;
   }
   return ctype;
}


void _std hlp_mtrrbios(void) {
   mt_swlock();
   if (resetdata) {
      hlp_mtrrapply(biosmtrrs);
      tm_calibrate();
      mtrr_changes = 0;
      resetdata = 0;
      log_it(2,"hlp_mtrrbios() done\n");
      // delete memlimit from VMTRR
      sto_del(STOKEY_MEMLIM);
   }
   mt_swunlock();
}

int _std hlp_mtrrchanged(int state, int fixed, int variable) {
   int rc = 0;
   mt_swlock();
   if (state    && (mtrr_changes&1)!=0) rc = 1;
   if (fixed    && (mtrr_changes&2)!=0) rc = 1;
   if (variable && (mtrr_changes&4)!=0) rc = 1;
   mt_swunlock();
   return rc;
}

u32t _std hlp_mtrrbatch(void *buffer) {
   memset(buffer, 0, MTRR_SAVE_BUFFER*10);

   if (mtrr_regs*2+FIXED_RANGE_REGS+1>MTRR_SAVE_BUFFER) {
      log_it(2,"Too many MSR regs for MTRR batch buffer\n");
      return 0;
   } else {
      int cnt = 0, ii = 0;
      u16t *cb = (u16t*)buffer;
      mt_swlock();
      if (mtrr_flags&MTRRQ_FIXREGS) {
          // if you put cnt as variable here - OW 1.7 will produce wrong code ;)
          while (ii<FIXED_RANGE_REGS) {
             *cb++ = fixedmtrregs[ii];
             hlp_getmsrsafe(fixedmtrregs[ii],(u32t*)cb,(u32t*)(cb+2));
             cb+=4; ii++;
          }
          cnt+=FIXED_RANGE_REGS;
      }
      _try_ {
         for (ii=0; ii<mtrr_regs; ii++) {
            *cb++ = MSR_IA32_MTRR_PHYSBASE0+ii*2;
            hlp_readmsr(MSR_IA32_MTRR_PHYSBASE0+ii*2,(u32t*)cb,(u32t*)(cb+2));
            cb+=4;
            *cb++ = MSR_IA32_MTRR_PHYSMASK0+ii*2;
            hlp_readmsr(MSR_IA32_MTRR_PHYSMASK0+ii*2,(u32t*)cb,(u32t*)(cb+2));
            cb+=4;
         }
         *cb++ = MSR_IA32_MTRR_DEF_TYPE;
         hlp_readmsr(MSR_IA32_MTRR_DEF_TYPE,(u32t*)cb,(u32t*)(cb+2));
         cb +=4;
         cnt+= 2*mtrr_regs+1;
      }
      _catch_(xcpt_all) {
         log_it(2,"Exception in hlp_mtrrbatch\n");
         cnt = 0;
      }
      _endcatch_
      mt_swunlock();
      return cnt;
   }
}

/** memcpy with SS segment as source & destination.
    I.e., function have access to 1st page in non-paging mode.
    Actually this is memmove, not memcpy. */
void* _std memcpy0(void *dst, const void *src, u32t length);
// 48-bit ptr to page 0, but we use only offset here
extern u32t _std page0_fptr;

static u32t hlp_memcpy_int(void *dst, const void *src, u32t length, int page0) {
   _try_ {
      /* page0 flag affects SOURCE access in paging mode and
         SOURCE+DST access in non-paging */
      if (page0) memcpy0(dst,src,length); else memmove(dst,src,length);
      _ret_in_try_(1);
   }
   _catch_(xcpt_all) {
      log_it(2,"Exception in memcpy(%08X, %08X, %d, %d)\n", dst, src, length, page0);
   }
   _endcatch_
   return 0;
}

u32t _std hlp_memcpy(void *dst, const void *src, u32t length, u32t flags) {
   u32t svint = 0, rc = 0;
   if ((flags&MEMCPY_DI)) svint = sys_intstate(0);

   if ((flags&MEMCPY_PG0) && in_pagemode) {
      do {
         u32t  dstv = (u32t)dst;
         char *srcv = (char*)src;
         // wrap around 0?
         if (dstv+length < dstv) {
            u32t upto0 = FFFF-dstv+1;
            if (!hlp_memcpy_int(dst, srcv, upto0, 1)) break;
            dst = 0; dstv = 0;
            srcv   += upto0;
            length -= upto0;
         }
         if (!length) { rc = 1; break; }
         /* 1st page copying.
            this madness here because page is r/o in paging mode */
         if (dstv < PAGESIZE) {
            u32t upto4 = dstv+length > PAGESIZE ? PAGESIZE-dstv : length;

            if (!hlp_memcpy_int((void*)(page0_fptr+dstv), srcv, upto4, 1))
               break;
            dstv   += upto4; dst = (void*)dstv;
            srcv   += upto4;
            length -= upto4;
         }
         if (!length) { rc = 1; break; }
         // above 1st page - normal copying
         rc = hlp_memcpy_int(dst, srcv, length, 1);
      } while (0);
   } else
      rc = hlp_memcpy_int(dst, src, length, flags&MEMCPY_PG0);

   if (svint) sys_intstate(svint);
   return rc;
}

/** query some CPU features in friendly way.
    @param  flags   SFEA_* flags combination
    @return actually supported subset of flags parameter */
u32t _std sys_isavail(u32t flags) {
   static u32t sflags = FFFF;
   // first time call is too early to think about safeness ;)
   if (sflags==FFFF) {
      u32t   idbuf[4];
      char  idstr[16];
      u8t      family, model, stepping;
      hlp_getcpuid(1,idbuf);
      sflags = 0;
      if (idbuf[3]&CPUID_FI2_PAE)   sflags|=SFEA_PAE;
      if (idbuf[3]&CPUID_FI2_PGE)   sflags|=SFEA_PGE;
      if (idbuf[3]&CPUID_FI2_PAT)   sflags|=SFEA_PAT;
      if (idbuf[3]&CPUID_FI2_CMOV)  sflags|=SFEA_CMOV;
      if (idbuf[3]&CPUID_FI2_MTRR)  sflags|=SFEA_MTRR;
      if (idbuf[3]&CPUID_FI2_MSR)   sflags|=SFEA_MSR;
      if (idbuf[3]&CPUID_FI2_ACPI)  sflags|=SFEA_CMODT;
      if (idbuf[3]&CPUID_FI2_APIC)  sflags|=SFEA_LAPIC;
      if (idbuf[3]&CPUID_FI2_FXSR)  sflags|=SFEA_FXSAVE;
      if (idbuf[3]&CPUID_FI2_SSE)   sflags|=SFEA_SSE1;
      if (idbuf[3]&CPUID_FI2_SSE2)  sflags|=SFEA_SSE2;
      if (idbuf[2]&CPUID_FI1_XSAVE) sflags|=SFEA_XSAVE;
      if (idbuf[2]&CPUID_FI1_INVM)  sflags|=SFEA_INVM;

      family   = idbuf[0]>>8&0xF;
      model    = idbuf[0]>>4&0xF;
      stepping = idbuf[0]&0xF;

      hlp_getcpuid(0x80000000,idbuf);
      cpu_lim8000 = idbuf[0];

      hlp_getcpuid(0,idbuf);
      *((u32t*)idstr+0)=idbuf[1]; *((u32t*)idstr+1)=idbuf[3];
      *((u32t*)idstr+2)=idbuf[2]; idstr[12]=0;

      if (strcmp(idstr,"GenuineIntel")==0) sflags|=SFEA_INTEL; else
      if (strcmp(idstr,"AuthenticAMD")==0) {
         sflags|= SFEA_AMD;
         sflags&=~SFEA_CMODT;
         /* P-State is OFF: have no modern AMD to test it and no setup in doshlp
            for secondary cores (they declared as independent by AMD docs).
            And i don`t want write this FID/VID madness for older CPUs ;) */
#if 0
         // query AMD P-State support
         if (cpu_lim8000>=0x80000007) {
            hlp_getcpuid(0x80000007,idbuf);
            if ((idbuf[3]&CPUID_FI5_PSTATE)) sflags|=SFEA_CMODT;
         }
#endif
      }

      if (cpu_lim8000>=0x80000001) {
         hlp_getcpuid(0x80000001,idbuf);
         if ((idbuf[3]&CPUID_FI4_IA64)) sflags|=SFEA_X64;
      }
      // fix for Banias Pentium M bug (missing PAE bit in CPUID)
      if ((sflags&SFEA_INTEL) && family==6 && model==9 && (sflags&SFEA_PAE)==0)
         sflags|=SFEA_PAE;

      if (sflags&SFEA_INVM) {
         memset(&idbuf, 0, sizeof(idbuf));
         hlp_getcpuid(0x40000000,idbuf);
         if (idbuf[1]) {
            *((u32t*)idstr+0)=idbuf[1]; *((u32t*)idstr+1)=idbuf[2];
            *((u32t*)idstr+2)=idbuf[3]; idstr[12]=0;
            log_it(3, "Hypervisor vendor string: %s\n", idstr);
         }
      } else
      // detect VBox 4.x (it has no special bit in cpuid).
      if (sys_acpiroot())
         if (patch_binary((u8t*)sys_acpiroot()+8,8,1,"VBOX",4,0,0,0,0)==0)
            sflags|=SFEA_INVM;
      /* we have CMOV and have no LAPIC? this can be a Virtual PC, but also
         this can be an old P2 with disabled LAPIC */
      if ((sflags&(SFEA_INVM|SFEA_CMOV|SFEA_LAPIC))==SFEA_CMOV) {
         _try_ {
            __asm {  // VPC get time from host
               db  0Fh, 3Fh, 3, 0
            }
            sflags|=SFEA_INVM;
         }
         _catch_(xcpt_all) {}
         _endcatch_
      }
      log_it(3, "SFEA_* = %08X (%u.%u.%u)\n", sflags, family, model, stepping);
   }
   return flags&sflags;
}

int _std sys_is64mode(void) {
   if ((getcr0()&CPU_CR0_PG)==0) return 0;
   if ((getcr4()&CPU_CR4_PAE)==0) return 0;

   if (sys_isavail(SFEA_X64)) {
      u32t  ld=0, hd;
      hlp_getmsrsafe(MSR_IA32_EFER,&ld,&hd);
      // 64-bit paging?
      if ((ld&(MSR_EFER_IA32E|MSR_EFER_IA32E_ON))==(MSR_EFER_IA32E|MSR_EFER_IA32E_ON))
         return 1;
   }
   return 0;
}


#define MAP_SIZE                  (_8MB)
#define MAP_MASK       (~(u64t)(_8MB-1))

u32t _std sys_memhicopy(u64t dst, u64t src, u64t length) {
   // negative size
   if ((s64t)length<=0) return 0;
   // 4Gb border
   if (src<_4GBLL && src+length>_4GBLL) return 0;
   if (dst<_4GBLL && dst+length>_4GBLL) return 0;
   // overflow check
   if (src+length<src) return 0;
   if (dst+length<dst) return 0;
   // 1st page
   if (src<_4KB || dst<_4KB) return 0;

   if (hlp_hosttype()==QSHT_EFI) {
      _try_ {
         call64(EFN_MEMMOVE, 7, 4, dst, src, length, 0);
      }
      _catch_(xcpt_all) {
         log_it(2,"Exception in sys_memhicopy(%09LX, %09LX, %Ld)\n", dst, src, length);
      }
      _endcatch_
   } else {
      static void *map[2] = { 0, 0 };
      static u64t madr[2] = { 0, 0 };
      u64t         adr[2];
      int           hi[2];

      if (src<_4GBLL && dst<_4GBLL)
         return hlp_memcpy((void*)(u32t)dst, (void*)(u32t)src, length, 0) ? 1 : 0;
      if (!in_pagemode)
         if (!pag_enable()) return 0;

      if (in_mtmode)
         if (mt_muxcapture(mhimux)) return 0;

      hi[0] = (adr[0]=src)>=_4GBLL;
      hi[1] = (adr[1]=dst)>=_4GBLL;
      while (length) {
         void *tptr[2];
         u32t   cpi = MAP_SIZE, ii;

         // bytes until the end of 8Mb borders
         for (ii=0; ii<2; ii++)
           if (hi[ii]) {
              u32t cpl = MAP_SIZE - ((u32t)adr[ii] & MAP_SIZE-1);
              if (cpl<cpi) cpi = cpl;
           }
         if ((u64t)cpi > length) cpi = length;
         // map 8Mb blocks
         for (ii=0; ii<2; ii++)
            if (!hi[ii]) tptr[ii] = 0; else
            if ((adr[ii]&MAP_MASK)==madr[ii]) tptr[ii] = 0; else {
               tptr[ii] = map[ii];
               madr[ii] = adr[ii]&MAP_MASK;
               map[ii]  = pag_physmap(madr[ii], MAP_SIZE, 0);

               if (!map[ii]) {
                  if (in_mtmode) mt_muxrelease(mhimux);
                  return 0;
               }
            }
         for (ii=0; ii<2; ii++) {
            // unmap AFTER map to use inc/dec usage counter properly
            if (tptr[ii]) pag_physunmap(tptr[ii]);
            // src/dst address
            tptr[ii] = hi[ii]?(u8t*)map[ii]+((u32t)adr[ii]&_8MB-1):(u8t*)(u32t)adr[ii];
         }
         // copying...
         _try_ {
            memmove(tptr[1], tptr[0], cpi);
         }
         _catch_(xcpt_all) {
            log_it(2,"Exception in sys_memhicopy(%09LX, %09LX, %Ld)\n", dst, src, length);
         }
         _endcatch_

         adr[0] += cpi;
         adr[1] += cpi;
         length -= cpi;
      }
      if (in_mtmode) mt_muxrelease(mhimux);
   }
   return 1;
}

/* a bit crazy code, trying to be safe here, because it called
   from the trap screen */
u32t  _std hlp_copytoflat(void* dst, u32t offs, u32t sel, u32t len) {
   struct desctab_s sd;
   if (!len) return 0;
   // loop over 4G?
   if (offs+len<offs) len = 0-offs;
   // query selector and check it
   if (!sys_selquery(sel,&sd)) return 0;
   if ((sd.d_access & D_PRES)==0) return 0;
   if (sd.d_attr & D_LONG) {
      if (sd.d_attr & D_DBIG) return 0;
   } else
   if ((sd.d_access & D_SEG)==0) return 0; else {
      int  code = sd.d_access & D_CODE,
            edn = !code && (sd.d_access&D_EXPDN);
      u32t base = sd.d_loaddr | (u32t)sd.d_hiaddr<<16 | (u32t)sd.d_extaddr<<24,
            lim = _lsl_(sel);
      if (edn) {
         if (offs<lim+1+base) return 0;
      } else {
         if (offs>lim) return 0;
         // > lim or just ends on 4Gb
         if (offs+len>lim || offs+len==0) len = lim+1-offs;
      }
      // flat offset
      offs += base;
   }
   // have something to copy: then copy page by page, with checking page tables
   if (len) {
      u32t cblen = len;
      u8t  *sptr = (u8t*)offs,
           *dptr = (u8t*)dst;
      while (len) {
         u32t cpsize = PAGEROUND(offs) - offs;
         if (!cpsize) cpsize = PAGESIZE;
         if (cpsize>len) cpsize = len;

         if (pag_query(sptr)<=PGEA_NOTPRESENT) break;
         memcpy0(dptr, sptr, cpsize);
         dptr += cpsize;
         sptr += cpsize; offs = (u32t)sptr;
         len  -= cpsize;
      }
      return cblen - len;
   }
   return 0;
}

static int      clockmod_aerr =  0;
static int       clockmod_ext  = -1;
extern u16t __stdcall IODelay;

u32t _std hlp_cmgetstate(void) {
   u32t  rc, value,
       info = sys_isavail(SFEA_CMODT|SFEA_AMD);
   if (clockmod_aerr || (info&SFEA_CMODT)==0) return 0;

   if ((info&SFEA_AMD)) {
      rc = hlp_getmsrsafe(MSR_AMD_PSTATE_LIMIT, &info, 0);
      if (!rc) { clockmod_aerr = 1; return 0; }
      rc = hlp_getmsrsafe(MSR_AMD_PSTATE_STATUS, &value, 0);
      if (!rc) { clockmod_aerr = 1; return 0; }

      info  = info>>4 & 0xF;
      value = value&0xF;

      if (16%(info+1)==0) info = 16/(info+1); else info = 1;
      return 16-value*info;
   } else {
      if (clockmod_ext<0) {
         u32t   idbuf[4];
         hlp_getcpuid(6,idbuf);
         // bit 5 in CPUID.06H:EAX
         clockmod_ext = idbuf[0]&0x20?1:0;
         log_it(3,"cm = %d\n", 3+clockmod_ext);
      }
      rc = hlp_getmsrsafe(MSR_IA32_CLOCKMODULATION, &value, 0);
      if (!rc) { clockmod_aerr = 1; return 0; }

      return value&0x10 ? value&0xF-(1-clockmod_ext) : CPUCLK_MAXFREQ;
   }
}

u32t _std START_EXPORT(hlp_cmsetstate)(u32t state) {
   u32t info = sys_isavail(SFEA_CMODT|SFEA_AMD), rc;

   if (clockmod_aerr || (info&SFEA_CMODT)==0) return ENODEV;
   if (!state || state>CPUCLK_MAXFREQ) return EINVAL;

   if ((info&SFEA_AMD)) {
      rc = hlp_getmsrsafe(MSR_AMD_PSTATE_LIMIT, &info, 0);
      if (!rc) return ENODEV;

      info = info>>4 & 0xF;
      if (16%(info+1)==0) info = 16/(info+1); else info = 1;
      //
      mt_swlock();
      rc = hlp_setmsrsafe(MSR_AMD_PSTATE_CONTROL, (CPUCLK_MAXFREQ-state)/info, 0);
   } else {
      rc = hlp_cmgetstate();

      if (clockmod_ext<=0) {
         state = state&~1;
         if (!state) state = 2;
      }
      rc = state==CPUCLK_MAXFREQ ? 0x0E : 0x10 + state;
      // lock it between setup & tm_calibrate() with notify (even if not required)
      mt_swlock();
      rc = hlp_setmsrsafe(MSR_IA32_CLOCKMODULATION, rc, 0);
   }
   if (rc) {
      tm_calibrate();
      sys_notifyexec(SECB_CMCHANGE, 0);
   }
   mt_swunlock();
   return rc?0:ENODEV;
}

static void _std cm_restore(sys_eventinfo *info) {
   if (sys_isavail(SFEA_CMODT) && !sto_dword(STOKEY_CMMODE)) {
      u32t state = hlp_cmgetstate();
      if (state>0 && state<CPUCLK_MAXFREQ) hlp_cmsetstate(CPUCLK_MAXFREQ);
   }
#if 0
   // reset xcr0
   if (sys_isavail(SFEA_XSAVE))
      if (get_xcr0()&CPU_XCR0_SSE) set_xcr0(CPU_XCR0_FPU);
#endif
}

/* we have 2 variants of memory location: 1st MB in BIOS host and top of ram
   in UEFI host. In both cases area must be mapped and readable */
static int check_rsdp(u32t mem) {
   static const char *sig = "RSD PTR ";
   u8t *ptr = (u8t*)mem;
   if (!ptr) return 0;
   if (*(u64t*)ptr==*(u64t*)sig) {
      u8t sum = 0, ii = 0;
      // check standard checksum only, this is well enough for us
      while (ii<ACPI_RSDP_LENGTH) sum += ptr[ii++];
      if (sum==0) return 1;
   }
   return 0;
}

u32t _std sys_acpiroot(void) {
   if (acpi_table) return acpi_table; else {
      u32t xbda = 0;
      // do it in intel way
      if (!hlp_memcpy(&xbda, (void*)(0x40E), 2, MEMCPY_PG0)) xbda = 0; else
         if (xbda<0x400) xbda = 0;

      while (1) {
         u32t addr, len, ofs;
         if (xbda) { addr = xbda<<PARASHIFT; len = _1KB; } else
            { addr = 0xE0000; len = _128KB; }

         for (ofs=0; ofs<len; ofs+=16)
            if (check_rsdp(addr+ofs)) return acpi_table = addr+ofs;

         if (!xbda) break; else xbda = 0;
      }
      return 0;
   }
}

void* malloc_local(u32t size) {
   void *rc = malloc(size);
   if (rc) mem_localblock(rc);
   return rc;
}

u32t _std sys_queryinfo(u32t index, void *outptr) {
   switch (index) {
      case QSQI_VERSTR: {
         u32t len = strlen(aboutstr)+1;
         if (outptr) memcpy(outptr, aboutstr, len);
         return len;
      }
      case QSQI_OS2LETTER: {
         if ((boot_info.boot_flags&BF_NEWBPB)) return 0;
         return BootBPB.BPB_BootLetter&0x80 ? (BootBPB.BPB_BootLetter&0x7F)+'C' : 0;
      }
      case QSQI_OS2BOOTFLAGS:
         return boot_info.boot_flags;
      case QSQI_TLSPREALLOC:
         return QTLS_MAXSYS+1;
      case QSQI_CPUSTRING: {
         static char idstr[50] = {0}, *cp;
         u32t        idlen;
         str_list    *flst;

         if (!idstr[0]) {
            u32t  dd[4], maxlevel = 0, ii;
            if (hlp_getcpuid(0x80000000,dd)) maxlevel = dd[0];

            if (maxlevel>=0x80000004) {
               u32t level = 0x80000002;
               cp  = idstr;
               *cp = 0;
               while (level<0x80000005) {
                 hlp_getcpuid(level, dd);
                 memcpy(cp+(level++-0x80000002)*16, dd, 16);
               }
               cp[48] = 0;
               for (ii=0;ii<48;ii++)
                  if (cp[ii]==0) cp[ii]=' ';
               for (ii=48;ii>0;)
                  if (cp[--ii]!=' ') break; else cp[ii]=0;
               if (cp[0]==' ') {
                  char *ps=cp;
                  while (*ps==' ') ps++;
                  ii=ps-cp;
                  if (ii==48) *cp=0; else
                     memmove(cp,ps,48-ii+1);
               }
            }
            // clean multiple spaces inside of the name
            flst = str_split(idstr, " ");
            cp   = str_gettostr(flst, " ");
            free(flst);
            strncpy(idstr,cp,50); idstr[49] = 0;
            free(cp);
         }
         idlen = strlen(idstr)+1;
         if (outptr) memcpy(outptr, idstr, idlen);
         return idlen;
      }
   }
   return 0;
}

void setup_hardware(void) {
   // take address, saved by EFI host
   acpi_table = sto_dword(STOKEY_ACPIADDR);
   if (acpi_table) log_it(3, "ACPI table at %X\n", acpi_table);

   if (!sys_isavail(SFEA_CMODT)) log_it(3, "no cm\n"); else {
      u32t cmv = hlp_cmgetstate();
      if (!cmv) log_it(3, "failed to read cm\n"); else {
         if (cmv<CPUCLK_MAXFREQ) {
            cmv = 10000/CPUCLK_MAXFREQ*cmv;
            log_it(0, "CPU at %u.%2.2u%%!!!\n", cmv/100, cmv%100);
         }
      }
   }
   if (!cpu_phys_bits) init_baseinfo();
   // restore some CPU modes to safe values on exit
   sys_notifyevent(SECB_QSEXIT|SECB_GLOBAL, cm_restore);
}
