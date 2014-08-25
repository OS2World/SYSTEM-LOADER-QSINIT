//
// QSINIT "start" module
// CPU low level functions (msr, mtrr, etc)
//
#include "vio.h"
#include "stdlib.h"
#include "qsutil.h"
#include "cpudef.h"
#include "qsxcpt.h"
#include "qslog.h"
#include "qshm.h"
#include "internal.h"
#include "qsstor.h"
#include "qsint.h"
#include "qsinit.h"

#define MSR_IA32_MTRRCAP             0x00FE
#define MSR_IA32_EXT_CONFIG          0x00EE
#define MSR_IA32_TEMPERATURE_TARGET  0x01A2
#define MSR_IA32_SMRR_PHYSBASE       0x01F2
#define MSR_IA32_SMRR_PHYSMASK       0x01F3
#define MSR_IA32_MTRR_PHYSBASE0      0x0200
#define MSR_IA32_MTRR_PHYSMASK0      0x0201
#define MSR_IA32_MTRR_PHYSBASE1      0x0202
#define MSR_IA32_MTRR_PHYSMASK1      0x0203
#define MSR_IA32_MTRR_PHYSBASE2      0x0204
#define MSR_IA32_MTRR_PHYSMASK2      0x0205
#define MSR_IA32_MTRR_PHYSBASE3      0x0206
#define MSR_IA32_MTRR_PHYSMASK3      0x0207
#define MSR_IA32_MTRR_PHYSBASE4      0x0208
#define MSR_IA32_MTRR_PHYSMASK4      0x0209
#define MSR_IA32_MTRR_PHYSBASE5      0x020A
#define MSR_IA32_MTRR_PHYSMASK5      0x020B
#define MSR_IA32_MTRR_PHYSBASE6      0x020C
#define MSR_IA32_MTRR_PHYSMASK6      0x020D
#define MSR_IA32_MTRR_PHYSBASE7      0x020E
#define MSR_IA32_MTRR_PHYSMASK7      0x020F
#define MSR_IA32_MTRR_PHYSBASE8      0x0210
#define MSR_IA32_MTRR_PHYSMASK8      0x0211
#define MSR_IA32_MTRR_PHYSBASE9      0x0212
#define MSR_IA32_MTRR_PHYSMASK9      0x0213
#define MSR_IA32_MTRR_FIX64K_00000   0x0250
#define MSR_IA32_MTRR_FIX16K_80000   0x0258
#define MSR_IA32_MTRR_FIX16K_A0000   0x0259
#define MSR_IA32_MTRR_FIX4K_C0000    0x0268
#define MSR_IA32_MTRR_FIX4K_C8000    0x0269
#define MSR_IA32_MTRR_FIX4K_D0000    0x026A
#define MSR_IA32_MTRR_FIX4K_D8000    0x026B
#define MSR_IA32_MTRR_FIX4K_E0000    0x026C
#define MSR_IA32_MTRR_FIX4K_E8000    0x026D
#define MSR_IA32_MTRR_FIX4K_F0000    0x026E
#define MSR_IA32_MTRR_FIX4K_F8000    0x026F
#define MSR_IA32_MTRR_DEF_TYPE       0x02FF

#define FIXED_RANGE_REGS    11
#define MTRR_SAVE_BUFFER    32

/** disable interrupts, cache and turn off MTRRs.
    @param [out]     state  State to save - for hlp_mtrrmend() call */
void _std hlp_mtrrmstart(volatile u32t *state);
/** enable interrupts, cache and turn on MTRRs.
    @param [in][out] state  State, returned from hlp_mtrrmstart(),
                            clear it on exit to prevent second call */
void _std hlp_mtrrmend  (volatile u32t *state);

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


u32t _std hlp_getcputemp(void) {
   static int inited = 0;
   static u32t TMax  = 100;
   u32t data[4];

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
         // it hang on 1156 socket i5 at least ;)
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


static u32t mtrr_regs = 0,
           mtrr_flags = 0,
        cpu_phys_bits = 0;

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

   if (!inited) {
      inited = 1;
      // no cpuid
      if (!hlp_getcpuid(1,data)) return 0;
      if ((data[3]&CPUID_FI2_MTRR)==0) return 0;

      hlp_getmsrsafe(MSR_IA32_MTRRCAP,data+0,data+1);
      mtrr_regs  = data[0] & 0x7F;
      mtrr_flags = (data[0]&0x100?MTRRQ_FIXREGS:0) | (data[0]&0x400?MTRRQ_WRCOMB:0) |
         (data[0]&0x800?MTRRQ_SMRR:0);

      _try_ {
         hlp_getcpuid(0x80000008,data);
      }
      _catch_(xcpt_all) {
         data[0] = 36;
      }
      _endcatch_
      cpu_phys_bits = data[0] & 0x7F;
      if (cpu_phys_bits<36) cpu_phys_bits=36;
   }

   if (state) {
      hlp_getmsrsafe(MSR_IA32_MTRR_DEF_TYPE,data+0,data+1);
      *state = data[0];
   }
   if (flags) *flags = mtrr_flags;
   if (addrbits) *addrbits = cpu_phys_bits;
   return mtrr_regs;
}

/** read mtrr register.
    @param [in]  reg       variable range register index
    @param [out] start     start address
    @param [out] length    block length
    @param [out] state     block flags (see MTRRF_*)
    @return success flag (1/0) */
int  _std hlp_mtrrvread(u32t reg, u64t *start, u64t *length, u32t *state) {
   if (start)  *start  = 0;
   if (length) *length = 0;
   if (state)  *state  = 0;
   if (!mtrr_regs) hlp_mtrrquery(0,0,0);
   if (!mtrr_regs) return 0;
   if (reg>=mtrr_regs) return 0;

   if (reg<10) {
      u64t base, len;
      int  rc = 1;
      _try_ {
         hlp_readmsr(MSR_IA32_MTRR_PHYSBASE0+reg*2, (u32t*)&base, (u32t*)&base+1);
         hlp_readmsr(MSR_IA32_MTRR_PHYSMASK0+reg*2, (u32t*)&len , (u32t*)&len +1);
      }
      _catch_(xcpt_all) {
         log_it(2,"Exception in MTRR_PHYS_%d\n",reg);
         base=0; len=0; rc=0;
      }
      _endcatch_

      // log_it(2,"%u: %012LX %012LX\n", reg, base, len);

      if (state) *state = (u32t)base & MTRRF_TYPEMASK | ((u32t)(len)&0x800?MTRRF_ENABLED:0);
      base = base & ~0xFFF & ((u64t)1 << cpu_phys_bits) - 1;
      if (start) *start = base;
      if (length) {
         u64t mask = len & ~0xFFF & ((u64t)1 << cpu_phys_bits) - 1, ebase;
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
      return 1;
   }
   return 0;
}

/** read variable range mtrr register.
    Function merge neighboring fixed blocks with the same cache types
    and return list in readable format.
    @param [out] start     array of [MTRR_FIXEDMAX] start adresses
    @param [out] length    array of [MTRR_FIXEDMAX] block lengths
    @param [out] state     array of [MTRR_FIXEDMAX] cache types
    @return number of filled entries */
u32t _std hlp_mtrrfread(u32t *start, u32t *length, u32t *state) {
   if (!start || !length || !state)  return 0;
   if (!mtrr_regs) hlp_mtrrquery(0,0,0);
   if ((mtrr_flags&MTRRQ_FIXREGS)==0) return 0; else {
      u32t ii, idx, ctype = FFFF, mempos = 0, rc = 0;

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
      return rc;
   }
}

static u8t biosmtrrs[MTRR_SAVE_BUFFER*10];
static int resetdata = 0,
        mtrr_changes = 0;

u32t _std START_EXPORT(hlp_mtrrstate)(u32t state) {
   // incorrect mask
   if ((state&MTRRS_DEFMASK)>MTRRF_WB ||
      (state&~(MTRRS_DEFMASK|MTRRS_FIXON|MTRRS_MTRRON|MTRRS_NOCALBRT))!=0)
         return MTRRERR_STATE;
   if (!mtrr_regs) hlp_mtrrquery(0,0,0);
   if (!mtrr_regs) return MTRRERR_HARDW;
   // save original state
   if (!resetdata) {
      hlp_mtrrbatch(biosmtrrs);
      mtrr_changes |= 1;
      resetdata = 1;
   }
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
   if (!mtrr_regs) hlp_mtrrquery(0,0,0);
   if (!mtrr_regs) return MTRRERR_HARDW;
   if (reg>=mtrr_regs) return MTRRERR_INDEX;
   // save before first modification
   if (!resetdata) {
      hlp_mtrrbatch(biosmtrrs);
      mtrr_changes |= 4;
      resetdata = 1;
   }
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
      if ((start&0xFFF)!=0 || start>=(u64t)1<<cpu_phys_bits)
         return MTRRERR_ADDRESS;
      // incorrect state
      if ((state&MTRRF_TYPEMASK)>MTRRF_WB ||
         (state&~(MTRRF_ENABLED|MTRRF_TYPEMASK))!=0) return MTRRERR_STATE;
      state &= MTRRF_TYPEMASK;
      length = ~(length-1);
      length&= ((u64t)1 << cpu_phys_bits) - 1;

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
   if (!mtrr_regs) hlp_mtrrquery(0,0,0);
   if (!mtrr_regs) return MTRRERR_HARDW;
   if ((mtrr_flags&MTRRQ_FIXREGS)==0) return MTRRERR_HARDW;
   // save before first modification
   if (!resetdata) {
      hlp_mtrrbatch(biosmtrrs);
      mtrr_changes |= 2;
      resetdata = 1;
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
                  int ps=0, pe=7;
                  if (allow==1) ps=(addr-start)/step;
                  if (start+csize>addr+length) pe=(addr+length-start)/step;
                  while (ps<=pe)
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

void _std hlp_mtrrbios(void) {
   if (resetdata) {
      hlp_mtrrapply(biosmtrrs);
      tm_calibrate();
      mtrr_changes = 0;
      resetdata = 0;
      log_it(2,"hlp_mtrrbios() done\n");
      // delete memlimit from VMTRR
      sto_save(STOKEY_MEMLIM, 0, 0, 1);
   }
}

int _std hlp_mtrrchanged(int state, int fixed, int variable) {
   int rc = 0;
   if (state    && (mtrr_changes&1)!=0) rc = 1;
   if (fixed    && (mtrr_changes&2)!=0) rc = 1;
   if (variable && (mtrr_changes&4)!=0) rc = 1;
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
      return cnt;
   }
}

/** memcpy with SS segment as source & destination.
    I.e., function have access to 1st page in non-paging mode. */
void* _std memcpy0(void *dst, const void *src, u32t length);
// 48-bit ptr to page 0, but we use only offset here
extern u32t _std page0_fptr;

static void* hlp_memcpy_int(void *dst, const void *src, u32t length, int page0) {
   _try_ {
      /* page0 flag here affect SOURCE access in paging mode and 
         SOURCE+DST access in non-paging */
      if (page0) memcpy0(dst,src,length); else memcpy(dst,src,length);
      _ret_in_try_(dst);
   }
   _catch_(xcpt_all) {
   }
   _endcatch_
   return 0;
}

void* _std hlp_memcpy(void *dst, const void *src, u32t length, int page0) {
   if (page0 && in_pagemode) {
      u32t  dstv = (u32t)dst;
      char *srcv = (char*)src;
      // wrap around 0?
      if (dstv+length < dstv) {
         u32t upto0 = FFFF-dstv+1;
         void   *rc = hlp_memcpy_int(dst, srcv, upto0, 1);
         if (!rc) return 0;
         dst = 0; dstv = 0;
         srcv   += upto0;
         length -= upto0;
      }
      if (!length) return dst;
      // 1st page copying
      if (dstv < PAGESIZE) {
         u32t upto4 = dstv+length > PAGESIZE ? PAGESIZE-dstv : length;
         void   *rc = hlp_memcpy_int((void*)(page0_fptr+dstv), srcv, upto4, 1);
         if (!rc) return 0;
         dstv   += upto4; dst = (void*)dstv; 
         srcv   += upto4;
         length -= upto4;
      }
      if (!length) return dst;
      // above 1st page - normal copying
      return hlp_memcpy_int(dst, srcv, length, 1);
   } else
   return hlp_memcpy_int(dst, src, length, page0);
}
