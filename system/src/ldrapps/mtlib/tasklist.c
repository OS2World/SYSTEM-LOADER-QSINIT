//
// QSINIT
// task list control code
// ------------------------------------------------------------
//
#include "mtlib.h"
#include "vioext.h"
#include "qcl/qsextdlg.h"
#include "qcl/sys/qsvioint.h"          // only for VHI_SLOW
#include "signal.h"

static qs_extcmd    extdlg = 0;
mt_thrdata*      tasklists[SYS_HANDLERS]; ///< task list threads for all devices
static u8t        slow_dev[SYS_HANDLERS]; ///< "slow device" flag
static u32t       n_active = 0;
static u32t     tls_period = 0;   ///< tls var for alarm() interval
u32t           pid_changes = 0;   // ugly hack - pid add/del funcs ++ it

// just a fake code to signal about session list changes
#define KEYC_UPDATE        (KEYC_CTRLBREAK-2)
// pid changes checking period - in seconds
#define PID_CHECK_PERIOD   (3)
// same interval for slow devices
#define PID_CHECK_SLOW     (5)

static void push_update(void) {
   u32t ii;
   for (ii=0; ii<SYS_HANDLERS; ii++)
      if (tasklists[ii]) se_keypush(tasklists[ii]->tiSession, KEYC_UPDATE, 0);
   pid_changes = 0;
}

static void _std update_now  (sys_eventinfo *info) { push_update(); }
static void _std update_later(sys_eventinfo *info) { pid_changes++; }

static void _std process_watcher(int signo) {
   if (pid_changes) push_update();
   // we have a thread for every device, so tls is nice storage for timeout
   alarm(mt_tlsget(tls_period));
}

#define C_KEY "\x1B[0;35;44m"
#define C_TXT "\x1B[1;37;44m"

static u32t _std tasklist_thread(void *arg) {
   char sesname[32];
   u32t  device = (u32t)arg, selno, mx = 0, my = 0, modeid = 0, ii;
   if (device) snprintf(sesname, 32, "Task list (device %u)", device); else
      strcpy(sesname, "Task list");
   // se_foreground() use no lock, so call it instead of common method
   selno = device ? se_active(device) : se_foreground();
   // try to use *current* screen mode on this device!
   if (selno) {
      se_stats *si = se_stat(selno);
      if (si->handlers)
         for (ii=0; ii<si->handlers; ii++)
            if (si->mhi[ii].dev_id==device) {
               modeid = si->mhi[ii].reserved;
               mx     = si->mx;
               my     = si->my;
               break;
            }
      if (si) free(si);
   }
   // first time TLS slot allocation
   mt_swlock();
   if (!tls_period) tls_period = mt_tlsalloc();
   mt_swunlock();
   // switch thread into the background session on caller device
   if (se_newsession(1<<device, 0, sesname, 0)==0) {
      u16t color = VIO_COLOR_BLUE<<4|VIO_COLOR_WHITE;
      if (modeid) vio_setmodeid(device<<16|modeid); else
         if (mx && my) vio_setmodeex(mx, my);
      vio_getmode(&mx, &my);
      vio_fillrect(0, 0, mx, my, 0xB0, color);
      vio_fillrect(0, my-1, mx, 1, ' ' /*0xDB*/, color);
      // force ANSI ON for this thread
      mt_tlsset(QTLS_FORCEANSI, 2);

      if (extdlg) {
         u32t id;
         vio_setpos(my-1, 4);
//         printf("\x1B[=7h" C_KEY "Enter" C_TXT " - select   " C_KEY "Del" C_TXT
//                " - kill process   " C_KEY "Esc" C_TXT " - exit ");
         printf("\x1B[=7h" C_KEY "Enter" C_TXT " - select   " C_KEY "Esc" C_TXT " - exit ");
         /* crazy to use it in a task list thread, but why not?
            this is just a "pid list changed" flag check */
         signal(SIGALRM, process_watcher);
         id = slow_dev[device] ? PID_CHECK_SLOW : PID_CHECK_PERIOD;
         mt_tlsset(tls_period, id);
         alarm(id);

         id = extdlg->tasklistdlg(0, MSG_BLUE|MSG_SESFORE, TLDT_CDEV|TLDT_NOSELF|
                                     TLDT_NODEL|TLDT_HINTCOL|MSG_CYAN, KEYC_UPDATE);
         if (id) {
            se_stats *sd = se_stat(id);
            if (sd) {
               if (sd->devmask & 1<<device) se_switch(id, 1<<device); else {
                  if (sd->devmask==0)
                     if (se_attach(id,device))
                        vio_msgbox("Task list", "Session has no active screen devices, "
                           "but not suitable for this device", MSG_OK|MSG_RED, 0);
                  se_switch(id, FFFF);
               }
            }
         }
      } else
         vio_msgbox("Task list", "Service module is not available!", MSG_OK|MSG_RED, 0);
   }
   mt_swlock();
   /* in theory, we can skip task list calling if exiting from it at this moment.
      But case is too rare and user can always press it again ;) */
   tasklists[device] = 0;
   if (--n_active==0) {
      sys_notifyevent(0, update_now);
      sys_notifyevent(0, update_later);
   }
   mt_swunlock();
   return 0;
}

qserr _std se_tasklist(u32t devmask) {
   u32t      ii;
   se_stats *di = 0;    // available devices information

   if (!devmask) return E_SYS_INVPARM;
   if (!mt_active()) return E_MT_DISABLED;
   mt_swlock();
   // first time action
   if (!extdlg) extdlg = NEW_G(qs_extcmd);
   // create thread for every asked device
   for (ii=0; ii<SYS_HANDLERS; ii++)
      if (1<<ii & devmask)
         if (tasklists[ii]) {
            // just switch exising task list to the foreground
            se_switch(tasklists[ii]->tiSession, 1<<ii);
         } else {
            mt_ctdata ctd = { sizeof(mt_ctdata), 0, 8192, 0, 0, 0, 1, 0 };
            mt_tid    tid = mt_createthread(tasklist_thread, MTCT_NOFPU|
                               MTCT_NOTRACE|MTCT_SUSPENDED, &ctd, (void*)ii);
            if (tid) {
               if (!di) di = se_deviceenum();
               // try to mark "slow" device
               slow_dev[ii] = 0;
               if (di) {
                  u32t ll;
                  for (ll=0; ll<di->handlers; ll++)
                     if (di->mhi[ll].dev_id==ii)
                        if (di->mhi[ll].dev_flags&VHI_SLOW) slow_dev[ii] = 1;
               }
               // launch it!
               tasklists[ii] = get_by_tid(pd_qsinit, tid);
               if (n_active++==0) {
                  sys_notifyevent(SECB_SESTART|SECB_SEEXIT|SECB_GLOBAL, update_now);
                  sys_notifyevent(SECB_SETITLE|SECB_DEVADD|SECB_GLOBAL, update_later);
               }
               mt_resumethread(1, tid);
            }
         }
   mt_swunlock();
   if (di) free(di);
   return 0;
}
