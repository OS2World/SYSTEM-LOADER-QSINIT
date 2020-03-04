//
// QSINIT "start" module
// trap screen implementation
//
#include "seldesc.h"
#include "vio.h"
#include "qsint.h"
#include "qsxcpt.h"
#include "qslog.h"
#include "qsinit.h"
#include "qsutil.h"
#include "qsinit_ord.h"
#include "syslocal.h"
#include "qsvdata.h"
#include "qssys.h"
#include "efnlist.h"
#include "cpudef.h"
#include "qspdata.h"

#if ORD_QSINIT_vio_charout!=100
#error VIO ordinals was changed!
#endif

extern module * _std  mod_list,  // loaded and loading module lists
              * _std mod_ilist;  //
extern u32t        xcpt_broken;  // "exception list was broken" flag. set by walk_xcpt.
extern
mt_proc_cb _std   mt_exechooks;  // this thing is import from QSINIT module
u32t                 reipltime = 0;

void      mod_resetchunk(u32t mh, u32t ordinal);

#define PRNBUF_SIZE   256
#define COMBUF_SIZE   512
static char buffer[PRNBUF_SIZE],
            combuf[COMBUF_SIZE];

static char *trapinfo1 = "EAX=%08X EBX=%08X ECX=%08X EDX=%08X DR6=%08X",
            *trapinfo2 = "ESI=%08X EDI=%08X EBP=%08X ESP=%08X DR7=%08X",
            *trapinfo3 = "DS=%04X ES=%04X SS=%04X GS=%04X FS=%04X TR=%04X Error Code: %04X",
            *trapinfo3a= "DS=%04X ES=%04X SS=%04X GS=%04X FS=%04X TR=%04X",
            *trapinfo4 = "CS:EIP=%04X:%08X      PID=%03u  TID=%03u.%02u  %s",
            *trapinfo5 = "Unhandled exception in file %s, line %d",
            *trapinfoA = "RAX=%016LX  RBX=%016LX  RCX=%016LX",
            *trapinfoB = "RDX=%016LX  RSI=%016LX  RDI=%016LX",
            *trapinfoC = "R8 =%016LX  R9 =%016LX  R10=%016LX",
            *trapinfoD = "R11=%016LX  R12=%016LX  R13=%016LX",
            *trapinfoE = "R14=%016LX  R15=%016LX  RBP=%016LX",
            *trapinfoF = "DR6=%08X DR7=%08X ",
            *trapinfoG = "CR0=%016LX  CR2=%016LX  CR3=%016LX",
            *trapinfoH = "RIP=%016LX  RSP=%016LX     PID=%03u  TID=%03u.%02u",
            *trapinfoI = "Check failure for module \"%s\", PID %d",
            *trapinfoJ = "CR4=%016LX  GS.=%016LX  FS.=%016LX";

static char *trapname[20] = {"Divide Error Fault", "Step Fault", "",
   "Breakpoint Trap", "Overflow Trap", "BOUND Range Exceeded Fault",
   "Invalid Opcode", "", "Double Fault Abort", "", "Invalid TSS Fault",
   "Segment Not Present Fault", "Stack-Segment Fault",
   "General Protection Fault", "Page Fault", "", "FPU Floating-Point Error",
   "Alignment Check Fault", "Machine Check Abort", "SIMD Floating-Point Error"};

static char *softname[7] = {"Exception stack broken", "Exit hook failed",
   "exit() in system module", "Shared class list damaged",
   "Process data structures damaged", "Internal data is broken",
   "System error" };

void __stdcall vio_drawborder(u32t x,u32t y,u32t dx,u32t dy,u32t color) {
   u32t ii,jj;
   if (dx<=2 || dy<=2) return;
   vio_setcolor(color);
   for (ii=0;ii<2;ii++) {
      u32t ypos=ii?y+dy-1:y;
      vio_setpos(ypos,x);
      vio_charout(ii?'È':'É');
      for (jj=0;jj<dx-2;jj++) vio_charout('Í');
      vio_charout(ii?'¼':'»');
   }
   for (ii=1;ii<dy-1;ii++) {
      vio_setpos(y+ii,x); vio_charout('º');
      for (jj=0;jj<dx-2;jj++) vio_charout(' ');
      vio_charout('º');
   }
}

// search module by flat eip. return 0-based object!
module *_std mod_by_eip(u32t eip,u32t *object,u32t *offset,u16t cs) {
   int ii, obj;
   if (object) *object=0;
   if (offset) *offset=0;
   if (!cs) cs = get_flatcs();
   /* note, that this function is called by the timer interrupt, when
      it prints "too long lock" message. This is a disaster, because it
      just ignores active lock (and inc/dec it here!), so any additional
      processing should not bother us here */
   mt_swlock();
   for (ii=0; ii<2; ii++) {
      module *rc = ii?mod_ilist:mod_list;
      while (rc) {
         for (obj=0;obj<rc->objects;obj++) {
            int   big = rc->obj[obj].flags&OBJBIGDEF?1:0;
            u32t base = big ? (u32t)rc->obj[obj].address : 0;
            u16t  sel = rc->obj[obj].sel;
            // sel must be equal to SEL32CODE for all FLAT objects,
            if ((big && cs==get_flatcs() || cs==sel) && eip>=base &&
               eip-base<rc->obj[obj].size)
            {
               if (object) *object=obj;
               if (offset) *offset=eip-base;
               mt_swunlock();
               return rc;
            }
         }
         rc=rc->next;
      }
   }
   mt_swunlock();
   return 0;
}

static void trap_out(const char *str) {
   vio_strout(str);
   strncat(combuf, str, COMBUF_SIZE);
   strncat(combuf, "\n", COMBUF_SIZE);
}

static void reset_video(void) {
   u32t ii;
   // reset vio chunks to original state to stop trace
   for (ii=100; ii<110; ii++) mod_resetchunk(mh_qsinit,ii);
   for (ii=200; ii<210; ii++) mod_resetchunk(mh_qsinit,ii);
   // trap screen tracing is very funny without this call ;)
   mod_resetchunk(mh_qsinit, ORD_QSINIT_snprintf);
   // reset handler to simple vio calls (drops session management)
   VHTable[VHI_ACTIVE] = VHTable[VHI_CVIO];
   // processing screen
   vio_resetmode();
   vio_setcolor(VIO_COLOR_RESET);
   vio_clearscr();
   vio_setshape(VIO_SHAPE_NONE);
}

static void stop_timer(void) {
   u32t *apic = (u32t*)sys_getlapic();
   if (apic) apic[APIC_LVT_TMR] = APIC_DISABLE;
}

void trap_screen_prepare(void) {
   stop_timer();
   reset_video();
}

static const char *get_trap_name(struct tss_s *ti) {
   if (ti->tss_reservdbl<20) return trapname[ti->tss_reservdbl]; else
   if (ti->tss_reservdbl>=xcpt_syserr)
      return softname[xcpt_invalid-ti->tss_reservdbl];
   return "";
}

static void get_flags_str(char *str, u32t flags) {
   static char *fstr[16] = {" C+",0," P+",0," A+",0," Z+"," S+"," T+"," I+"," D+"," O+",0,0," NT",0};
   u32t ii;
   snprintf(str, PRNBUF_SIZE, "Flags=%08X", flags);
   for (ii=0; ii<16; ii++)
      if (fstr[ii] && (flags&1<<ii)) strcat(str,fstr[ii]);
}

/** additional dump to serial port.
    @param  ti      registers
    @param  level   make copy to log if value>=0, -1 to serial port printing only */
static void dump_more(struct tss_s *ti, int level) {
   u32t   pass;
   // FPU usage
   if (mt_exechooks.mtcb_cfpt) {
      mt_thrdata *owner = mt_exechooks.mtcb_cfpt;
      snprintf(combuf, COMBUF_SIZE, "FPU owner pid %u tid %u, fib %u (%u)\n",
         owner->tiPID, owner->tiTID, mt_exechooks.mtcb_cfpf, owner->tiState);
      hlp_seroutstr(combuf);
      if (level>=0) log_push(level, combuf);
   }
   // stack printing removed to separate code below
   for (pass=0; pass<1 /*2*/; pass++) {
      u8t  buffer[28];
      char    fmt[20];
      u32t   blen = 28,
              len = hlp_copytoflat(buffer, pass?ti->tss_esp:ti->tss_eip,
                                   pass?ti->tss_ss:ti->tss_cs, blen);
      if (pass) { len>>=2; blen>>=2; }
      snprintf(combuf, COMBUF_SIZE, pass?"SS:ESP ":"CS:EIP ");
      if (len) {
         snprintf(fmt, 20, pass?"%%%ulb":"%%%ub", len);
         snprintf(combuf+7, COMBUF_SIZE-7, fmt, buffer);
      }
      while (len++<blen) strncat(combuf, "?? ", COMBUF_SIZE);
      strncat(combuf, "\n", COMBUF_SIZE);
      hlp_seroutstr(combuf);
      if (level>=0) log_push(level, combuf);
   }
   if (ti->tss_esp) {
      u32t  buffer[8];
      u32t   blen = 8, ii, pos,
              len = hlp_copytoflat(buffer, ti->tss_esp, ti->tss_ss, blen<<2);
      len>>=2;
      pos = snprintf(combuf, COMBUF_SIZE, "SS:ESP");
      for (ii=0; ii<len; ii++) {
         pos += snprintf(combuf+pos, COMBUF_SIZE-pos, " %08X", buffer[ii]);
         if (buffer[ii]) {
            u32t object, offset;
            module  *mi = mod_by_eip(buffer[ii], &object, &offset, ti->tss_cs);
            if (mi)
               pos += snprintf(combuf+pos, COMBUF_SIZE-pos, "(\"%s\": %d:%08X)",
                  mi->name, object+1, offset);
         }
      }
      while (len++<blen) strncat(combuf, " ??", COMBUF_SIZE);
      strncat(combuf, "\n", COMBUF_SIZE);
      hlp_seroutstr(combuf);
      if (level>=0) log_push(level, combuf);
   }
   /// walk over ebp frames
   if (ti->tss_ebp) {
      u32t ebpv = ti->tss_ebp;
      for (pass=0; pass<10 && ebpv; pass++) {
         u32t  vbuf[2], object, offset,
                len = hlp_copytoflat(&vbuf, ebpv, ti->tss_ss, 8);
         module *mi;
         if (len<8) break;
         mi = mod_by_eip(vbuf[1], &object, &offset, ti->tss_cs);

         snprintf(combuf, COMBUF_SIZE, pass?"           ":"Call stack:");

         if (mi)
            snprintf(combuf+11, COMBUF_SIZE-11, " %08X  \"%s\": %d:%08X\n",
               vbuf[1], mi->name, object+1, offset);
         else
            snprintf(combuf+11, COMBUF_SIZE-11, " %08X\n", vbuf[1]);
         hlp_seroutstr(combuf);
         if (level>=0) log_push(level, combuf);
         ebpv = vbuf[0];
      }
   }
}

static u32t getfiber(void) {
   mt_thrdata *th = mt_exechooks.mtcb_cth;
   if ((u32t)th>=0x1000 && th->tiSign==THREADINFO_SIGN) return th->tiFiberIndex;
   return 99;
}

/** system trap screen.
    file & line will be filled only on catched trap */
void __stdcall trap_screen(struct tss_s *ti, const char *file, u32t line) {
   char   th_rem[17];
   u32t   height = 11;

   trap_screen_prepare();

   combuf[0] = '\n';
   combuf[1] = 0;

   if (xcpt_broken) height++;
   vio_drawborder(1,2,78,height,0x4F);
   vio_setpos(3,3);
   snprintf(buffer, PRNBUF_SIZE, "Exception %d: %s\n", ti->tss_reservdbl,
      get_trap_name(ti));
   trap_out(buffer);
   // printing registers
   snprintf(buffer, PRNBUF_SIZE, trapinfo1, ti->tss_eax, ti->tss_ebx,
      ti->tss_ecx, ti->tss_edx, ti->tss_esp1);
   vio_setpos(5,3);
   trap_out(buffer);
   snprintf(buffer, PRNBUF_SIZE, trapinfo2, ti->tss_esi, ti->tss_edi,
      ti->tss_ebp, ti->tss_esp, ti->tss_esp2);
   vio_setpos(6,3);
   trap_out(buffer);
   snprintf(buffer, PRNBUF_SIZE, trapinfo3, ti->tss_ds, ti->tss_es, ti->tss_ss,
      ti->tss_gs, ti->tss_fs, ti->tss_ss2, ti->tss_cr3);
   vio_setpos(7,3);
   trap_out(buffer);
   get_flags_str(buffer, ti->tss_eflags);
   vio_setpos(8,3);
   trap_out(buffer);
   memcpy(th_rem, mt_getthreadname(), 16); th_rem[16] = 0;
   snprintf(buffer, PRNBUF_SIZE, trapinfo4, ti->tss_cs, ti->tss_eip,
      mt_exechooks.mtcb_cth->tiPID, mt_exechooks.mtcb_cth->tiTID,
         ti->tss_reservdbl==8?99:getfiber(), th_rem);
   vio_setpos(9,3);
   trap_out(buffer);
   vio_setshape(VIO_SHAPE_NONE);
   if (file) {
      if (ti->tss_reservdbl==xcpt_syserr) snprintf(buffer, PRNBUF_SIZE, file, line);
         else snprintf(buffer, PRNBUF_SIZE, ti->tss_reservdbl==xcpt_prcerr?
            trapinfoI:trapinfo5,file, line);
      vio_setpos(10,3);
      trap_out(buffer);
   }
   // query point location in module
   if (ti->tss_cs) {
      u32t object,offset;
      module *mi = mod_by_eip(ti->tss_eip, &object, &offset, ti->tss_cs);
      if (mi) {
         snprintf(buffer, PRNBUF_SIZE, mi->obj[object].flags&OBJBIGDEF ?
            "Module \"%s\": %d:%08X\n" : "Module \"%s\": %d:%04X\n",
               mi->name, object+1, offset);

         vio_setpos(11,3);
         trap_out(buffer);
      }
   }
   if (xcpt_broken) {
      vio_setpos(12,3);
      trap_out("Exception stack is broken.");
   }
   // copy dump to serial port
   if (hlp_seroutinfo(0)) {
      hlp_seroutstr(combuf);
      dump_more(ti,-1);
   }
}

/** system trap screen (trap in EFI/64 bit code). */
void __stdcall trap_screen_64(struct tss_s *ti, struct xcpt64_data *xd) {
   u32t   height = 16, b_gs[2], b_fs[2];
   char *exctext = "";

   trap_screen_prepare();

   combuf[0] = '\n';
   combuf[1] = 0;
   if (xcpt_broken) height++;
   vio_drawborder(1,2,78,height,0x4F);
   vio_setpos(3,3);
   snprintf(buffer, PRNBUF_SIZE, "Exception %d: %s\n", ti->tss_reservdbl,
      get_trap_name(ti));
   trap_out(buffer);

   // printing registers
   snprintf(buffer, PRNBUF_SIZE, trapinfoA, xd->x64_rax, xd->x64_rbx, xd->x64_rcx);
   vio_setpos(5,3);
   trap_out(buffer);
   snprintf(buffer, PRNBUF_SIZE, trapinfoB, xd->x64_rdx, xd->x64_rsi, xd->x64_rdi);
   vio_setpos(6,3);
   trap_out(buffer);
   snprintf(buffer, PRNBUF_SIZE, trapinfoC, xd->x64_r8, xd->x64_r9, xd->x64_r10);
   vio_setpos(7,3);
   trap_out(buffer);
   snprintf(buffer, PRNBUF_SIZE, trapinfoD, xd->x64_r11, xd->x64_r12, xd->x64_r13);
   vio_setpos(8,3);
   trap_out(buffer);
   snprintf(buffer, PRNBUF_SIZE, trapinfoE, xd->x64_r14, xd->x64_r15, xd->x64_rbp);
   vio_setpos(9,3);
   trap_out(buffer);
   snprintf(buffer, PRNBUF_SIZE, trapinfoG, xd->x64_cr0, xd->x64_cr2, xd->x64_cr3);
   vio_setpos(10,3);
   trap_out(buffer);

   hlp_readmsr(MSR_IA32_FS_BASE, b_fs+0, b_fs+1);
   hlp_readmsr(MSR_IA32_GS_BASE, b_gs+0, b_gs+1);

   snprintf(buffer, PRNBUF_SIZE, trapinfoJ, xd->x64_cr4, b_gs[0], b_gs[1], b_fs[0], b_fs[1]);
   vio_setpos(11,3);
   trap_out(buffer);
   snprintf(buffer, PRNBUF_SIZE, trapinfo3, ti->tss_ds, ti->tss_es, ti->tss_ss,
      ti->tss_gs, ti->tss_fs, ti->tss_ss2, ti->tss_cr3);
   vio_setpos(13,3);
   trap_out(buffer);
   snprintf(buffer, PRNBUF_SIZE, trapinfoF, ti->tss_esp1, ti->tss_esp2);
   get_flags_str(buffer+strlen(buffer), ti->tss_eflags);
   vio_setpos(14,3);
   trap_out(buffer);
   snprintf(buffer, PRNBUF_SIZE, trapinfoH, xd->x64_rip, xd->x64_rsp,
      mt_exechooks.mtcb_cth->tiPID, mt_exechooks.mtcb_cth->tiTID,
         ti->tss_reservdbl==8?99:getfiber());
   vio_setpos(16,3);
   trap_out(buffer);

   vio_setshape(VIO_SHAPE_NONE);

   if (xcpt_broken) {
      vio_setpos(17,3);
      trap_out("Exception stack is broken.");
   }
   // copy dump to serial port
   if (hlp_seroutinfo(0)) {
      u32t   pass;
      hlp_seroutstr(combuf);
      for (pass=0; pass<2; pass++) {
         u8t  buffer[32];
         u32t   rdok = sys_memhicopy((u32t)& buffer, pass?xd->x64_rsp:
                                     xd->x64_rip, 32);
         snprintf(combuf, COMBUF_SIZE, pass?"RSP ":"RIP ");
         if (rdok)
            snprintf(combuf+4, COMBUF_SIZE-4, pass?"%8lb":"%28b", buffer);
         else
            strcpy(combuf+4, "access error");
         strncat(combuf, "\n", COMBUF_SIZE);
         hlp_seroutstr(combuf);
      }
   }
}

void __stdcall log_dumpregs_int(struct tss_s *ti, int level) {
   static char th_rem[17];
   /* MT lock here because we using static buffer.
      And we using static buffer because stack size & state is unknown as well
      as heap manager stability at the time of this DEBUG call */
   mt_swlock();
   // printing registers
   snprintf(buffer, PRNBUF_SIZE, trapinfo1, ti->tss_eax, ti->tss_ebx,
      ti->tss_ecx, ti->tss_edx, ti->tss_esp1);
   strncat(buffer, "\n", PRNBUF_SIZE);
   hlp_seroutstr(buffer); if (level>=0) log_push(level, buffer);

   snprintf(buffer, PRNBUF_SIZE, trapinfo2, ti->tss_esi, ti->tss_edi,
      ti->tss_ebp, ti->tss_esp, ti->tss_esp2);
   strncat(buffer, "\n", PRNBUF_SIZE);
   hlp_seroutstr(buffer); if (level>=0) log_push(level, buffer);

   snprintf(buffer, PRNBUF_SIZE, trapinfo3a, ti->tss_ds, ti->tss_es, ti->tss_ss,
      ti->tss_gs, ti->tss_fs, ti->tss_ss2);
   strncat(buffer, "\n", PRNBUF_SIZE);
   hlp_seroutstr(buffer); if (level>=0) log_push(level, buffer);

   get_flags_str(buffer, ti->tss_eflags);
   strncat(buffer, "\n", PRNBUF_SIZE);
   hlp_seroutstr(buffer); if (level>=0) log_push(level, buffer);

   memcpy(th_rem, mt_getthreadname(), 16); th_rem[16] = 0;
   snprintf(buffer, PRNBUF_SIZE, trapinfo4, ti->tss_cs, ti->tss_eip,
      mt_exechooks.mtcb_cth->tiPID, mt_exechooks.mtcb_cth->tiTID,
         mt_exechooks.mtcb_cth->tiFiberIndex, th_rem);

   strncat(buffer, "\n", PRNBUF_SIZE);
   hlp_seroutstr(buffer); if (level>=0) log_push(level, buffer);

   // query point location in module
   if (ti->tss_cs) {
      u32t object,offset;
      module *mi = mod_by_eip(ti->tss_eip, &object, &offset, ti->tss_cs);
      if (mi) {
         snprintf(buffer, PRNBUF_SIZE, mi->obj[object].flags&OBJBIGDEF ?
            "Module \"%s\": %d:%08X\n" : "Module \"%s\": %d:%04X\n",
               mi->name, object+1, offset);
         hlp_seroutstr(buffer); if (level>=0) log_push(level, buffer);
      }
   }
   dump_more(ti, level);
   mt_swunlock();
}

void _std reipl(int errcode) {
   sys_intstate(1);
   if (hlp_hosttype()==QSHT_EFI) {
      if (reipltime) key_wait(reipltime); else key_read();
      exit_pm32(errcode);
   } else {
      if (reipltime) {
         key_wait(reipltime); 
         exit_reboot(0);
      } else
      // loop forewer
      while (1) { sys_intstate(1); cpuhlt(); }
   }
}
