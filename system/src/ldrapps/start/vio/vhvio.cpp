//
// QSINIT "start" module
// vio handler : common text modes support
//
#include "qcl/sys/qsvioint.h"
#include "qsutil.h"
#include "efnlist.h"
#include "qecall.h"
#include "qserr.h"
#include "syslocal.h"
#include "qsvdata.h"

#define VIOH_SIGN        0x48564956          // VIVH
#define MODELIST_SZ               4

typedef struct {
   u32t           sign;
   u16t           opts;
   qs_kh          link;
   qs_vh          self;
   u32t            *ml;
   u32t           mcnt;
} vhi_data;

typedef struct {
   u32t           sign;
   qs_vh          link;
} khi_data;

#define instance_ret(inst,err)       \
   vhi_data *inst = (vhi_data*)data; \
   if (inst->sign!=VIOH_SIGN) return err;

#define instance_void(inst)          \
   vhi_data *inst = (vhi_data*)data; \
   if (inst->sign!=VIOH_SIGN) return;

#define kh_instance_ret(inst,err)    \
   khi_data *inst = (khi_data*)data; \
   if (inst->sign!=VIOH_SIGN) return err;

#define kh_instance_void(inst)       \
   khi_data *inst = (khi_data*)data; \
   if (inst->sign!=VIOH_SIGN) return;

u32t      vh_vio_classid = 0,
          kh_vio_classid = 0;
static vio_handler *cvio = 0;

static qserr _exicc vh_vio_init(EXI_DATA, const char *setup) {
   // arg should be empty
   if (setup && *setup) return E_SYS_INVPARM;
   instance_ret(td,E_SYS_INVOBJECT);
   qserr rc = 0;

   /* create & initialize keyboard pair. */
   td->link = (qs_kh)exi_createid(kh_vio_classid, EXIF_SHARED);
   if (!td->link) {
      log_it(0, "unable to init kh_vio pair!\n");
      rc = E_SYS_SOFTFAULT;
   } else {
      rc = td->link->init(setup);
      if (rc) {
         log_it(0, "kh_vio init err %0X!\n", rc);
         DELETE(td->link);
         td->link = 0;
      } else {
         khi_data *kd = (khi_data*)exi_instdata(td->link);
         kd->link = td->self;

         if (hlp_hosttype()==QSHT_EFI) {
            struct console_mode *cml;
            s32t res = call64(EFN_VIOENUMMODES, 0, 1, &cml);
            // add at least 80x25 in case of error
            if (res<=0) {
               log_it(0, "EFI mode list err?\n");
               td->ml    = new u32t[2];
               td->ml[0] = 80<<16|25;
               td->ml[1] = 0;
               td->mcnt  = 1;
            } else {
               td->ml    = new u32t[res+1];
               for (u32t ii=0; ii<res; ii++)
                  td->ml[ii] = cml[ii].conmode_x<<16|cml[ii].conmode_y;
               td->ml[res] = 0;
               td->mcnt    = res;
            }
            // print it into log
            char fmt[16];
            snprintf(fmt, 16, "vio ml: %%%ulb\n", td->mcnt);
            log_it(3, fmt, td->ml);
         } else {
            u32t *lp = new u32t[MODELIST_SZ];
            td->ml   = lp;
            td->mcnt = MODELIST_SZ-1;
            *lp++ = 80<<16|25;
            *lp++ = 80<<16|43;
            *lp++ = 80<<16|50;
            *lp++ = 0;
         }
      }
   }
   return rc;
}

static qs_kh _exicc vh_vio_link(EXI_DATA) {
   instance_ret(td,0);
   return td->link;
}

static qserr _exicc vh_vio_setopts(EXI_DATA, u32t opts) {
   instance_ret(vd,E_SYS_INVOBJECT);
   // line counter setup call
   if (opts&VHSET_LINES) {
      cvio->vh_ttylines(opts & x7FFF);
      return 0;
   }
   // shape settings
   if (opts&VHSET_SHAPE) {
      cvio->vh_setshape(opts&0xFFFF);
      opts &= ~(VHSET_SHAPE|0xFFFF);
   }
   // and then validate remains
   if (opts&~(VHSET_WRAPX|VHSET_BLINK)) return E_SYS_INVPARM;

   if (opts&VHSET_BLINK^vd->opts&VHSET_BLINK)
      cvio->vh_intensity(opts&VHSET_BLINK?0:1);

   if (opts&VHSET_WRAPX^vd->opts&VHSET_WRAPX) {
   }
   vd->opts = opts;
   return 0;
}

static qserr _exicc vh_vio_setmode(EXI_DATA, u32t cols, u32t lines, void *extmem) {
   instance_ret(vd, E_SYS_INVOBJECT);
   if (extmem) return E_SYS_INVPARM;
   // quick search in the mode list
   if (memchrd(vd->ml, cols<<16|lines&0xFFFF, vd->mcnt)==0) return E_CON_MODERR;
   return cvio->vh_setmodeex(cols,lines) ? 0 : E_SYS_SOFTFAULT;
}

static qserr _exicc vh_vio_setmodeid(EXI_DATA, u16t mode_id) {
   instance_ret(vd, E_SYS_INVOBJECT);
   if (--mode_id>=vd->mcnt) return E_CON_BADMODEID;
   u32t mode = vd->ml[mode_id];
   return vh_vio_setmode(data, 0, mode>>16, mode&0xFFFF, 0);
}

static qserr _exicc vh_vio_reset(EXI_DATA) {
   cvio->vh_resetmode();
   return 0;
}

static qserr _exicc vh_vio_getmode(EXI_DATA, u32t *modex, u32t *modey, u16t *modeid) {
   instance_ret(vd,E_SYS_INVOBJECT);
   u32t mx, my;

   cvio->vh_getmfast(&mx, &my);
   if (modex) *modex = mx;
   if (modey) *modey = my;
   if (modeid) {
      u32t *pos = memchrd(vd->ml, mx<<16|my, vd->mcnt);
      *modeid = pos?(pos-vd->ml)+1:0;
   }
   return 0;
}

static void  _exicc vh_vio_setcolor(EXI_DATA, u16t color) {
   cvio->vh_setcolor(color);
}

static void _exicc vh_vio_clear(EXI_DATA) { cvio->vh_clearscr(); }

static void  _exicc vh_vio_setpos(EXI_DATA, u32t line, u32t col) {
   cvio->vh_setpos(line, col);
}

static u32t _exicc vh_vio_info(EXI_DATA, vio_mode_info **modes, char *prnname) {
   instance_ret(vd,0);
   if (modes) {
      // zero-filled heap block, owned by START module (i.e. permanent)
      vio_mode_info *mi = (vio_mode_info*)calloc(vd->mcnt+1, sizeof(vio_mode_info));
      *modes = mi;

      for (u32t ii=0; ii<vd->mcnt; ii++) {
         mi[ii].size    = VIO_MI_FULL;
         mi[ii].mx      = vd->ml[ii]>>16;
         mi[ii].my      = vd->ml[ii]&0xFFFF;
         if (mi[ii].mx==80 && mi[ii].my==25) { mi[ii].fontx=9; mi[ii].fonty=16; }
            else { mi[ii].fontx=8; mi[ii].fonty=8; }
         mi[ii].mode_id = ii+1;
      }
   }
   if (prnname) strcpy(prnname, "vio");
   /* return slow flag on EFI host - this makes Tetris playable in native
      EFI console */
   return VHI_MEMORY|VHI_BLINK|(hlp_hosttype()==QSHT_EFI?VHI_SLOW:0);
}

static qserr _exicc vh_vio_state(EXI_DATA, vh_settings *sb) {
   if (!sb) return E_SYS_ZEROPTR;
   instance_ret(vd,E_SYS_INVOBJECT);
   // fill values
   sb->color    = cvio->vh_getcolor();
   sb->shape    = cvio->vh_getshape();
   cvio->vh_getpos((u32t*)&sb->py, (u32t*)&sb->px);
   sb->opts     = vd->opts;
   sb->ttylines = cvio->vh_ttylines(-1);
   return 0;
}

static u16t* _exicc vh_vio_memory(EXI_DATA) { return 0; }
static qs_gh _exicc vh_vio_graphic(EXI_DATA) { return 0; }

static u32t _exicc vh_vio_charout(EXI_DATA, char ch) {
   return cvio->vh_charout(ch);
}

static u32t _exicc vh_vio_strout(EXI_DATA, const char *str) {
   if (!str) return 0;
   return cvio->vh_strout(str);
}

static qserr _exicc vh_vio_writebuf(EXI_DATA, u32t col, u32t line, u32t width, u32t height, void *buf, u32t pitch) {
   if (!width || !height) return E_SYS_INVPARM;
   if (!buf) return E_SYS_ZEROPTR;
   cvio->vh_writebuf(col, line, width, height, buf, pitch);
   return 0;
}

static qserr _exicc vh_vio_readbuf(EXI_DATA, u32t col, u32t line, u32t width, u32t height, void *buf, u32t pitch) {
   if (!width || !height) return E_SYS_INVPARM;
   if (!buf) return E_SYS_ZEROPTR;
   cvio->vh_readbuf(col, line, width, height, buf, pitch);
   return 0;
}

static void _exicc vh_vio_fillrect(EXI_DATA, u32t col, u32t line, u32t width, u32t height, char ch, u16t color) {
   instance_void(vd);
   if (!width || !height) return;
   if (!ch) ch = ' ';
   // alloc dynamic buffer
   u16t *vb = new u16t[width*height];
   memsetw(vb, color<<8|ch, width*height);
   vh_vio_writebuf(data, 0, col, line, width, height, vb, width<<1);
   delete vb;
}

static void *vh_methods_list[] = { vh_vio_init, vh_vio_link, vh_vio_graphic,
   vh_vio_info, vh_vio_setmode, vh_vio_setmodeid, vh_vio_reset, vh_vio_clear,
   vh_vio_getmode, vh_vio_setcolor, vh_vio_setpos, vh_vio_setopts, vh_vio_state,
   vh_vio_charout, vh_vio_strout, vh_vio_writebuf, vh_vio_readbuf, vh_vio_memory,
   vh_vio_fillrect };
   
static void _std vh_vio_initialize(void *instance, void *data) {
   vhi_data *vd = (vhi_data*)data;
   // other fields zeroed by the caller
   vd->sign     = VIOH_SIGN;
   vd->self     = (qs_vh)instance;
}

// should never be called
static void _std vh_vio_finalize(void *instance, void *data) {
   instance_void(td);
   td->sign   = 0;
   if (td->ml) { delete td->ml; td->ml=0; }
}

/** qs_kh_vio *********************************************/

static qserr _exicc kh_vio_init(EXI_DATA, const char *setup) { return 0; }

static void* _exicc kh_vio_link(EXI_DATA) {
   kh_instance_ret(kd,0);
   return kd->link;
}

static u16t _exicc kh_vio_read(EXI_DATA, int wait, u32t *status) { 
   u16t rc = wait?cvio->vh_kread():cvio->vh_kwaitex(0,status);
   if (wait && status && rc) *status = cvio->vh_kstatus();
   // log_it(2,"kh_vio_read(%i,%08X) = %04X %04X\n", wait, status, rc, status?*status:0);
   return rc;
}

static u32t _exicc kh_vio_status(EXI_DATA) { return cvio->vh_kstatus(); }

static void *kh_methods_list[] = { kh_vio_init, kh_vio_link, kh_vio_read, kh_vio_status };

static void _std kh_vio_initialize(void *instance, void *data) {
   khi_data *kd = (khi_data*)data;
   kd->sign     = VIOH_SIGN;
}

static void _std kh_vio_finalize(void *instance, void *data) {
   kh_instance_void(kd);
   kd->sign   = 0;
}

void exi_register_vh_vio(void) {
   cvio = VHTable[VHI_CVIO];

   if (sizeof(vh_methods_list)!=sizeof(_qs_vh) || sizeof(kh_methods_list)!=sizeof(_qs_kh))
      vio_msgbox("SYSTEM WARNING!", "vhvio: Fix me!", MSG_LIGHTRED|MSG_OK, 0);
   vh_vio_classid = exi_register("qs_vh_vio", vh_methods_list, sizeof(vh_methods_list)/
      sizeof(void*), sizeof(vhi_data), EXIC_EXCLUSIVE, vh_vio_initialize, vh_vio_finalize, 0);
   kh_vio_classid = exi_register("qs_kh_vio", kh_methods_list, sizeof(kh_methods_list)/
      sizeof(void*), sizeof(khi_data), EXIC_PRIVATE, kh_vio_initialize, kh_vio_finalize, 0);
}
