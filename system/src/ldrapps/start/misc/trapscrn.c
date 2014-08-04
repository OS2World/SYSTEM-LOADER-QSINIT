//
// QSINIT "start" module
// trap screen implementation
//
#include "stdlib.h"
#include "seldesc.h"
#include "vio.h"
#define MODULE_INTERNAL
#include "qsmod.h"
#include "qsxcpt.h"
#include "qslog.h"
#include "qsinit.h"
#include "qsutil.h"
#include "qsinit_ord.h"
#include "qssys.h"

#if ORD_QSINIT_vio_charout!=100
#error VIO ordinals was changed!
#endif

#define TRAP_STACK        6144   // stack size for traps 8, 12 & 13

extern module * _std  mod_list,  // loaded and loading module lists
              * _std mod_ilist;  //
extern u32t        xcpt_broken;  // "exception list was broken" flag. set by walk_xcpt.

struct tss_s     *tss_data = 0,  // tss struct
                    *tss08 = 0, 
                    *tss12 = 0,
                    *tss13 = 0;
u16t              main_tss = 0,  // tss selector
                trap08_tss = 0,
                trap12_tss = 0,
                trap13_tss = 0;
u32t             no_tgates = 0;

void      mod_resetchunk(u32t mh, u32t ordinal);
void _std sys_settr  (u16t sel);
void _std except_init(void);
void      traptask_8(void);
void      traptask_12(void);
void      traptask_13(void);


#define PRNBUF_SIZE   256
#define COMBUF_SIZE   512
static char buffer[PRNBUF_SIZE],
            combuf[COMBUF_SIZE];

static char *trapinfo1="EAX=%08X EBX=%08X ECX=%08X EDX=%08X DR6=%08X",
            *trapinfo2="ESI=%08X EDI=%08X EBP=%08X ESP=%08X DR7=%08X",
            *trapinfo3="DS=%04X ES=%04X SS=%04X GS=%04X FS=%04X TR=%04X Error Code: %04X",
            *trapinfo4="CS:EIP=%04X:%08X",
            *trapinfo5="Unhandled exception in file %s, line %d";
static char *trapname[19]={"Divide Error Fault", "Step Fault", "",
   "Breakpoint Trap", "Overflow Trap", "BOUND Range Exceeded Fault",
   "Invalid Opcode", "", "Double Fault Abort", "", "Invalid TSS Fault",
   "Segment Not Present Fault", "Stack-Segment Fault",
   "General Protection Fault", "Page Fault", "Exception stack broken",
   "FPU Floating-Point Error", "Alignment Check Fault", "Machine Check Abort"};

static char *softname[2]={"Bad _currentexception_", "Exit hook failed"};

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
   if (!cs) cs=SEL32CODE;
   for (ii=0;ii<2;ii++) {
      module *rc=ii?mod_ilist:mod_list;
      while (rc) {
         for (obj=0;obj<rc->objects;obj++) {
            int   big = rc->obj[obj].flags&OBJBIGDEF?1:0;
            u32t base = big ? (u32t)rc->obj[obj].address : 0;
            u16t  sel = rc->obj[obj].sel;
            // sel must be equal to SEL32CODE for all FLAT objects,
            if ((big && cs==SEL32CODE || cs==sel) && eip>=base &&
               eip-base<rc->obj[obj].size) 
            {
               if (object) *object=obj;
               if (offset) *offset=eip-base;
               return rc;
            }
         }
         rc=rc->next;
      }
   }
   return 0;
}

static void trap_out(const char *str) {
   vio_strout(str);
   strncat(combuf, str, COMBUF_SIZE);
   strncat(combuf, "\n", COMBUF_SIZE);
}

/** system trap screen.
    file & line will be filled only on catched trap */
void __stdcall trap_screen(struct tss_s *ti, const char *file, u32t line) {
   u32t   height = 10;
   char *exctext = "";
   u32t   qsinit = mod_query(MODNAME_QSINIT, MODQ_NOINCR);
   /* reset vio chunks to original state to stop trace calling and console.dll`s
      interseption */
   if (qsinit) {
      u32t ii;
      for (ii=100; ii<110; ii++) mod_resetchunk(qsinit,ii);
      for (ii=200; ii<210; ii++) mod_resetchunk(qsinit,ii);
      // it was funny without this call ;)
      mod_resetchunk(qsinit, ORD_QSINIT_snprintf);
   }
   // processing screen
   vio_resetmode();
   vio_clearscr();
   combuf[0] = '\n';
   combuf[1] = 0;
   if (xcpt_broken) height++;
   draw_border(1,2,78,height,0x4F);
   vio_setpos(3,3);
   // broken exception record fake trap screen
   if (ti->tss_reservdbl==0xFFFE) ti->tss_reservdbl=16;
   // print
   if (ti->tss_reservdbl<19) exctext=trapname[ti->tss_reservdbl]; else
   if (ti->tss_reservdbl>=xcpt_hookerr)
      exctext=softname[xcpt_invalid-ti->tss_reservdbl];
   snprintf(buffer, PRNBUF_SIZE, "Exception %d: %s\n", ti->tss_reservdbl,
      exctext);
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
      ti->tss_gs, ti->tss_fs, ti->tss_backlink, ti->tss_cr3);
   vio_setpos(7,3);
   trap_out(buffer);
   snprintf(buffer, PRNBUF_SIZE, trapinfo4, ti->tss_cs, ti->tss_eip);
   vio_setpos(8,3);
   trap_out(buffer);
   vio_setshape(0x20,0);
   if (file) {
      snprintf(buffer, PRNBUF_SIZE, trapinfo5, file, line);
      vio_setpos(9,3);
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

         vio_setpos(10,3);
         trap_out(buffer);
      }
   }
   if (xcpt_broken) {
      vio_setpos(11,3);
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
   }
}

void _std sys_exfunc2(volatile sys_xcpt* except) {
   sys_exfunc3(except);
   log_it(2,"Exception %d occured in file %s, line %d\n", except->reserved[1],
      (char*)except->reserved[2], except->reserved[3]);
}

/** allocate TSS selector.
    @param   tssdata       TSS data (must not cross page boundary, as Intel says)
    @param   limit         TSS limit (use 0 for default)
    @return  tss selector or 0 on error. */
u16t _std sys_tssalloc(void *tssdata, u16t limit) {
   struct desctab_s  sd;
   u16t sel = hlp_selalloc(2);
   u32t  pa = (u32t)tssdata - hlp_segtoflat(0);
   if (!sel) return 0;
   if (!limit) limit = sizeof(struct tss_s) - 1;
   
   sd.d_limit   = limit;
   sd.d_loaddr  = pa;
   sd.d_hiaddr  = pa>>16;
   sd.d_access  = D_TSS032;
   sd.d_attr    = 0;
   sd.d_extaddr = pa>>24;

   if (!sys_seldesc(sel,&sd) || !hlp_selsetup(sel+SEL_INCR, pa, limit, 
      QSEL_16BIT|QSEL_BYTES)) 
   {
      log_it(2,"tssalloc(%X,%u) -> %X %8b\n", tssdata, limit, pa, &sd);

      hlp_selfree(sel+SEL_INCR);
      hlp_selfree(sel);
      return 0;
   }
   return sel;
}

u16t get_task(void);
#pragma aux get_task = "str ax" value [ax];

/** free TSS selector.
    Function will fail if TSS is current task or linked to current task.
    @param   sel           TSS selector
    @return success flag (1/0) */
int _std sys_tssfree(u16t sel) {
   u32t  diff = hlp_segtoflat(0);
   int  svint = sys_intstate(0),
           rc = 0;
   u16t   ctr = get_task(), 
         wsel = ctr;
   /* walk over linked tasks ... (but who can create this list??? ;) */
   while (wsel!=sel) {
      struct tss_s *tss;
      u32t   base = hlp_selbase(wsel,0);
      // invalid selector? -> end of list
      if (base==FFFF) { rc = 1; break; }

      tss  = (struct tss_s*)(base - diff);
      wsel = tss->tss_backlink;
      // walked to the first entry again
      if (wsel==ctr) { rc = 1; break; }
   }
   sys_intstate(svint);
   if (rc) {
      u32t lim1, lim2;
      // selector for r/w access to TSS
      if (hlp_selbase(sel+SEL_INCR,&lim2) == hlp_selbase(sel,&lim1))
         if (lim1==lim2) hlp_selfree(sel+SEL_INCR);
      hlp_selfree(sel);
   }
   return rc;
}

void setup_exceptions(int stage) {
   if (stage==0) {
      except_init();
   } else 
   if (stage==1 && !no_tgates) {
      u32t step = Round256(sizeof(struct tss_s));
      // create tasks for common code and 8, 12, 13 exceptions
      tss_data = hlp_memalloc(step * 4 + TRAP_STACK * 4, QSMA_READONLY|QSMA_RETERR);
      if (tss_data) {
         tss08 = (struct tss_s *)((u8t*)tss_data + step);
         tss12 = (struct tss_s *)((u8t*)tss08 + step);
         tss13 = (struct tss_s *)((u8t*)tss12 + step);
         // this stack used in exception handling
         tss_data->tss_esp0  = (u32t)tss_data + step * 4 + TRAP_STACK;
         tss_data->tss_ss0   = SEL32DATA;
         tss_data->tss_iomap = sizeof(struct tss_s);
         // task stacks
         tss08->tss_esp   = tss_data->tss_esp0 + TRAP_STACK;
         tss08->tss_ss    = SEL32DATA;
         tss08->tss_eip   = (u32t)&traptask_8;
         tss08->tss_cs    = SEL32CODE;
         tss08->tss_esp2  = (u32t)tss08;
         tss08->tss_iomap = sizeof(struct tss_s);
      
         tss12->tss_esp   = tss08->tss_esp + TRAP_STACK;
         tss12->tss_ss    = SEL32DATA;
         tss12->tss_eip   = (u32t)&traptask_12;
         tss12->tss_cs    = SEL32CODE;
         tss12->tss_esp2  = (u32t)tss12;
         tss12->tss_iomap = sizeof(struct tss_s);
      
         tss13->tss_esp   = tss12->tss_esp + TRAP_STACK;
         tss13->tss_ss    = SEL32DATA;
         tss13->tss_eip   = (u32t)&traptask_13;
         tss13->tss_cs    = SEL32CODE;
         tss13->tss_esp2  = (u32t)tss13;
         tss13->tss_iomap = sizeof(struct tss_s);
      
         main_tss   = sys_tssalloc(tss_data, 0);
         trap08_tss = sys_tssalloc(tss08, 0);
         trap12_tss = sys_tssalloc(tss12, 0);
         trap13_tss = sys_tssalloc(tss13, 0);
      
         if (trap08_tss) sys_intgate( 8, trap08_tss);
         if (trap12_tss) sys_intgate(12, trap12_tss);
         if (trap13_tss) sys_intgate(13, trap13_tss);
         
         if (main_tss) sys_settr(main_tss);
      
         if (trap08_tss && trap12_tss && trap13_tss && main_tss)
            log_it(2,"task gates on\n");
      }
   }
}

// update cr3 in tss segments, called just before page mode on 
void sys_tsscr3(u32t cr3) {
   if (tss_data) tss_data->tss_cr3 = cr3;
   if (tss08) tss08->tss_cr3 = cr3;
   if (tss12) tss12->tss_cr3 = cr3;
   if (tss13) tss13->tss_cr3 = cr3;
}
