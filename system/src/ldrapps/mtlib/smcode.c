//
// QSINIT
// session switching & vio handling
// ------------------------------------------------------------
//
#include "mtlib.h"

static vio_handler      *cvio = 0;
static vio_session_data **sda = 0;
static qshandle        keymux = 0;
static u32t         sno_alloc = 0;    ///< allocated sda size
u32t                 kbask_ms = 0;    ///< keyb thread polling period
volatile static 
u32t               current_ks = 1,    ///< current keyboard active session
                   current_vs = 1;    ///< current vio active session
extern
mod_addfunc    *mod_secondary;        ///< function table for QSINIT, we use exit_cb here
se_free_data          *se_rel = 0;    ///< list of sessions to release

static void process_sysq(qe_event *ev);

static void _std v_init(u32t sesno) {
}

static void _std v_fini(void) {
}

static vio_session_data* _std v_savestate(void) {
   return 0;
}

static void _std v_reststate(vio_session_data *state) {

}

static void _std v_switchto(int foreground) {
}

static void _std v_setmode(u32t lines) {
   if (pt_current->tiSession==SESN_DETACHED) return;
   cvio->vh_setmode(lines);
}

static int  _std v_setmodeex(u32t cols, u32t lines) {
   if (pt_current->tiSession==SESN_DETACHED) return 0;
   return cvio->vh_setmodeex(cols, lines);
}

static void _std v_resetmode(void) {
   if (pt_current->tiSession==SESN_DETACHED) return;
   cvio->vh_resetmode();
}

static u8t  _std v_getmode(u32t *cols, u32t *lines) {
   if (pt_current->tiSession==SESN_DETACHED) {
      if (cols) *cols = 80;
      if (lines) *lines = 25;
      return 0;
   }
   return cvio->vh_getmode(cols, lines);
}

static void _std v_getmfast(u32t *cols, u32t *lines) {
   if (pt_current->tiSession==SESN_DETACHED) {
      if (cols) *cols = 80;
      if (lines) *lines = 25;
   }
   cvio->vh_getmfast(cols, lines);
}

static void _std v_intensity(u8t value) {
   if (pt_current->tiSession==SESN_DETACHED) return;
   cvio->vh_intensity(value);
}

static void _std v_setshape(u8t start, u8t end) {
   if (pt_current->tiSession==SESN_DETACHED) return;
   cvio->vh_setshape(start, end);
}

static u16t _std v_getshape(void) {
   if (pt_current->tiSession==SESN_DETACHED) return VIO_SHAPE_NONE;
   return cvio->vh_getshape();
}

static void _std v_defshape(u8t shape) {
   if (pt_current->tiSession==SESN_DETACHED) return;
   cvio->vh_defshape(shape);
}

static void _std v_setcolor(u16t color) {
   if (pt_current->tiSession==SESN_DETACHED) return;
   cvio->vh_setcolor(color);
}

static void _std v_clearscr(void) {
   if (pt_current->tiSession==SESN_DETACHED) return;
   cvio->vh_clearscr();
}

static void _std v_setpos(u8t line, u8t col) {
   if (pt_current->tiSession==SESN_DETACHED) return;
   cvio->vh_setpos(line, col);
}

static void _std v_getpos(u32t *line, u32t *col) {
   if (pt_current->tiSession==SESN_DETACHED) {
      if (line) *line = 0;
      if (col) *col = 0;
      return;
   }
   cvio->vh_getpos(line, col);
}

static u32t* _std v_ttylines(void) {
   if (pt_current->tiSession==SESN_DETACHED) {
      static u32t fakevar;
      // zero it at every call in every thread
      fakevar = 0;
      return &fakevar;
   }
   return cvio->vh_ttylines();
}

static u32t _std v_charout(char ch) {
   if (pt_current->tiSession==SESN_DETACHED) return 0;
   return cvio->vh_charout(ch);
}

static u32t _std v_strout(const char *str) {
   if (pt_current->tiSession==SESN_DETACHED) return 0;
   return cvio->vh_strout(str);
}

static void _std v_writebuf(u32t col, u32t line, u32t width, u32t height, void *buf, u32t pitch) {
   if (pt_current->tiSession==SESN_DETACHED) return;
   cvio->vh_writebuf(col, line, width, height, buf, pitch);
}

static void _std v_readbuf(u32t col, u32t line, u32t width, u32t height, void *buf, u32t pitch) {
   if (pt_current->tiSession==SESN_DETACHED) return;
   cvio->vh_readbuf(col, line, width, height, buf, pitch);
}

void _std v_beep(u16t freq, u32t ms) {
   if (freq>16384 || (int)ms<0) return;
   qe_postevent(sys_q, SYSQ_BEEP, freq, ms, 0);
}

// warning! called from interrupt!
u32t key_available(u32t sno) {
   if (sno>=sno_alloc) return 0; else {
      vio_session_data *sd = sda[sno];
      return sd ? sd->vs_keybuf[sd->vs_keyrp] : 0;
   }
}

static u8t _std k_pressed(void) {
   if (pt_current->tiSession==SESN_DETACHED) return 0; else {
      int  svint = sys_intstate(0);
      u8t     rc = key_available(pt_current->tiSession)?1:0;
      sys_intstate(svint);
      return rc;
   }
}

static u8t push_to_session(u32t sno, u16t code) {
   u8t    rc = 0;
   /* disable interrupts here because w_check_conditions() called during
      timer interrupt and key buffer must be valid at this moment */
   int svint = sys_intstate(0);

   if (sno<sno_alloc) {
      vio_session_data  *sd = sda[sno];
      if (sd) {
         sd->vs_keybuf[sd->vs_keywp] = code;
         if (++sd->vs_keywp>=SESDATA_KBSIZE) sd->vs_keywp = 0;
         sd->vs_keybuf[sd->vs_keywp] = 0;
         // buffer overflow, we caught our`s read position
         if (sd->vs_keywp==sd->vs_keyrp)
            if (++sd->vs_keyrp>=SESDATA_KBSIZE) sd->vs_keyrp = 0;
         rc = 1;
      }
   }
   sys_intstate(svint);

   return rc;
}

static u16t next_key(u32t sno) {
   u16t   rc = 0;
   /* disable interrupts here because w_check_conditions() called during
      timer interrupt and key buffer must be valid at this moment */
   int svint = sys_intstate(0);

   if (sno<sno_alloc) {
      vio_session_data  *sd = sda[sno];
      if (sd) {
         rc = sd->vs_keybuf[sd->vs_keyrp];
         if (rc)
            if (++sd->vs_keyrp>=SESDATA_KBSIZE) sd->vs_keyrp = 0;
      }
   }
   sys_intstate(svint);
   return rc;
}

// must be called inside keymux only!
static void key_read_loop(void) {
   u16t key;
   int  cnt = SESDATA_KBSIZE/2;
   mt_swlock();
   do {
      key = cvio->vh_kwait(0);
      if (key) {
#if 0
         if (pt_current->tiPID!=1)
            log_it(3,"key_read_loop (%u, %u) = %hX!\n", pt_current->tiPID, pt_current->tiTID, key);
#endif
         if (log_hotkey(key)) continue;
         /* in theory session can be changed during this call, but
            just ignore this (current_ks will be happy reciever) */
         push_to_session(current_ks, key);
      }
   } while (--cnt && key);
   // hlp_seroutchar(cnt<SESDATA_KBSIZE/2-1?'\"':'\'');
   mt_swunlock();
}

static u16t _std k_wait(u32t seconds) {
   u16t code = 0;
   if (k_pressed()) {
      code = next_key(pt_current->tiSession);
      if (code) return code;
   }
   // logic is not clear, but most of time it should work fine.
   if (seconds==0) {
      if (pt_current->tiSession==current_ks) {
         mt_muxcapture(keymux);
         // check it again, yes!
         if (pt_current->tiSession==current_ks) key_read_loop();
         mt_muxrelease(keymux);
      }
      code = next_key(pt_current->tiSession);
   } else {
      mt_waitentry we[2] = {{QWHT_KEY,1}, {QWHT_CLOCK,0}};
      u32t        sig;
      we[1].tme = sys_clock() + (u64t)seconds*CLOCKS_PER_SEC;
      do {
         sig = 0;
         mt_waitobject(&we, 2, 0, &sig);
         if (sig==1) code = next_key(pt_current->tiSession); else
            break; // timeout or error
      } while (!code);
   }
   return code;
}

static u16t _std k_read(void) {
   return k_wait(x7FFF);
}

static u32t _std k_status(void) {
   if (pt_current->tiSession==SESN_DETACHED) return 0; else {
      u32t  rc;
      mt_swlock();
      rc = cvio->vh_kstatus();
      mt_swunlock();
      return rc;
   }
}

static u8t _std k_push(u16t code) {
   if (pt_current->tiSession==SESN_DETACHED) return 0; else
      return push_to_session(pt_current->tiSession, code);
}

static u32t _std keyread_thread(void *arg) {
   mt_threadname("keyboard");
   while (1) {
      mt_waitentry we[2] = {{QWHT_MUTEX,1}, {QWHT_CLOCK,1}};
      u32t        res;
      qe_event    *ev;
      we[0].mux = keymux;
      we[1].tme = sys_clock() + kbask_ms*1000;
      // process sys queue (exec time subtracted from mt_waitobject() below)
      do {
         ev = qe_waitevent(sys_q, 0);
         if (ev) {
            process_sysq(ev);
            free(ev);
         }
      } while (ev);
      // wait for mutex AND possible key
      res = mt_waitobject(&we, 2, QWCF_GSET(1,QWCF_AND), 0);
      if (res) {
         log_it(3,"keyb thr wo rc %X!\n", res);
         usleep(kbask_ms*1000);
         mt_muxcapture(keymux);
      }
      key_read_loop();
      mt_muxrelease(keymux);
   }
   return 0;
}

static vio_handler mtvio = { "mtvio", v_init, v_fini, v_savestate, v_reststate,
   v_setmode, v_setmodeex,v_resetmode, v_getmode, v_getmfast, v_intensity,
   v_setshape, v_getshape, v_defshape, v_setcolor, v_clearscr, v_setpos,
   v_getpos, v_ttylines, v_charout, v_strout, v_writebuf, v_readbuf, k_read,
   k_pressed, k_wait, k_status, k_push };

static u32t se_newslot(void) {
   vio_session_data *sd;
   int            svint = sys_intstate(0);
   u32t            *pos = 0, rc;

   if (!sda) sda = (vio_session_data **)calloc(sno_alloc = 20, sizeof(void*));

   if (sda) pos = memchrd((u32t*)sda, 0, sno_alloc);
   if (!pos) {
      u32t new_sno = sno_alloc+10<<1;
      sda = (vio_session_data **)realloc(sda, new_sno*sizeof(ptrdiff_t));
      memset(sda+sno_alloc, 0, (new_sno-sno_alloc)*sizeof(ptrdiff_t));
      sno_alloc = new_sno;
      pos = memchrd((u32t*)sda, 0, sno_alloc);
   }
   rc = pos-(u32t*)sda;
   sd = (vio_session_data*)malloc(sizeof(vio_session_data));
   mem_zero(sd);
   sd->vs_sign  = SESDATA_SIGN;
   sd->vs_x     = 80;
   sd->vs_y     = 25;
   sda[rc] = sd;
   sys_intstate(svint);
   return rc;
}

qserr _std mod_execse(u32t mod, const char *env, const char *params,
   u32t flags, mt_pid *ppid)
{
   module *mi = (module*)mod;
   qserr  res = 0;
   if (!mt_on) {
      res = mt_initialize();
      if (res) return res;
   }
   if (!mod || (flags&~(QEXS_DETACH|QEXS_WAITFOR|QEXS_NOTACHILD)))
      return E_SYS_INVPARM;
   if ((flags&QEXS_DETACH)==0) return E_SYS_UNSUPPORTED;

   mt_swlock();
   if (mi->sign!=MOD_SIGN) res = E_SYS_INVOBJECT; else 
   if (mi->flags&MOD_EXECPROC) res = E_MOD_EXECINPROC; else
   if (mi->flags&MOD_LIBRARY) res = E_MOD_LIBEXEC; else {
      int         eres;
      se_start_data ld;
      // here we safe again
      mt_swunlock();

      ld.sign  = SEDAT_SIGN;
      ld.flags = flags;
      ld.pid   = 0;
      eres     = mod_exec(mod, env, params, &ld);
      res      = eres<0?E_SYS_SOFTFAULT:0;
      if (!res && ppid) *ppid = ld.pid;
   }
   return res;
}

/// free session module after exit
void se_release(mt_prcdata *pd) {
   mod_secondary->exit_cb(pd->piContext, pd->piExitCode);
   mt_exit(pd->piContext);
   mt_safedand(&pd->piModule->flags, ~MOD_EXECPROC);
   mod_free((u32t)pd->piModule);
}

static void process_sysq(qe_event *ev) {
   switch (ev->code) {
      case SYSQ_SESSIONFREE:
         se_release((mt_prcdata*)ev->a);
         break;
      case SYSQ_BEEP:
         if (_beep) _beep(ev->a,ev->b);
         break;
      default:
         log_it(3,"unknown sys_q event %X %X %X %X!\n", ev->code, ev->a, ev->b, ev->c);
   }
}

static void _std start_keyb_thread(sys_eventinfo *info) {
   mt_ctdata  ctd;
   mt_tid     tid;
   mt_thrdata *td;

   cvio = (vio_handler*)mt_exechooks.mtcb_cvh;
   mt_exechooks.mtcb_cvh = &mtvio;

   if (mt_muxcreate(0,0,&keymux)) {
      log_it(0,"keyb mux error!\n");
      THROW_ERR_PD(pd_qsinit);
   } else
      io_setstate(keymux, IOFS_DETACHED, 1);
   // keyboard poll thread
   memset(&ctd, 0, sizeof(mt_ctdata));
   ctd.size      = sizeof(mt_ctdata);
   ctd.stacksize = 8192;
   ctd.pid       = 1;

   tid = mt_createthread(keyread_thread, 0, &ctd, 0);
   if (tid) td = get_by_tid(pd_qsinit, tid);
   if (!td) {
      log_it(0, "keyb thread error!\n");
      THROW_ERR_PD(pd_qsinit);
   } else
      td->tiMiscFlags|=TFLM_SYSTEM|TFLM_NOFPU|TFLM_NOTRACE;
}

void init_session_data(void) {
   if (!sys_notifyevent(SECB_MTMODE|SECB_GLOBAL, start_keyb_thread) ||
      se_newslot()!=0 || se_newslot()!=1)
   { 
      log_it(0,"sess init err!\n");
      THROW_ERR_PD(pd_qsinit);
   }
}
