//
// QSINIT
// multi-tasking implementation module
// --------------------------------------------------------
//
// Note, that any system calls in timer inturrupt handlers is prohibited!
// At least, any used call MUST be protected from TRACE (not included into it
// or called directly) - else damage is guaranteed!
//
#include "mtlib.h"
#include "qshm.h"

extern u32t   timer_cnt;
extern u32t   yield_cnt;

static u64t    last_tsc;
static u32t  lockcaller;              ///< last lock caller
static u64t   lockstart;              ///< first time last caller was catched

static int check_in_service(void) {
   u32t reg = apic_timer_int>>3 & 0x3C;

   return apic[APIC_ISR_TAB+reg] & 1<<(apic_timer_int&0x1F) ?1:0;
}

/** check for long locks from the same address.
    Serial port printing here is dangerous and mod_by_eip() call may cause
    trap, because we`re in the timer interrupt now and module tables are
    in unpredicted state. */
static void check_lcaller(void) {
   u64t now = hlp_tscread();
   if (lockcaller)
      if (lockcaller==mt_exechooks.mtcb_llcaller) {
         if (now-lockstart>>tsc_shift > 3000) {
            static char errstr[128];
            u32t   object, offset;
            module    *mi;
            // disable mt_yield call until the end of printing
            mt_exechooks.mtcb_yield = 0;

            snprintf(errstr, 128, "Too long lock from %08X", lockcaller);
            hlp_seroutstr(errstr);
            // this can make trap, so print it in separate step
            mi = mod_by_eip(lockcaller, &object, &offset, get_flatcs());
            if (mi) {
               snprintf(errstr, 128, "(\"%s\": %d:%08X)", mi->name, object+1, offset);
               hlp_seroutstr(errstr);
            }
            hlp_seroutstr("\n");
            // restore things, damaged by printing
            mt_exechooks.mtcb_yield    = &yield;
            mt_exechooks.mtcb_llcaller = lockcaller;

            lockstart = hlp_tscread();
         }
         return;
      }
   lockcaller = mt_exechooks.mtcb_llcaller;
   lockstart  = hlp_tscread();
}

/// 64-bit timer interrupt callback (EFI host)
void _std timer64cb(struct xcpt64_data *user) {
   struct desctab_s  sd;
   u32t       wrongmode = mt_exechooks.mtcb_glock;
   timer_cnt++;
   if (!wrongmode) {
      wrongmode = !sys_selquery(user->x64_cs, &sd);
      // we`re in 64-bit code? wrong place to switch thread
      if (!wrongmode)
         if (sd.d_attr&D_LONG) wrongmode = 1;
   }
   if (!mt_exechooks.mtcb_glock) lockcaller = 0; else check_lcaller();
   // EOI to APIC
   if (check_in_service()) apic[APIC_EOI] = 0;
   // a good time to switch thread
   if (!wrongmode) {
      // switch context, check wait objects, call attime and so on ...
      switch_context_timer(user, 1);
      // next check time for mt_yield()
      next_rdtsc = hlp_tscread() + tick_rdtsc;
   }
}

/// 32-bit timer interrupt (BIOS host) & mt_yield() callback (both hosts)
void _std timer32cb(struct tss_s *user) {
   u32t  wrongmode = mt_exechooks.mtcb_glock;
   /* if IF=0, then this mt_yield() callback - enable it on exit.
      IF=1 must be in real timer interrupt (else how it occurs?) */
   if ((user->tss_eflags&CPU_EFLAGS_IF)==0) {
      user->tss_eflags|=CPU_EFLAGS_IF;
      yield_cnt++;
   } else {
      timer_cnt++;
      if (mt_exechooks.mtcb_glock) check_lcaller();
      // EOI to APIC
      if (check_in_service()) apic[APIC_EOI] = 0;
   }
   // reset owner value both in timer & yield
   if (!mt_exechooks.mtcb_glock) lockcaller = 0;
   // is it main task?
   if (!wrongmode && main_tss) wrongmode = get_taskreg()!=main_tss;
   // a good time to switch thread
   if (!wrongmode) {
      switch_context_timer(user, 0);
      // next check time for mt_yield()
      next_rdtsc = hlp_tscread() + tick_rdtsc;
   }
}
