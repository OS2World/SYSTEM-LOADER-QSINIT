//
// QSINIT "start" module
// trap screen implementation
//
#include "stdlib.h"
#include "seldesc.h"
#include "vio.h"
#include "qsmodext.h"
#include "qsint.h"
#include "qsxcpt.h"
#include "qslog.h"
#include "qsinit.h"
#include "qsutil.h"
#include "qsinit_ord.h"
#include "internal.h"
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


void      mod_resetchunk(u32t mh, u32t ordinal);

#define PRNBUF_SIZE   256
#define COMBUF_SIZE   512
static char buffer[PRNBUF_SIZE],
            combuf[COMBUF_SIZE];

static char *trapinfo1="EAX=%08X EBX=%08X ECX=%08X EDX=%08X DR6=%08X",
            *trapinfo2="ESI=%08X EDI=%08X EBP=%08X ESP=%08X DR7=%08X",
            *trapinfo3="DS=%04X ES=%04X SS=%04X GS=%04X FS=%04X TR=%04X Error Code: %04X",
            *trapinfo4="CS:EIP=%04X:%08X      PID=%04d  TID=%04d    %s",
            *trapinfo5="Unhandled exception in file %s, line %d",
            *trapinfoA="RAX=%016LX  RBX=%016LX  RCX=%016LX",
            *trapinfoB="RDX=%016LX  RSI=%016LX  RDI=%016LX",
            *trapinfoC="R8 =%016LX  R9 =%016LX  R10=%016LX",
            *trapinfoD="R11=%016LX  R12=%016LX  R13=%016LX",
            *trapinfoE="R14=%016LX  R15=%016LX  RBP=%016LX",
            *trapinfoF="DR6=%08X DR7=%08X ",
            *trapinfoG="CR3=%016LX  GS.=%016LX  FS.=%016LX",
            *trapinfoH="RIP=%016LX  RSP=%016LX     PID=%04d  TID=%04d",
            *trapinfoI="Check failure for module \"%s\", PID %d";

static char *trapname[19]={"Divide Error Fault", "Step Fault", "",
   "Breakpoint Trap", "Overflow Trap", "BOUND Range Exceeded Fault",
   "Invalid Opcode", "", "Double Fault Abort", "", "Invalid TSS Fault",
   "Segment Not Present Fault", "Stack-Segment Fault",
   "General Protection Fault", "Page Fault", "", "FPU Floating-Point Error",
   "Alignment Check Fault", "Machine Check Abort"};

static char *softname[5]={"Exception stack broken", "Exit hook failed",
   "Unsupported exit() in dll module", "Shared class list damaged",
   "Process data structures damaged"};

void draw_border(u32t x,u32t y,u32t dx,u32t dy,u32t color) {
   u32t ii,jj;
   if (dx<=2||dy<=2||x>=78||y>=48) return;
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
   u32t  qsinit = mod_query(MODNAME_QSINIT, MODQ_NOINCR);
   /* reset vio chunks to original state to stop trace calling and console.dll`s
      interseption */
   if (qsinit) {
      u32t ii;
      for (ii=100; ii<110; ii++) mod_resetchunk(qsinit,ii);
      for (ii=200; ii<210; ii++) mod_resetchunk(qsinit,ii);
      // trap screen tracing was funny without this call ;)
      mod_resetchunk(qsinit, ORD_QSINIT_snprintf);
   }
   // processing screen
   vio_resetmode();
   vio_clearscr();
}

static void stop_timer(void) {
   u32t *apic = (u32t*)sys_getlapic();
   if (apic) apic[APIC_LVT_TMR] = APIC_DISABLE;
}

static const char *get_trap_name(struct tss_s *ti) {
   if (ti->tss_reservdbl<19) return trapname[ti->tss_reservdbl]; else
   if (ti->tss_reservdbl>=xcpt_prcerr)
      return softname[xcpt_invalid-ti->tss_reservdbl];
   return "";
}

static const char *get_fnline_fmt(struct tss_s *ti) {
   return ti->tss_reservdbl==xcpt_prcerr?trapinfoI:trapinfo5;
}

static void get_flags_str(char *str, u32t flags) {
   static char *fstr[16] = {" C+",0," P+",0," A+",0," Z+"," S+"," T+"," I+"," D+"," O+",0,0," NT",0};
   u32t ii;
   snprintf(str, PRNBUF_SIZE, "Flags=%08X", flags);
   for (ii=0; ii<16; ii++)
      if (fstr[ii] && (flags&1<<ii)) strcat(str,fstr[ii]);
}

/** system trap screen.
    file & line will be filled only on catched trap */
void __stdcall trap_screen(struct tss_s *ti, const char *file, u32t line) {
   char   th_rem[17];
   u32t   height = 11;

   stop_timer();
   reset_video();

   combuf[0] = '\n';
   combuf[1] = 0;

   if (xcpt_broken) height++;
   draw_border(1,2,78,height,0x4F);
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
      mt_exechooks.mtcb_ctxmem->pid, mt_exechooks.mtcb_ctid, th_rem);
   vio_setpos(9,3);
   trap_out(buffer);
   vio_setshape(0x20,0);
   if (file) {
      snprintf(buffer, PRNBUF_SIZE, get_fnline_fmt(ti), file, line);
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
      u32t   pass;
      hlp_seroutstr(combuf);
      for (pass=0; pass<2; pass++) {
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
            ebpv = vbuf[0];
         }
      }
   }
}

/** system trap screen. */
void __stdcall trap_screen_64(struct tss_s *ti, struct xcpt64_data *xd) {
   u32t   height = 14, b_gs[2], b_fs[2];
   char *exctext = "";

   stop_timer();
   reset_video();

   combuf[0] = '\n';
   combuf[1] = 0;
   if (xcpt_broken) height++;
   draw_border(1,2,78,height,0x4F);
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

   hlp_readmsr(MSR_IA32_FS_BASE, b_fs+0, b_fs+1);
   hlp_readmsr(MSR_IA32_GS_BASE, b_gs+0, b_gs+1);

   snprintf(buffer, PRNBUF_SIZE, trapinfoG, xd->x64_cr3, b_gs[0], b_gs[1], b_fs[0], b_fs[1]);
   vio_setpos(10,3);
   trap_out(buffer);
   snprintf(buffer, PRNBUF_SIZE, trapinfo3, ti->tss_ds, ti->tss_es, ti->tss_ss,
      ti->tss_gs, ti->tss_fs, ti->tss_ss2, ti->tss_cr3);
   vio_setpos(11,3);
   trap_out(buffer);
   snprintf(buffer, PRNBUF_SIZE, trapinfoF, ti->tss_esp1, ti->tss_esp2);
   get_flags_str(buffer+strlen(buffer), ti->tss_eflags);
   vio_setpos(12,3);
   trap_out(buffer);
   snprintf(buffer, PRNBUF_SIZE, trapinfoH, xd->x64_rip, xd->x64_rsp,
      mt_exechooks.mtcb_ctxmem->pid, mt_exechooks.mtcb_ctid);
   vio_setpos(14,3);
   trap_out(buffer);

   vio_setshape(0x20,0);

   if (xcpt_broken) {
      vio_setpos(15,3);
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
