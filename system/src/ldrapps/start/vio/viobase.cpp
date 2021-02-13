//
// QSINIT "start" module
// vio handler chains
// ------------------------------------------------------------------
//
#include "qcl/sys/qsvioint.h"
#include "qsbase.h"
#include "qserr.h"
#include "syslocal.h"
#include "qsvdata.h"
#include "classes.hpp"
#include "sysio.h"

static TPtrList   *ses_list = 0,
                  *hk_funcs = 0;
static TUList     *hk_codes = 0,
                    *hk_pid = 0;
static u32t   dbport_device = 0;
#define Sessions (*ses_list)

extern u32t               vh_vio_classid;
extern "C" {
extern mod_addfunc*        mod_secondary;
extern mt_proc_cb           mt_exechooks;
#pragma aux mod_secondary  "_*";
#pragma aux mt_exechooks   "_*";
}

static qs_vh            VH[SYS_HANDLERS];   ///< array of output handlers.
static u32t            Vsn[SYS_HANDLERS];   ///< active session number
static u32t                       VHfree,   ///< start of empty area in VH
                                  VHslow;   ///< bit mask - indicating slow devices

#define call_out(se,func,...) {                  \
   if (se->vs_flags&VSF_FOREGROUND)              \
      for (u32t idx=0; idx<VHfree; idx++)        \
         if (VH[idx] && Vsn[idx]==se->vs_selfno) \
            VH[idx]->func(__VA_ARGS__);          \
}

static int _std log_hotkey(u16t key, u32t status, u32t device);

static qs_vh new_handler(const char *setup, const char *name, u32t id = 0,
                         qserr *perr = 0, int local = 0)
{
   qserr  err = 0;
   qs_vh  res = 0;
   if (!id) id = exi_queryid(name);
   // check # of methods, at least
   if (!id) err = E_SYS_NOTFOUND; else
      if (exi_methods(id)*sizeof(void*)!=sizeof(_qs_vh)) err = E_CON_BADHANDLER;
   // create instance
   if (!err) {
      res = (qs_vh)exi_createid(id, local?0:EXIF_SHARED);
      if (res) {
         err = res->init(setup);
         if (err) {
            log_it(0,"video handler %s(%u) init error %05X\n", name, id, err);
            DELETE(res);
            res = 0;
         }
      } else
         err =E_SYS_SOFTFAULT;
   }
   if (perr) *perr = err;
   return res;
}

// must be called inside of MT lock only
static int new_handler_slot(void) {
   u32t *pos = memchrd((u32t*)&VH, 0, SYS_HANDLERS);
   if (!pos) return -1;
   return pos - (u32t*)&VH;
}

static void update_VHfree(void) {
   u32t *pos = memrchrnd((u32t*)&VH, 0, SYS_HANDLERS);
   if (pos) {
       u32t nv = (pos - (u32t*)&VH) + 1;
       if (nv!=VHfree) {
          VHfree = nv;
          log_it(3, "VHfree = %u\n", VHfree);
       }
   }
}

static se_sysdata *newslot(void) {
   se_sysdata *sd = new se_sysdata;
   mem_zero(sd);
   sd->vs_sign  = SESDATA_SIGN;
   sd->vs_x     = 80;
   sd->vs_y     = 25;
   // allocate buffer instance
   sd->vs_tb    = new_handler(0, QS_VH_BUFFER);
   sd->vs_focus = NEW_G(dd_list);
   return sd;
}

static se_sysdata* get_se(u32t sesno) {
   if (ses_list)
      if (sesno<=Sessions.Max()) return (se_sysdata*)Sessions[sesno];
   return 0;
}

// called from the timer interrupt!
se_sysdata* _std se_dataptr(u32t sesno) {
   int       svint = sys_intstate(0);
   se_sysdata  *rc = get_se(sesno);
   sys_intstate(svint);
   return rc;
}

u32t _std se_foreground(void) {
   // should be atomic and faster than se_active() because of no lock
   return Vsn[0];
}

u32t _std se_active(u32t dev_id) {
   if (dev_id>=SYS_HANDLERS) return 0;
   MTLOCK_THIS_FUNC lk;
   if (!VH[dev_id]) return 0;
   return Vsn[dev_id];
}

u32t se_curalloc(void) {
   MTLOCK_THIS_FUNC lk;
   return ses_list?ses_list->Count():0;
}

se_stats* _std se_stat(u32t sesno) {
   MTLOCK_THIS_FUNC lk;
   se_sysdata* se = get_se(sesno);
   if (!se) return 0;
   u32t   ii, cnt,
         titlelen = se->vs_title?strlen(se->vs_title)+1:0,
         titlepos = sizeof(se_stats) + sizeof(((se_stats*)0)->mhi)*VHfree;
   se_stats   *rc = (se_stats*)malloc_local(titlepos + titlelen);

   for (ii=0, cnt=0; ii<VHfree; ii++)
      if (Vsn[ii]==sesno) {
         rc->mhi[cnt].dev_id    = ii;
         rc->mhi[cnt].dev_flags = VH[ii]->info(0, rc->mhi[cnt].name);
         rc->mhi[cnt].reserved  = se->vs_devdata[ii];
         cnt++;
      }
   rc->handlers = cnt;
   rc->devmask  = se->vs_devmask;
   rc->sess_no  = sesno;
   rc->flags    = se->vs_flags;
   rc->mx       = se->vs_x;
   rc->my       = se->vs_y;
   rc->focus    = se->vs_focus->count()?se->vs_focus->value(se->vs_focus->max()):0;
   rc->title    = titlelen ? (char*)rc + titlepos : 0;
   if (rc->title) strcpy(rc->title, se->vs_title);
   return rc;
}

static qserr setmode_int(qs_vh dev, u32t cols, u32t lines, u16t modeid = 0) {
   u32t mx = 0, my;
   u16        cmid;
   /* mode is the same - we just reset all settings insead of real setmode
      call! */
   if (!dev->getmode(&mx, &my, &cmid))
      if (mx==cols && my==lines && (!modeid || modeid==cmid)) {
         dev->clear();
         dev->setcolor(VIO_COLOR_WHITE);
         dev->setopts(VHSET_SHAPE|VIO_SHAPE_LINE);
         dev->setopts(VHSET_LINES);
         return 0;
      }
   return modeid ? dev->setmodeid(modeid) : dev->setmode(cols, lines, 0);
}

/// set mode on all active devices for the session.
static int setmode_alldevs(se_sysdata* se, u32t cols, u32t lines, u32t except = 0) {
   u32t nmask = 0, failmask = 0,
      devmask = se->vs_devmask & ~except;

   for (u32t ii=0; ii<VHfree; ii++)
      if (1<<ii & devmask) {
         // avoid of setmode without check its availability
         vio_mode_info *mi = vio_modeinfo(cols, lines, ii), *mp = mi;
         qserr         res = E_CON_MODERR;
         if (mi) {
            // reset custom mode if it not in the list of modes with asked resolution
            if (se->vs_devdata[ii]) {
               vio_mode_info *mp = mi;
               while (mp->size)
                  if ((u16t)mp->mode_id==se->vs_devdata[ii]) break; else mp++;
               if (!mp->size) se->vs_devdata[ii] = 0;
            }
            // set mode on devices where we`re in favor ;)
            if (Vsn[ii]!=se->vs_selfno) res = 0; else
               res = setmode_int(VH[ii], cols, lines, se->vs_devdata[ii]);
            free(mi);
         }
         // finally, we got it
         if (!res) nmask|=1<<ii; else
            if (Vsn[ii]==se->vs_selfno) failmask|=1<<ii;
      }
   // at least one call was good
   if (nmask || except) {
      se->vs_devmask = se->vs_devmask & except | nmask;
      se->vs_tb->setmode(cols, lines, 0);
      se->vs_x = cols;
      se->vs_y = lines;
   }
   // update foreground for all devises, who lost this session
   if (failmask) se_switch(0, failmask);
   return nmask?1:0;
}

static int _std v_setmodeex(u32t cols, u32t lines) {
   MTLOCK_THIS_FUNC lk;
   se_sysdata* se = get_se(se_sesno());
   if (se->vs_selfno==SESN_DETACHED) return 0;

   // this is possible and no limits for mode resultion here!
   if (!se->vs_devmask) {
      if (se->vs_tb->setmode(cols, lines, 0)) return 0;
      se->vs_x = cols;
      se->vs_y = lines;
      return 1;
   }
   return setmode_alldevs(se, cols, lines);
}

static void _std v_setmode(u32t lines) {
   if (lines!=25 && lines!=43 && lines!=50) return;
   v_setmodeex(80, lines);
}

static void _std v_resetmode(void) {
   MTLOCK_THIS_FUNC lk;
   se_sysdata* se = get_se(se_sesno());
   int       once = 0;
   if (se->vs_flags&VSF_FOREGROUND)
      for (u32t idx=0; idx<VHfree; idx++)
         if (VH[idx] && Vsn[idx]==se->vs_selfno) {
            VH[idx]->reset();
            VH[idx]->getmode(&se->vs_x, &se->vs_y, 0);
            // reset custom mode
            se->vs_devdata[idx] = 0;
            once = 1;
         }
   // update buffer
   if (!once) {
      se->vs_tb->reset();
      se->vs_tb->getmode(&se->vs_x, &se->vs_y, 0);
   } else
      se->vs_tb->setmode(se->vs_x, se->vs_y, 0);
}

static u8t _std v_getmode(u32t *cols, u32t *lines) {
   MTLOCK_THIS_FUNC lk;
   se_sysdata* se = get_se(se_sesno());
   if (cols) *cols = se->vs_x;
   if (lines) *lines = se->vs_y;
   return 1;
}

static void _std v_getmfast(u32t *cols, u32t *lines) {
   v_getmode(cols, lines);
}

static void _std v_intensity(u8t value) {
   MTLOCK_THIS_FUNC lk;
   se_sysdata* se = get_se(se_sesno());
   if (se->vs_selfno==SESN_DETACHED) return;

   se->vs_tb->setopts(value?0:VHSET_BLINK);
   call_out(se, setopts, value?0:VHSET_BLINK);
}

static void _std v_setshape(u16t shape) {
   MTLOCK_THIS_FUNC lk;
   se_sysdata* se = get_se(se_sesno());
   if (se->vs_selfno==SESN_DETACHED) return;

   se->vs_tb->setopts(VHSET_SHAPE|shape);
   call_out(se, setopts, VHSET_SHAPE|shape);
}

static u16t _std v_getshape(void) {
   MTLOCK_THIS_FUNC lk;
   se_sysdata      *se = get_se(se_sesno());
   vh_settings      sb;
   if (se->vs_selfno==SESN_DETACHED || se->vs_tb->state(&sb))
      return VIO_SHAPE_NONE;
   return sb.shape;
}

static void _std v_setcolor(u16t color) {
   MTLOCK_THIS_FUNC lk;
   se_sysdata* se = get_se(se_sesno());
   if (se->vs_selfno==SESN_DETACHED) return;

   call_out(se, setcolor, color);
   se->vs_tb->setcolor(color);
}

static u16t _std v_getcolor(void) {
   MTLOCK_THIS_FUNC lk;
   se_sysdata      *se = get_se(se_sesno());
   vh_settings      sb;
   if (se->vs_selfno==SESN_DETACHED || se->vs_tb->state(&sb))
      return VIO_COLOR_WHITE;
   return sb.color;
}

static void _std v_clearscr(void) {
   MTLOCK_THIS_FUNC lk;
   se_sysdata* se = get_se(se_sesno());
   if (se->vs_selfno==SESN_DETACHED) return;

   se->vs_tb->clear();
   call_out(se, clear, );
}

static void _std v_setpos(u8t line, u8t col) {
   MTLOCK_THIS_FUNC lk;
   se_sysdata* se = get_se(se_sesno());
   if (se->vs_selfno==SESN_DETACHED) return;

   se->vs_tb->setpos(line, col);
   call_out(se, setpos, (u32t)line, (u32t)col);
}

static void _std v_getpos(u32t *line, u32t *col) {
   MTLOCK_THIS_FUNC lk;
   se_sysdata      *se = get_se(se_sesno());
   vh_settings      sb;

   if (line) *line = 0;
   if (col)  *col  = 0;
   if (se->vs_selfno==SESN_DETACHED || se->vs_tb->state(&sb)) return;
   if (line) *line = sb.py;
   if (col)  *col  = sb.px;
}

static u32t _std v_ttylines(s32t nval) {
   MTLOCK_THIS_FUNC lk;
   vh_settings      sb;
   se_sysdata      *se = get_se(se_sesno());

   if (se->vs_selfno==SESN_DETACHED || se->vs_tb->state(&sb)) return 0;
   // update if requested
   if (nval>=0) {
      se->vs_tb->setopts(VHSET_LINES|nval);
      call_out(se, setopts, VHSET_LINES|nval);
   }
   return sb.ttylines;
}

static u32t _std v_charout(char ch) {
   MTLOCK_THIS_FUNC lk;
   se_sysdata* se = get_se(se_sesno());
   if (se->vs_selfno==SESN_DETACHED) return 0;

   call_out(se, charout, ch);
   return se->vs_tb->charout(ch);
}

static u32t _std v_strout(const char *str) {
   MTLOCK_THIS_FUNC lk;
   se_sysdata* se = get_se(se_sesno());
   if (se->vs_selfno==SESN_DETACHED) return 0;

   call_out(se, strout, str);
   return se->vs_tb->strout(str);
}

static void _std v_writebuf(u32t col, u32t line, u32t width, u32t height, void *buf, u32t pitch) {
   MTLOCK_THIS_FUNC lk;
   se_sysdata* se = get_se(se_sesno());
   if (se->vs_selfno==SESN_DETACHED) return;

   if (!pitch) pitch = width<<1;

   if (se->vs_flags&VSF_FOREGROUND)
      for (u32t idx=0; idx<VHfree; idx++)
         if (VH[idx] && Vsn[idx]==se->vs_selfno)
            /* little optimization, which makes Turbo Vision a bit more Turbo,
               than Crawl on serial port console and EFI native console */
            if (VHslow & 1<<idx) {
               if (line>=se->vs_y || col>=se->vs_x) continue;
               if (col +width >se->vs_x) width  = se->vs_x - col;
               if (line+height>se->vs_y) height = se->vs_y - line;
               if (width && height) {
                  u16t *src = (u16t*)buf;
                  for (u32t  hh = height, ln = line; hh>0; hh--,ln++) {
                     u16t *bptr = src,
                          *bscr = se->vs_tb->memory() + ln*se->vs_x + col;
                     u32t   wdt = width;
                     while (wdt && *bptr==*bscr) { bptr++; bscr++; wdt--; }
                     if (wdt) {
                        u32t pos = bptr - src;
                        bptr += wdt - 1;
                        bscr += wdt - 1;
                        while (wdt && *bptr==*bscr) { bptr--; bscr--; wdt--; }
                        if (wdt) {
                           VH[idx]->writebuf(col+pos, ln, wdt, 1, src+pos, 0);
                           // release lock for other threads (this output can be really slow)
                           mt_swunlock();
                           mt_swlock();
                        }
                     }
                     src = (u16t*)((u8t*)src+pitch);
                  }
               }
            } else
               VH[idx]->writebuf(col, line, width, height, buf, pitch);

   se->vs_tb->writebuf(col, line, width, height, buf, pitch);
}

static void _std v_readbuf(u32t col, u32t line, u32t width, u32t height, void *buf, u32t pitch) {
   MTLOCK_THIS_FUNC lk;
   se_sysdata* se = get_se(se_sesno());
   if (se->vs_selfno==SESN_DETACHED) return;

   if (!pitch) pitch = width<<1;
   se->vs_tb->readbuf(col, line, width, height, buf, pitch);
}

qserr _std se_keypush(u32t sesno, u16t code, u32t statuskeys) {
   MTLOCK_THIS_FUNC lk;
   se_sysdata* se = get_se(sesno);
   if (!se) return E_CON_SESNO;
   if (se->vs_selfno==SESN_DETACHED) return E_CON_DETACHED;
   // ctrl-c/ctrl-break processing (but send it to the user too)
   if (code==0x2E03 || code==KEYC_CTRLBREAK) {
      u32      pid = se->vs_focus->count()?se->vs_focus->value(se->vs_focus->max()):0;
      if (pid && mt_active()) get_mtlib()->sendsignal(pid, 0, QMSV_BREAK, QMSF_TREE);
   }
   // common key push
   se->vs_keybuf[se->vs_keywp] = statuskeys<<16|code;
   if (++se->vs_keywp>=SESDATA_KBSIZE) se->vs_keywp = 0;
   se->vs_keybuf[se->vs_keywp] = 0;
   // buffer overflow, we caught our`s read position
   if (se->vs_keywp==se->vs_keyrp)
      if (++se->vs_keyrp>=SESDATA_KBSIZE) se->vs_keyrp = 0;
   return 0;
}

/* both avail_key() & next_key() must be called in MT lock.

   MTLIB checks for key availability by direct read of session keyboard
   buffer, so all of read/write ops must be covered by MT lock. */
static inline u8t avail_key(se_sysdata *se) {
   return se->vs_keybuf[se->vs_keyrp]?1:0;
}

static u32t next_key(se_sysdata *sd) {
   u32t rc = sd->vs_keybuf[sd->vs_keyrp];
   if (rc)
      if (++sd->vs_keyrp>=SESDATA_KBSIZE) sd->vs_keyrp = 0;
   return rc;
}

extern "C"
void vh_input_read_loop(void) {
#define CB_LEN 16
   mt_swlock();
   for (u32t ii=0; ii<VHfree; ii++)
      if (VH[ii]) {
         qs_kh kh = VH[ii]->input();
         if (kh) {
            u32t codes[CB_LEN], cbpos = 0;
            /* loop device read without releasing of MT lock - and only when
                no more key presses - type all keys into the queue.
                Device without active session will react on the hotkey too */
            while (1) {
               u32t status;
               u16t   code = kh->read(0, &status);
               if (!code) break;
               codes[cbpos++] = code|status<<16;
               if (cbpos>=CB_LEN) break;
            }
            if (cbpos)
               for (u32t ci=0; ci<cbpos; ci++) {
                  u32t code = codes[ci];
                  mt_swunlock();
                  int hotkey = log_hotkey((u16t)code, code>>16, ii);
                  mt_swlock();
                  if (!hotkey && Vsn[ii])
                     se_keypush(Vsn[ii], (u16t)code, code>>16);
               }
         }
      }
   mt_swunlock();
}

/* manual DC commit call in non-MT mode */
static void check_dccommit(void) {
   if (!in_mtmode) {
      static qsclock ltime = 0;
      qsclock now = sys_clock();
      if (now - ltime > CLOCKS_PER_SEC) {
         sys_dccommit(DCN_TIMER);
         ltime = now;
      }
   }
}

static u16t _std k_waitex(u32t ms, u32t *status) {
   u32t      code = 0;
   se_sysdata *se = get_se(se_sesno());

   if (in_mtmode) {
      mt_swlock();
      // logic is not clear, but most of time it should work fine.
      if (ms==0) {
         code = next_key(se);
      } else {
         mt_waitentry we[3] = {{QWHT_KEY,1}, {QWHT_CLOCK,0}, {QWHT_SIGNAL,2}};
         qs_mtlib     mt = get_mtlib();
         u32t        sig;
         we[1].tme = sys_clock() + (u64t)ms*(CLOCKS_PER_SEC/1000);
         do {
            sig = 0;
            mt_swunlock();
            mt->waitobject(we, 3, 0, &sig);
            mt_swlock();
            // read code anyway, because at may be available with timeout res
            code = next_key(se);
            // break on timeout or error
            if (sig==0) break;
         } while (!code);
      }
      mt_swunlock();
      if (status) *status = code>>16;
      return code;
   }
   // non-MT way
   qsclock  btime = 0;

   /* hotkey has able to turn MT mode on. Result will be fatal because no locks
      in this loop, so we just go away. */
   while (se && !in_mtmode) {
      if (avail_key(se)) code = next_key(se);
      if (!code) {
         qs_kh      kh = 0;
         int   is_cvio = 0;
         u32t       ii;
         // enum input handlers (every time because hotkey may alter this
         for (ii=0; ii<VHfree; ii++)
            if (VH[ii] && Vsn[ii]) { // device attached to a (it should be the one) session?
               qs_kh ckh = VH[ii]->input();
               if (ckh) {
                  // single handler must be vio (zero index)
                  is_cvio++;
                  if (ii) is_cvio++;
                  if (!kh) kh = ckh;
               }
            }
         check_dccommit();
         // no any input? - just sleep a bit and return 0
         if (!kh) {
            if (ms) cpuhlt();
            break;
         } else
         if (is_cvio==1 && (!ms || ms==FFFF)) {
            u32t status = 0;
            /* simulate old code (kread() goes into RM for a while and this
               saves VBox from immediate hanging in PAE mode :) - i.e., it able
               to work at least in shell - a few more seconds) */
            code = kh->read(ms==FFFF, &status);
            if (code)
               if (log_hotkey(code, status, 0)) {
                  code = 0;
                  if (ms) continue;
               } else
                  code|=status<<16;
         } else {
            // wait handling
            if (!btime) btime = sys_clock(); else {
               if ((u32t)((sys_clock()-btime)/1000) > ms) break;
               if (hlp_hosttype()==QSHT_EFI) usleep(10000); else cpuhlt();
            }
            vh_input_read_loop();
            if (ms) check_dccommit();
            continue;
         }
      }
      break;
   }
   if (ms) check_dccommit();
   if (status) *status = code>>16;
   return code;
}

static u8t _std k_pressed(void) {
   MTLOCK_THIS_FUNC lk;
   u32t     sesno = se_sesno(), status;
   se_sysdata* se = get_se(sesno);

   if (avail_key(se)) return 1;

   while (1) {
      u16t code = k_waitex(0, &status);
      if (!code) break;
      se_keypush(sesno, code, status);
      return 1;
   }
   return 0;
}

static u16t _std k_read(void) {
   return k_waitex(FFFF,0);
}

static u32t _std k_status(void) {
   MTLOCK_THIS_FUNC lk;
   se_sysdata* se = get_se(se_sesno());

   if (se->vs_selfno==SESN_DETACHED || (se->vs_flags&VSF_FOREGROUND)==0)
      return 0;

   for (u32t idx=0; idx<VHfree; idx++)
      if (VH[idx] && Vsn[idx]==se->vs_selfno) {
         qs_kh in = VH[idx]->input();
         if (in) return in->status();
      }
   return 0;
}

static u8t _std k_push(u16t code) {
   u32t state = 0;
   u8t     ec = code>>8;
   // try to simulate valid keyboard state for this push
   if (ec)
      if (ec>=0x54&&ec<=0x5D || ec==0x87 || ec==0x88) state|=KEY_SHIFTLEFT+KEY_SHIFT;
         else
      if (ec>=0x5E&&ec<=0x67 || ec>=0x72&&ec<=0x77 || ec==0x84 || ec==0x89 ||
         ec==0x8A || ec>=0x8D&&ec<=0x96) state|=KEY_CTRLLEFT+KEY_CTRL; else
      if (ec>=0x68&&ec<=0x71 || ec>=0x0F&&ec<=0x35 || ec==0x39 || ec>=0x78&&ec<=0x83
         || ec==0x8B || ec==0x8C || ec>=0x97&&ec<=0xA6 || code==0x0100 || code==0x0E00
            || code==0x3700 || code==0x4A00 || code==0x4E00)
               state|=KEY_ALTLEFT+KEY_ALT;

   return se_keypush(se_sesno(), code, state) ? 0 : 1;
}

static vio_handler bvio = { "basevio", v_setmode, v_setmodeex, v_resetmode,
   v_getmode, v_getmfast, v_intensity, v_setshape, v_getshape, v_setcolor,
   v_getcolor, v_clearscr, v_setpos, v_getpos, v_ttylines, v_charout,
   v_strout, v_writebuf, v_readbuf, k_read, k_pressed, k_waitex, k_status,
   k_push };

/** get video mode(s) information.
    Function return information about one or more (or all of available) video
    modes. Note, that mx, my and dev_id values are used as a filters for the
    entire mode list.
    @param mx             text mode width, use 0 for any available width
    @param my             text mode height, use 0 for any available height
    @param dev_id         use -1 for mode on any device, else - device id
                          (0 - screen).
    @return one or more mode information records in user`s heap buffer (must
            be free()-ed). The end of list indicated by vio_mode_info.size
            field = 0. */
vio_mode_info* _std vio_modeinfo(u32t mx, u32t my, int dev_id) {
   if (VHTable[VHI_ACTIVE]!=&bvio) return 0;
   if (dev_id>=SYS_HANDLERS) return 0;

   MTLOCK_THIS_FUNC lk;
   if (!VH[dev_id]) return 0;

   vio_mode_info *rc = 0;
   u32t        alloc = 0, ccount = 0, idx;

   for (idx=0; idx<VHfree; idx++)
      if (VH[idx] && (dev_id<0 || dev_id==idx)) {
         vio_mode_info *ml = 0;
         VH[idx]->info(&ml, 0);

         if (ml) {
            for (u32t ii = 0; ml[ii].mx; ii++) {
               if ((!mx || ml[ii].mx==mx) && (!my || ml[ii].my==my)) {
                  if (ccount>=alloc) {
                     alloc += 4;
                     rc = (vio_mode_info*)realloc_thread(rc, alloc*sizeof(vio_mode_info)+4);
                  }
                  memcpy(rc+ccount, ml+ii, sizeof(vio_mode_info));
                  // add device number to mode_id
                  rc[ccount++].mode_id |= idx<<16;
               }
            }
            free(ml);
            if (dev_id>=0) break;
         }
      }
   if (rc) {
      // we should always have space for this dword (reserved in realloc above)
      rc[ccount].size = 0;
      mem_localblock(rc);
   }
   return rc;
}

qserr _std vio_setmodeid(u32t mode_id) {
   // check - is we`re the ruler?
   if (VHTable[VHI_ACTIVE]!=&bvio) return E_SYS_SOFTFAULT;
   // check device number word in mode_id
   u32t dev_id = mode_id>>16;
   if (dev_id>=SYS_HANDLERS) return E_SYS_INVPARM;

   MTLOCK_THIS_FUNC lk;
   if (!VH[dev_id]) return E_CON_NODEVICE;
   se_sysdata* se = get_se(se_sesno());
   if (se->vs_selfno==SESN_DETACHED) return E_CON_DETACHED;

   vio_mode_info *mi = vio_modeinfo(0, 0, dev_id), *mp;
   if (!mi) return E_CON_MODERR;

   qserr rc = E_CON_BADMODEID;

   for (mp=mi; mp->size; mp++)
      if (mp->mode_id==mode_id) {
         if (Vsn[dev_id]==se->vs_selfno) rc = VH[dev_id]->setmodeid(mode_id);
            else rc = 0;
         // save custom mode
         if (!rc) se->vs_devdata[dev_id] = mode_id;
         break;
      }
   if (!rc) setmode_alldevs(se, mp->mx, mp->my, 1<<dev_id);
   free(mi);
   return rc;
}

qserr _std vio_querymodeid(u32t mode_id, vio_mode_info *mib) {
   // check - is we`re the ruler?
   if (VHTable[VHI_ACTIVE]!=&bvio) return E_SYS_SOFTFAULT;
   // check device number word in mode_id
   u32t dev_id = mode_id>>16;
   if (dev_id>=SYS_HANDLERS) return E_SYS_INVPARM;
   // check mode info buffer
   if (!mib) return E_SYS_ZEROPTR;
   if (mib->size!=VIO_MI_SHORT && mib->size!=VIO_MI_FULL) return E_SYS_INVPARM;

   MTLOCK_THIS_FUNC lk;
   if (!VH[dev_id]) return E_CON_NODEVICE;
   se_sysdata* se = get_se(se_sesno());
   if (se->vs_selfno==SESN_DETACHED) return E_CON_DETACHED;

   vio_mode_info *mi = vio_modeinfo(0, 0, dev_id), *mp;
   if (!mi) return E_CON_MODERR;

   qserr rc = E_CON_BADMODEID;

   for (mp=mi; mp->size; mp++)
      if (mp->mode_id==mode_id) {
         u32t sz = mib->size;
         memcpy(mib, mp, sz);
         mib->size = sz;
         rc = 0;
         break;
      }
   free(mi);
   return rc;
}

/// update VSF_FOREGROUND for all sessions - MUST be called in MT lock
static void se_updforeflag(void) {
   for (u32t ii=1; ii<=Sessions.Max(); ii++) {
      se_sysdata* sd = (se_sysdata*)Sessions[ii];
      if (sd)
         if (Xor(memchrd(Vsn, sd->vs_selfno, SYS_HANDLERS), sd->vs_flags&VSF_FOREGROUND))
            sd->vs_flags ^= VSF_FOREGROUND;
   }
}

extern "C"
qserr _std START_EXPORT(se_deviceadd)(const char *handler, u32t *dev_id, const char *setup) {
   if (!handler || !dev_id) return E_SYS_ZEROPTR;
   MTLOCK_THIS_FUNC lk;
   qserr  rc;
   int   idx = new_handler_slot();
   if (idx<0) return E_CON_DEVLIMIT;
   // add new device
   VH [idx] = new_handler(setup, handler, 0, &rc);
   if (!VH[idx]) return rc?rc:E_SYS_SOFTFAULT;
   // no active session
   Vsn[idx] = 0;
   // update "slow device" bit
   if (VH[idx]->info(0,0)&VHI_SLOW) VHslow|=1<<idx; else VHslow&=~(1<<idx);
   // and VHfree
   update_VHfree();
   *dev_id  = idx;
   log_it(2, "device %u added (%s)\n", idx, handler);
   // notify about new device
   sys_notifyexec(SECB_DEVADD, idx);
   return 0;
}

extern "C"
qserr _std START_EXPORT(se_devicedel)(u32t dev_id) {
   // deny zero device deletition
   if (!dev_id) return E_SYS_ACCESS;
   if (dev_id>=SYS_HANDLERS) return E_SYS_INVPARM;

   MTLOCK_THIS_FUNC lk;
   if (!VH[dev_id]) return E_CON_NODEVICE;
   // notify subcribers about device removing
   sys_notifyexec(SECB_DEVDEL, dev_id);

   for (u32t ii=1; ii<=Sessions.Max(); ii++) {
      se_sysdata* se = (se_sysdata*)Sessions[ii];
      if (se) se->vs_devmask &= ~(1<<dev_id);
   }
   Vsn[dev_id] = 0;
   DELETE(VH[dev_id]);
   VH [dev_id] = 0;
   update_VHfree();
   // drop foreground flag from sessions, who lost it
   se_updforeflag();
   log_it(2, "device %u deleted\n", dev_id);

   return 0;
}

/// internal call for device substitution
qserr _std se_deviceswap(u32t dev_id, qs_vh np, qs_vh *op) {
   if (dev_id>=SYS_HANDLERS) return E_SYS_INVPARM;
   if (!op || !np) return E_SYS_ZEROPTR;

   MTLOCK_THIS_FUNC lk;
   if (!VH[dev_id]) return E_CON_NODEVICE;

   u32t id = exi_classid(np);
   // check # of methods
   if (!id) return E_CON_BADHANDLER;
   if (exi_methods(id)*sizeof(void*)!=sizeof(_qs_vh)) return E_CON_BADHANDLER;
   // query mode list on the new device
   vio_mode_info *mi = 0;
   np->info(&mi,0);

   if (mi) {
      for (u32t ii=1; ii<=Sessions.Max(); ii++) {
         se_sysdata* se = (se_sysdata*)Sessions[ii];
         if (se) {
            u32t        ok = 0, idx = 0;
            while (mi[idx].mx)
               if (mi[idx].mx==se->vs_x && mi[idx].my==se->vs_y) { ok=1; break; }
                  else idx++;
            if (!ok) { free(mi); return E_CON_NOSESMODE; }
         }
      }
      free(mi);
   }
   *op = VH[dev_id];
   VH[dev_id] = np;

   /* copy screen contents to the new device.
      if no active session - just use resetmode on it */
   u32t sesno = Vsn[dev_id];
   if (sesno) {
      se_sysdata* se = get_se(sesno);
      if (se) se_clone(se->vs_tb, np, SE_CLONE_MODE); else sesno = 0;
   }
   if (!sesno) np->reset();

   return 0;
}


extern "C"
qserr _std START_EXPORT(se_attach)(u32t sesno, u32t dev_id) {
   if (!sesno || dev_id>=SYS_HANDLERS) return E_SYS_INVPARM;
   MTLOCK_THIS_FUNC lk;
   if (!VH[dev_id]) return E_CON_NODEVICE;
   se_sysdata* se = get_se(sesno);
   if (!se) return E_CON_SESNO;
   // device already marked in session data
   if (se->vs_devmask&1<<dev_id) return E_SYS_INITED;

   vio_mode_info *ml = 0;
   int         found = 0;
   u32t       dflags = VH[dev_id]->info(&ml, 0);
   if (ml) {
      for (vio_mode_info *mp = ml; mp->mx; mp++)
         if (mp->mx==se->vs_x && mp->my==se->vs_y) { found = 1; break; }
      free(ml);
   } else
   if (dflags&VHI_ANYMODE) found = 1;

   if (!found) return E_CON_MODERR;

   se->vs_devmask|=1<<dev_id;
   log_it(0,"device %u added for session %u\n", dev_id, sesno);
   // force it to foreground if no active session
   if (!Vsn[dev_id]) se_switch(sesno, 1<<dev_id);

   return 0;
}

extern "C"
qserr _std START_EXPORT(se_detach)(u32t sesno, u32t dev_id) {
   if (!sesno || dev_id>=SYS_HANDLERS) return E_SYS_INVPARM;
   MTLOCK_THIS_FUNC lk;
   if (!VH[dev_id]) return E_CON_NODEVICE;
   se_sysdata* se = get_se(sesno);
   if (!se) return E_CON_SESNO;
   if ((se->vs_devmask&1<<dev_id)==0) return E_SYS_INVPARM;
   // drop device bit and search for another session on it
   se->vs_devmask &= ~(1<<dev_id);
   if (Vsn[dev_id]==sesno) se_switch(0, 1<<dev_id);

   log_it(0,"device %u removed from session %u\n", dev_id, sesno);

   return 0;
}

se_stats* _std se_deviceenum(void) {
   MTLOCK_THIS_FUNC lk;
   se_stats *rc = (se_stats*)malloc_local(sizeof(se_stats) + sizeof(rc->mhi)*VHfree);
   u32t     cnt = 0, mask = 0;
   for (u32t ii = 0; ii<VHfree; ii++) {
      if (VH[ii]) {
         rc->mhi[cnt].dev_id    = ii;
         rc->mhi[cnt].dev_flags = VH[ii]->info(0, rc->mhi[cnt].name);
         rc->mhi[cnt].reserved  = Vsn[ii];
         mask |= 1<<ii;
         cnt++;
      }
   }
   rc->handlers = cnt;
   rc->devmask  = mask;
   rc->title    = 0;
   rc->sess_no  = 0;
   rc->flags    = 0;
   rc->focus    = 0;
   rc->mx       = 0;
   rc->my       = 0;
   return rc;
}

/// a short form of device enumeration - return a bit list of existing devices.
u32t _std se_devicemask(void) {
   MTLOCK_THIS_FUNC lk;
   u32t res = 0;
   for (u32t idx=0; idx<VHfree; idx++)
      if (VH[idx]) res |= 1<<idx;
   return res;
}

extern "C"
qserr _std START_EXPORT(se_switch)(u32t sesno, u32t foremask) {
   if (!foremask) foremask = FFFF;
   MTLOCK_THIS_FUNC lk;
   se_sysdata *dse = sesno ? get_se(sesno) : 0;
   if (sesno && !dse) return E_CON_SESNO;
   // foremask should have at least one valid bit
   if (dse)
      if ((foremask&=dse->vs_devmask)==0) return E_SYS_INVPARM;

   for (u32t ii=0; ii<VHfree; ii++)
      if (VH[ii] && (foremask&1<<ii)) {
         u32t tses = sesno;
         // search for any session on device
         if (!tses) {
            u32t       eses = 0;
            se_sysdata *ese = 0;

            for (u32t si=1; si<=Sessions.Max(); si++) {
               se_sysdata* sd = (se_sysdata*)Sessions[si];
               if (sd && (sd->vs_devmask&1<<ii)) {
                  if (sd->vs_flags&VSF_NOLOOP) {
                     eses = si;
                     ese  = sd;
                  } else {
                     tses = si;
                     dse  = sd;
                     break;
                  }
               }
            }
            // error result (VSF_NOLOOP session, but no one else available)
            if (!tses && eses) { tses = eses; dse = ese; }
         }
         // known session number
         if (tses && Vsn[ii]!=tses) {
            u16t modeid = dse->vs_devdata[ii]?(u16t)dse->vs_devdata[ii]:SE_CLONE_MODE;
            qserr   res = se_clone(dse->vs_tb, VH[ii], modeid);
            if (res)
               log_it(0,"clone on dev %u failed = %08X (%ux%u)\n", ii, res,
                  dse->vs_x, dse->vs_y);
            else {
               log_it(3,"VH[%u] %u->%u\n", ii, Vsn[ii], tses);
               Vsn[ii] = tses;
            }
         } else
         if (!tses) Vsn[ii] = 0;
      }
   // update VSF_FOREGROUND for all sessions
   se_updforeflag();

   return 0;
}

extern "C"
qserr _std START_EXPORT(se_selectnext)(u32t device) {
   if (device>=SYS_HANDLERS) return E_SYS_INVPARM;
   MTLOCK_THIS_FUNC lk;
   if (!VH[device]) return E_CON_NODEVICE;
   // use "search any" code from the function above
   if (!Vsn[device]) return se_switch(0, 1<<device);

   u32t  sesno = Vsn[device], ii,
        sesnew = 0;

   for (ii=sesno+1; ii<=Sessions.Max(); ii++) {
      se_sysdata *sd = (se_sysdata*)Sessions[ii];
      if (sd && (sd->vs_flags&VSF_NOLOOP)==0 &&
         (sd->vs_devmask&1<<device)) { sesnew = ii; break; }
   }
   if (!sesnew)
      for (ii=1; ii<sesno; ii++) {
         se_sysdata *sd = (se_sysdata*)Sessions[ii];
         if (sd && (sd->vs_flags&VSF_NOLOOP)==0 &&
            (sd->vs_devmask&1<<device)) { sesnew = ii; break; }
      }
   if (!sesnew) return E_SYS_NOTFOUND;

   return se_switch(sesnew, 1<<device);
}

extern "C"
qserr _std START_EXPORT(se_clone)(qs_vh src, qs_vh dst, u16t modeid) {
   vh_settings  sb;
   u32t   vmx, vmy, dmx, dmy;
   qserr       res;
   u16t       *mem, dmid;
   if (!src || !dst) return E_SYS_ZEROPTR;
   res = src->getmode(&vmx, &vmy, 0);
   if (res) return res;
   res = dst->getmode(&dmx, &dmy, &dmid);
   if (res) return res;
   // resolution or mode id mismatch
   if (modeid)
      if (vmx!=dmx || vmy!=dmy || modeid!=SE_CLONE_MODE && modeid!=dmid) {
         res = modeid==SE_CLONE_MODE ? dst->setmode(vmx, vmy, 0) :
                                       dst->setmodeid(modeid);
         // check it again!
         if (!res) res = dst->getmode(&dmx, &dmy, 0);
         if (res) return res;
      }
   if (vmx!=dmx || vmy!=dmy) return E_CON_MODERR;

   // ignore errors & try to use memory() when it possible
   mem = dst->memory();
   if (mem)
      if (src->readbuf(0, 0, vmx, vmy, mem, vmx<<1)) mem = 0;
   if (!mem) {
      mem = src->memory();
      if (mem) {
         if (dst->writebuf(0, 0, vmx, vmy, mem, vmx<<1)) mem = 0;
      } else {
         mem = (u16t*)malloc(vmx*vmy<<1);
         res = src->readbuf(0, 0, vmx, vmy, mem, vmx<<1);
         if (!res) res = dst->writebuf(0, 0, vmx, vmy, mem, vmx<<1);
         free(mem);
         if (res) mem = 0;
      }
   }
   // something failed - just clears screen
   if (!mem) dst->clear();

   memset(&sb, 0, sizeof(sb));
   // set attrs to default if state function failed
   if (src->state(&sb)) {
      sb.shape = VIO_SHAPE_LINE;
      sb.color = VIO_COLOR_WHITE;
   }
   dst->setopts(VHSET_LINES|sb.ttylines);
   dst->setopts(sb.opts&(VHSET_WRAPX|VHSET_BLINK)|VHSET_SHAPE|sb.shape);
   dst->setpos(sb.py,sb.px);
   dst->setcolor(sb.color);

   return 0;
}

static int _std process_enum_callback(mt_prcdata *pd, void *usrdata) {
   u32t     *cnta = (u32t*)usrdata, ii;

   for (ii=0; ii<pd->piListAlloc; ii++) {
      mt_thrdata *th = pd->piList[ii];
      if (th && th->tiSession!=SESN_DETACHED) {
         se_sysdata *sd = 0;
         if (th->tiSession<=Sessions.Max()) sd = (se_sysdata*)Sessions[th->tiSession];
         /* calc session usage & minimal validation of used structs */
         if (!sd) {
            log_it(0,"pid %u tid %u - unknown session %u!\n", pd->piPID,
               th->tiTID, th->tiSession);
         } else
         if (sd->vs_sign!=SESDATA_SIGN) {
            log_it(0,"session %u data header is broken!\n", th->tiSession);
         } else
            cnta[th->tiSession]++;
      }
   }
   return 1;
}

static void se_release(se_sysdata *se) {
   DELETE(se->vs_focus);
   DELETE(se->vs_tb);
   se->vs_sign    = 0;
   se->vs_tb      = 0;
   se->vs_focus   = 0;
   se->vs_devmask = 0;
   if (se->vs_title) { free(se->vs_title); se->vs_title = 0; }
   memset(&se->vs_devdata, 0, sizeof(se->vs_devdata));
}

/** close session with zero threads in it.
    Called from se_newsession() and thread exit callback */
void _std se_closeempty() {
   if (!mtmux) return;
   mt_swlock();
   // zeroed array for session usage counter
   u32t *cnta = (u32t*)calloc(Sessions.Count(),sizeof(u32t)), ii;
   // enum all threads in system
   mtmux->enumpd(process_enum_callback, cnta);
   // delete unused session data
   for (ii=1; ii<=Sessions.Max(); ii++) {
      se_sysdata* sd = (se_sysdata*)Sessions[ii];
      if (sd && !cnta[ii]) {
         u32t mask = sd->vs_devmask;
         // disable all devices
         sd->vs_devmask = 0;
         // empty session is selected somewhere
         if (sd->vs_flags&VSF_FOREGROUND) se_switch(0,mask);
         Sessions[ii]   = 0;
         se_release(sd);
         // session exit notification
         sys_notifyexec(SECB_SEEXIT, ii);
      }
   }
   // unlock MT and then free
   mt_swunlock();
   free(cnta);
}

static char *copy_title(const char *title) {
   if (!title) return 0;
   char *rc = strdup(title), *rp = rc;
   mem_modblock(rc);
   // fix string from invalid characters (and end of lines especially)
   while (*rp) {
      if (*rp<' '||*rp==0x7F) *rp = 0;
      rp++;
   }
   return rc;
}

qserr _std se_newsession(u32t devices, u32t foremask, const char *title, u32t flags) {
   if (!in_mtmode) return E_MT_DISABLED;
   if (flags&~(VSF_NOLOOP|VSF_HIDDEN)) return E_SYS_INVPARM;
   MTLOCK_THIS_FUNC lk;
   mt_thrdata      *th;
   mt_getdata(0,&th);
   if (th->tiTID==1 && th->tiSession==SESN_DETACHED) return E_CON_DETACHED;
   // check device bits
   if (devices && (devices&se_devicemask())!=devices) return E_CON_NODEVICE;
   // search for the first zero entry
   int        sesno = Sessions.IndexOf(0);
   se_sysdata  *cse = get_se(se_sesno()),
               *nse = newslot();
   nse->vs_selfno   = sesno<0 ? Sessions.Count() : sesno;
   nse->vs_devmask  = devices ? devices : cse->vs_devmask;
   nse->vs_flags    = flags;
   nse->vs_x        = devices ? 80 : cse->vs_x;
   nse->vs_y        = devices ? 25 : cse->vs_y;
   if (title)
      nse->vs_title = copy_title(title);
   // clone custom video modes
   if (!devices)
      for (u32t ii=0; ii<VHfree; ii++)
         if (1<<ii & nse->vs_devmask) nse->vs_devdata[ii] = cse->vs_devdata[ii];
   // set buffer`s video mode
   nse->vs_tb->setmode(nse->vs_x, nse->vs_y, 0);
   // insert it into list - session is ready
   if (sesno<0) Sessions.Add(nse); else Sessions[sesno] = nse;
   // new session number (update mt_exechooks because we`re the current thread!)
   th->tiSession = nse->vs_selfno;
   mt_exechooks.mtcb_sesno = nse->vs_selfno;
   // system notification
   sys_notifyexec(SECB_SESTART, nse->vs_selfno);
   // enum sessions and close which one, who has no threads in it
   se_closeempty();

   if (foremask) {
      qserr rc = se_switch(nse->vs_selfno, foremask);
      // print error, but ignore it
      if (rc) log_it(0,"session %u gofore(%03X) err %08X\n", nse->vs_selfno,
         foremask, rc);
   }
   return 0;
}

qserr _std se_settitle(u32t sesno, const char *title) {
   if (sesno==SESN_DETACHED) return E_CON_DETACHED;
   MTLOCK_THIS_FUNC lk;
   se_sysdata *se = get_se(sesno);
   if (!se) return E_CON_SESNO;
   char   *ntitle = title ? copy_title(title) : 0;
   /* compare titles *after* convert.
      this should be done because user (cmd.exe especially) may annoy with
      too often calls and every one of them cause system notification below */
   if (ntitle && se->vs_title)
      if (strcmp(ntitle,se->vs_title)==0) {
         free(ntitle);
         return 0;
      }
   if (se->vs_title) free(se->vs_title);
   se->vs_title = ntitle;
   // notify subscribers
   sys_notifyexec(SECB_SETITLE, sesno);
   return 0;
}

qserr _std se_setstate(u32t sesno, u32t mask, u32t values) {
   if (sesno==SESN_DETACHED) return E_CON_DETACHED;
   if ((mask&~(VSF_NOLOOP|VSF_HIDDEN)) || !mask) return E_SYS_INVPARM;
   MTLOCK_THIS_FUNC lk;
   se_sysdata *se = get_se(sesno);
   if (!se) return E_CON_SESNO;
   se->vs_flags = se->vs_flags&~mask | values&mask;
   return 0;
}

se_stats** _std se_enum(u32t devmask) {
   if (!devmask) devmask = FFFF;

   MTLOCK_THIS_FUNC lk;
   u32t  allocsz = Sessions.Count()*sizeof(se_stats*), pos, ii;
   se_stats **rc = (se_stats**)malloc_local(allocsz);
   if (!rc) return 0;
   // dumb method - just call se_stat() one by one and then copy results
   for (ii=1, pos=0; ii<=Sessions.Max(); ii++) {
      se_sysdata *se = (se_sysdata*)Sessions[ii];
      if (se)
         if ((se->vs_devmask&devmask) || !se->vs_devmask && devmask==FFFF) {
            se_stats *st = se_stat(ii);
            if (st)
               if (st->flags&VSF_HIDDEN) free(st); else rc[pos++] = st;
         }
   }
   if (!pos) {
      free(rc);
      return 0;
   }
   // collect block length
   u32t *ilen = new u32t[pos], addsz = 0;
   for (ii=0; ii<pos; ii++) addsz += ilen[ii] = mem_blocksize(rc[ii]);

   se_stats **nrc = (se_stats**)realloc(rc, allocsz+addsz);
   if (nrc) {
      u8t *dst = (u8t*)nrc + allocsz;
      for (ii=0; ii<pos; ii++) {
         memcpy(dst, nrc[ii], ilen[ii]);
         // update title pointer (it MUST points into the source block)
         if (nrc[ii]->title)
            ((se_stats*)dst)->title = (char*)dst + (nrc[ii]->title - (char*)nrc[ii]);
         free(nrc[ii]);
         nrc[ii] = (se_stats*)dst;
         dst    += ilen[ii];
      }
      nrc[pos] = 0;
   } else {
      // realloc failed?? should never occur
      for (ii=0; ii<pos; ii++) free(rc[ii]);
      free(rc);
   }
   delete ilen;
   return nrc;
}

/* set/unset current process as Ctrl-C focus app for the current session.
   Function called from the main thread`s final exit hook. */
qserr _std se_sigfocus(u32t focus) {
   MTLOCK_THIS_FUNC lk;
   u32t            res = 0;
   mt_prcdata      *pd;
   mt_thrdata     *thm;
   // get the main thread
   mt_getdata(&pd, 0);
   thm = pd->piList[0];

   if (thm->tiSession==SESN_DETACHED) res = E_CON_DETACHED; else
   if (thm->tiSession!=se_sesno()) res = E_SYS_ACCESS; else {
      se_sysdata *se = get_se(thm->tiSession);

      if (!se->vs_focus->delvalue(pd->piPID))
         if (!focus) res = E_SYS_NOTFOUND;
      if (focus) se->vs_focus->add(pd->piPID);
   }
   return res;
}

/** return qs_viobuf instance with copy of session screen.
    returned instance should be DELETEd, zero returned on incorrect
    session number */
qs_vh _std se_screenshot(u32t sesno) {
   if (sesno==SESN_DETACHED) return 0;
   MTLOCK_THIS_FUNC lk;
   se_sysdata *se = get_se(sesno);
   if (!se) return 0;

   qs_vh rc = new_handler(0, QS_VH_BUFFER, 0, 0, 1);
   if (!rc) return 0;
   if (se_clone(se->vs_tb, rc, SE_CLONE_MODE)==0) return rc;
   DELETE(rc);
   return 0;
}

#define HOTKEY_STATUS_BITS (KEY_SHIFT|KEY_ALT|KEY_CTRL)

static int _std log_hotkey(u16t key, u32t status, u32t device) {
   u32t sv = (status&HOTKEY_STATUS_BITS)<<16 | key;
   // log_printf("key %04X state: %08X\n", key, status);

   MTLOCK_THIS_FUNC lk;
   if (hk_codes) {
      u32t *pos = memchrd(hk_codes->Value(), sv, hk_codes->Count());
      if (pos) {
         u32 index = pos - hk_codes->Value();
         run_notify((sys_eventcb)((*hk_funcs)[index]), SECB_HOTKEY, sv,
            device, 0, (*hk_pid)[index]);
         return 1;
      }
   }
   // default old processing
   u8t kh = key>>8;
   if ((status&(KEY_CTRL|KEY_ALT))==(KEY_CTRL|KEY_ALT)) {
      switch (kh) {
         // Ctrl-Alt-F1: vt100 terminal
         case 0x3B: case 0x5E: case 0x68: {
            qserr res = hlp_serconsole(dbport_device?0:se_foreground());
            if (res) {
               char *emsg = cmd_shellerrmsg(EMSG_QS, res);
               log_it(1,"c-a-f1 error %08X: %s\n", res, emsg);
               free(emsg);
            }
            return 1;
         }
         // Ctrl-Alt-F2: session list
         case 0x3C: case 0x5F: case 0x69: log_sedump(0); return 1;
         // Ctrl-Alt-F3: class list
         case 0x3D: case 0x60: case 0x6A: exi_dumpall(0); return 1;
         // Ctrl-Alt-F4: file handles
         case 0x3E: case 0x61: case 0x6B:
            log_ftdump(0); io_dumpsft(0);
            return 1;
         // Ctrl-Alt-F5: process tree
         case 0x3F: case 0x62: case 0x6C: mod_dumptree(0); return 1;
         // Ctrl-Alt-F6: gdt dump
         case 0x40: case 0x63: case 0x6D: sys_gdtdump(0); return 1;
         // Ctrl-Alt-F7: idt dump
         case 0x41: case 0x64: case 0x6E: sys_idtdump(0); return 1;
         // Ctrl-Alt-F8: page tables dump
         case 0x42: case 0x65: case 0x6F: pag_printall(0); return 1;
         // Ctrl-Alt-F9: dump pci config space
         case 0x43: case 0x66: case 0x70: log_pcidump(0); return 1;
         // Ctrl-Alt-F10: dump module table
         case 0x44: case 0x67: case 0x71: log_mdtdump(0); return 1;
         // Ctrl-Alt-F11: dump main memory table
         case 0x85: case 0x89: case 0x8B:
            hlp_memcprint(0); hlp_memprint(0);
            return 1;
         // Ctrl-Alt-F12: dump memory log
         case 0x86: case 0x8A: case 0x8C:
            mem_dumplog(0, "User request");
            return 1;
      }
   }
   return 0;
}

void sys_hotkeyfree(u32t pid) {
   if (!pid) return;
   mt_swlock();
   if (hk_funcs)
      for (u32t ii=0; ii<hk_funcs->Count(); ii++)
         if ((*hk_pid)[ii]==pid) {
            hk_funcs->Delete(ii);
            hk_codes->Delete(ii);
            hk_pid  ->Delete(ii);
         }
   mt_swunlock();
}

qserr sys_modifyhotkey(sys_eventcb cbfunc, u32t code, u32t pid = 0) {
   MTLOCK_THIS_FUNC lk;
   if (code) {
      u32t *pos = 0;
      if (!hk_funcs) {
         hk_funcs = new TPtrList;
         hk_codes = new TUList;
         hk_pid   = new TUList;
      }
      if (hk_codes->Count())
         pos = memchrd(hk_codes->Value(), code, hk_codes->Count());
      if (!pos) {
         hk_funcs->Add(cbfunc);
         hk_codes->Add(code);
         hk_pid  ->Add(pid);
      } else {
         u32t index = pos - hk_codes->Value();
         (*hk_funcs)[index] = cbfunc;
         (*hk_pid)  [index] = pid;
      }
      return 0;
   } else {
      if (!hk_funcs) return E_SYS_NOTFOUND;
      u32t counter = 0;
      // delete all entries for this function
      while (hk_funcs->Count()) {
         u32t  *pos = memchrd((u32t*)hk_funcs->Value(), (u32t)cbfunc, hk_funcs->Count());
         if (!pos) break;
         u32t   idx = pos - (u32t*)hk_funcs->Value();
         hk_funcs->Delete(idx); hk_codes->Delete(idx); hk_pid->Delete(idx);
         counter++;
      }
      return counter?0:E_SYS_NOTFOUND;
   }
}

qserr _std sys_sethotkey(u16t code, u32t statuskeys, u32t flags, sys_eventcb cbfunc) {
   if (!cbfunc) return E_SYS_ZEROPTR; else
   if ((flags&~(SECB_THREAD|SECB_GLOBAL)) || (statuskeys&~HOTKEY_STATUS_BITS))
      return E_SYS_INVPARM;
   else {
      qserr rc;
      if (!code) rc = sys_modifyhotkey(cbfunc, 0); else
         rc = sys_modifyhotkey(cbfunc, statuskeys<<16|code, flags&SECB_THREAD?
            (flags&SECB_GLOBAL?1:mod_getpid()):0);
      return rc;
   }
}

/// shutdown handler.
static void _std shutdown_handler(sys_eventinfo *info) {
   u32t  ii,
       esno = se_sesno(); // session, who handling shutdown
   // delete all devices except 0
   for (ii=1; ii<VHfree; ii++) se_devicedel(ii);
   // force 80x25 on screen in any case
   if (esno==SESN_DETACHED) VH[0]->reset(); else {
      se_sysdata* se = get_se(esno);
      vio_resetmode();
      if ((se->vs_devmask&1)==0) se_attach(esno, 0);
      if (Vsn[0]!=esno) se_switch(esno, 1);
   }
}

qserr _std hlp_serconsole(u32t sesno) {
   if (dbport_device && sesno) return E_SYS_INITED;
   if (sesno) {
      u32t rate;
      u16t port = hlp_seroutinfo(&rate);
      if (!port) return E_SYS_NONINITOBJ;

      MTLOCK_THIS_FUNC lk;
      se_sysdata *se = get_se(sesno);
      if (!se) return E_CON_SESNO;
      // only 80x25
      if (se->vs_x!=80 && se->vs_y!=25) return E_CON_MODERR;

      char setup[32];
      snprintf(setup, 32, "%u;%u", port, rate);

      qserr res = se_deviceadd("qs_vh_tty", &dbport_device, setup);
      if (!res) {
         res = se_attach(sesno, dbport_device);
         if (!res) res = se_switch(sesno, 1<<dbport_device);
         // delete device if failed
         if (res) se_devicedel(dbport_device);
      }
      if (res) dbport_device = 0;
      return res;
   } else
   if (!dbport_device) return E_SYS_NONINITOBJ; else {
      qserr res = se_devicedel(dbport_device);
      if (!res) dbport_device = 0;
      return res;
   }
}

void _std log_sedump(printf_function pfn) {
   process_context* pq = mod_context();
   if (pfn==0) pfn = log_printf;
   pfn("== Session List ==\n");
   mt_swlock();
   if (ses_list)
      for (u32t ii=0; ii<=Sessions.Max(); ii++) {
         se_sysdata* se = get_se(ii);
         if (se) {
            u32t fl = se->vs_flags;
            pfn("No %2u. %ux%u >> dev:%03X tb:%08X:%08X %s%s%s [%s]\n", ii,
               se->vs_x, se->vs_y, se->vs_devmask, se->vs_tb, se->vs_tb->memory(),
                  fl&VSF_FOREGROUND?"FG ":"", fl&VSF_NOLOOP?"NOSW ":"",
                     fl&VSF_HIDDEN?"HID ":"", se->vs_title?se->vs_title:"");
            if (se->vs_flags&VSF_FOREGROUND)
               for (u32t idx=0; idx<VHfree; idx++)
                  if (VH[idx] && Vsn[idx]==se->vs_selfno) {
                     char prnname[32];
                     u32t    caps = VH[idx]->info(0, prnname);
                     pfn("       vh%u: %-16s %s (%04X)\n", idx, prnname,
                        VH[idx]->input()?"in/out":" out  ", caps);
                  }

            if (se->vs_keybuf[se->vs_keyrp]) {
               static u32t kb_copy[SESDATA_KBSIZE];
               u32t  len = SESDATA_KBSIZE - se->vs_keyrp;
               memcpy(&kb_copy[0], &se->vs_keybuf[se->vs_keyrp], len<<2);
               if (len<SESDATA_KBSIZE) {
                  len = se->vs_keyrp;
                  memcpy(&kb_copy[len], &se->vs_keybuf, len<<2);
               }
               pfn("       kb: %16lb\n", &kb_copy);
            }
         }
      }
   mt_swunlock();
   pfn("==================\n");
}

extern "C" void exi_register_vio(void);
void exi_register_vh_vio(void);

extern "C"
void setup_sessions(void) {
   // register classes
   exi_register_vio();
   exi_register_vh_vio();
   // even if BSS should be zero, we just wipe it again for safety ;)
   memset(&VH, 0, sizeof(VH));
   memset(&Vsn, 0, sizeof(Vsn));
   // vio handler
   VH [0] = new_handler(0, 0, vh_vio_classid);
   Vsn[0] = 1;
   VHfree = 1;
   VHslow = VH[0]->info(0,0)&VHI_SLOW ? 1 : 0;
   // create session list
   ses_list = new TPtrList;
   // detached
   se_sysdata *se = newslot();
   se->vs_selfno  = 0;
   Sessions.Add(se);
   // boot session
   se = newslot();
   se->vs_selfno  = 1;
   se->vs_devmask = 1;
   se->vs_flags   = VSF_FOREGROUND/*|VSF_INTENSITY*/;
   // sync buffer with current screen contents
   se_clone(VH[0], se->vs_tb, SE_CLONE_MODE);
   se->vs_tb->getmode(&se->vs_x, &se->vs_y, 0);
   Sessions.Add(se);
   // switch vio output to common mode
   VHTable[VHI_BVIO]   = &bvio;
   VHTable[VHI_ACTIVE] = &bvio;
   //se_settitle(1, "Boot session");
   // install shutdown handler
   sys_notifyevent(SECB_QSEXIT|SECB_GLOBAL, shutdown_handler);
}
