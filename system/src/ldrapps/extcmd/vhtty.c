//
// QSINIT "extcmd" module
// vio handler : vt100 com port output
//
#include "qcl/sys/qsvioint.h"
#include "qcl/qslist.h"
#include "qsutil.h"
#include "stdlib.h"
#include "vioext.h"
#include "qserr.h"
#include "qscon.h"

#define TTYH_SIGN        0x48595454          // TTYH

typedef struct {
   u32t           sign;
   void _std (*charout)(u16t port, u8t ch);
   u32t          width;
   qs_kh          link;
   qs_vh          self;
   u16t          color;
   u16t           opts;
   u16t           port;
} tty_data;

typedef struct {
   u32t           sign;
   u8t _std  (*charin )(u16t port);
   qs_vh          link;
   u16t           port;
   u32t           bpos;
   char       escb[16];
} kh_data;

static dd_list portlist = 0;

#define instance_ret(inst,err)       \
   tty_data *inst = (tty_data*)data; \
   if (inst->sign!=TTYH_SIGN) return err;

#define instance_void(inst)          \
   tty_data *inst = (tty_data*)data; \
   if (inst->sign!=TTYH_SIGN) return;

#define v_setcolor(col) vh_tty_setcolor(data,0,col)

#define kh_instance_ret(inst,err)    \
   kh_data *inst = (kh_data*)data;   \
   if (inst->sign!=TTYH_SIGN) return err;

#define kh_instance_void(inst)       \
   kh_data *inst = (kh_data*)data;   \
   if (inst->sign!=TTYH_SIGN) return;

static u32t   vh_classid = 0,
              kh_classid = 0;

static void tty_strout(tty_data *td, const char *str);

static void tty_charout(tty_data *td, char ch) {
   static const char *mtx = " ...............><......^\xFB\xAF\xAE..^\xFB";
   switch (ch) {
      case   9: tty_strout(td, "    "); break;
      case 127: td->charout(td->port, '.'); break;
      case  10: td->charout(td->port, 13);
      case  13:
      case  27: td->charout(td->port, ch); break;
      default :
         td->charout(td->port, ch<0x20?mtx[ch]:ch);
   }
}

static void tty_strout(tty_data *td, const char *str) {
   if (str)
      while (*str) tty_charout(td, *str++);
}

static qserr parse_setup(const char *setup, u32t *port, u32t *rate) {
   qserr res = 0;
   str_list *args = str_split(setup,";");
   if (args->count>2) res = E_SYS_INVPARM; else {
      *port = str2ulong(args->item[0]);
      *rate = args->count==2?str2ulong(args->item[1]):0;
      if (!*rate || *rate>115200) *rate = 115200;

      if (!*port) res = E_SYS_INVPARM;
   }
   free(args);
   return res;
}

static qserr _exicc vh_tty_init(EXI_DATA, const char *setup) {
   u32t    port, rate;
   qserr     rc;
   instance_ret(td, E_SYS_INVOBJECT);

   if (!setup) return E_SYS_ZEROPTR;
   rc = parse_setup(setup, &port, &rate);

   if (!portlist) portlist = NEW_G(dd_list);
   if (portlist->indexof(port,0)>=0) rc = E_CON_DUPDEVICE;

   if (!rc)
      if (!hlp_serialset(port,rate)) rc = E_SYS_INVPARM; else td->port = port;
   if (rc) return rc;

   /* create & initialize keyboard pair. */
   td->link = (qs_kh)exi_createid(kh_classid, EXIF_SHARED);
   if (!td->link) {
      log_it(0, "unable to init kh_tty pair!\n");
      rc = E_SYS_SOFTFAULT;
   } else {
      rc = td->link->init(0);
      if (rc) {
         log_it(0, "kh_tty init err %0X!\n", rc);
         DELETE(td->link);
         td->link = 0;
      } else {
         kh_data *kd = (kh_data*)exi_instdata(td->link);
         kd->link = td->self;
         kd->port = td->port;
         portlist->add(port);
      }
   }
   return rc;
}

static qs_kh _exicc vh_tty_link(EXI_DATA) {
   instance_ret(td,0);
   return td->link;
}

static qserr _exicc vh_tty_setopts(EXI_DATA, u32t opts) {
   instance_ret(td, E_SYS_INVOBJECT);
   // line counter just not supported by this implementation
   if (opts&VHSET_LINES) return 0;
   // just drop shape settings
   if (opts&VHSET_SHAPE) opts &= ~(VHSET_SHAPE|0xFFFF);
   // and then validate remains
   if (opts&~(VHSET_WRAPX|VHSET_BLINK)) return E_SYS_INVPARM;

   if (td->opts&VHSET_WRAPX^opts&VHSET_WRAPX)
      tty_strout(td, opts&VHSET_WRAPX?"\x1B[?7h":"\x1B[?7l");

   td->opts = opts;
   return 0;
}

static void vh_resetmode(tty_data *td) {
   //se_sysdata* se = get_se(se_sesno());
   tty_strout(td, "\x1B[0m\x1B[2J\x1B[1;1H");
   td->color = VIO_COLOR_WHITE;
   // drop options
   if (td->opts) vh_tty_setopts(td,0,0);
}

static qserr _exicc vh_tty_setmode(EXI_DATA, u32t cols, u32t lines, void *extmem) {
   instance_ret(td, E_SYS_INVOBJECT);

   if (cols>132 || lines!=25) return E_CON_MODERR;
   if (extmem) return E_SYS_INVPARM;
   td->width = cols>80?132:80;

   tty_strout(td, td->width==132?"\x1B[?3h":"\x1B[?3l");
   vh_resetmode(td);
   return 0;
}

static qserr _exicc vh_tty_setmodeid(EXI_DATA, u16t modeid) {
   instance_ret(vd, E_SYS_INVOBJECT);
   if (!modeid || modeid>2) return E_CON_BADMODEID;
   return vh_tty_setmode(data, 0, modeid==1?80:132, 25, 0);
}


static qserr _exicc vh_tty_reset(EXI_DATA) {
   instance_ret(td, E_SYS_INVOBJECT);
   if (td->width!=80) 
      return vh_tty_setmode(data, 0, 80, 25, 0);

   vh_resetmode(td);
   return 0;
}

static qserr _exicc vh_tty_getmode(EXI_DATA, u32t *modex, u32t *modey, u16t *modeid) {
   instance_ret(td, E_SYS_INVOBJECT);

   if (modex) *modex = td->width;
   if (modey) *modey = 25;
   if (modeid) *modeid = td->width==80?1:2;
   return 0;
}

static void  _exicc vh_tty_setcolor(EXI_DATA, u16t color) {
   static u8t col_rec[8] = { 0, 4, 2, 6, 1, 5, 3, 7 };
   char          str[24];
   instance_void(td);

   if (color>=0x100) color = VIO_COLOR_WHITE;

   // any of intensity bit mistmatch?
   if ((color&0x88^td->color&0x88) || (color&7)==7) {
      tty_strout(td,"\x1B[0m");
      if (color&8) tty_strout(td,"\x1B[1m");
      if (color&0x80) tty_strout(td,"\x1B[5m");
      td->color = (color&0x88)+7;
   }
   if ((td->color>>4&7)!=(color>>4&7)) {
      snprintf(str, 24, "\x1B[%um", 40+col_rec[color>>4&7]);
      tty_strout(td, str);
   }
   if ((td->color&7)!=(color&7)) {
      snprintf(str, 24, "\x1B[%um", 30+col_rec[color&7]);
      tty_strout(td, str);
   }
   td->color = color;
}

static void _exicc vh_tty_clear(EXI_DATA) {
   instance_void(td);
   tty_strout(td,"\x1B[2J\x1B[1;1H");
}

static void  _exicc vh_tty_setpos(EXI_DATA, u32t line, u32t col) {
   char str[24];
   instance_void(td);

   snprintf(str, 24, "\x1B[%u;%uH", line+1, col+1);
   tty_strout(td, str);
}

static u32t _exicc vh_tty_info(EXI_DATA, vio_mode_info **modes, char *prnname) {
   if (modes) *modes = 0;
   if (prnname) *prnname = 0;
   {
      instance_ret(td, 0);
      // show only 80x25
      if (modes) {
         vio_mode_info *mi = (vio_mode_info*)calloc(3, sizeof(vio_mode_info));
         u32t           ii;
         *modes = mi;
         for (ii=0; ii<2; ii++) {
            mi[ii].size    = VIO_MI_FULL;
            mi[ii].mx      = ii?132:80;
            mi[ii].my      = 25;
            mi[ii].fontx   = 8;
            mi[ii].fonty   = 16;
            mi[ii].mode_id = ii+1;
         }
      }
      if (prnname) snprintf(prnname, 32, "tty:0x%04X", td->port);
      return VHI_ANYMODE|VHI_SLOW;
   }
}

static qserr _exicc vh_tty_state(EXI_DATA, vh_settings *sb) {
   instance_ret(td, E_SYS_INVOBJECT);
   if (!sb) return E_SYS_ZEROPTR;
   // fill values
   sb->color    = td->color;
   sb->shape    = VIO_SHAPE_LINE;
   sb->px       = -1;
   sb->py       = -1;
   sb->opts     = td->opts;
   sb->ttylines = 0;
   return 0;
}

static u16t* _exicc vh_tty_memory(EXI_DATA) { return 0; }
static qs_gh _exicc vh_tty_graphic(EXI_DATA) { return 0; }

static u32t _exicc vh_tty_charout(EXI_DATA, char ch) {
   instance_ret(td,0);
   tty_charout(td,ch);
   return 0;
}

static u32t  _exicc vh_tty_strout(EXI_DATA, const char *str) {
   instance_ret(td,0);
   tty_strout(td,str);
   return 0;
}

static qserr _exicc vh_tty_writebuf(EXI_DATA, u32t col, u32t line, u32t width, u32t height, void *buf, u32t pitch) {
   u16t svcol;
   instance_ret(td, E_SYS_INVOBJECT);
   if (!width || !height) return E_SYS_INVPARM;
   if (!buf) return E_SYS_ZEROPTR;

   tty_strout(td, "\x1B[s\x1B[?7h");

   svcol = td->color;

   while (height--) {
      char    str[24];
      u16t  *bptr = (u16t*)buf;
      u32t     ii;
      snprintf(str, 24, "\x1B[%u;%uH", ++line, col+1);
      tty_strout(td, str);
      // windows hyperterm knows nothing about warp mode and scrolls the screen
      if (line==25 && col+width>=80) width = 79-col;

      for (ii=0; ii<width; ii++) {
         u8t col = *bptr>>8,
             chr = *bptr++;
         if (col!=td->color) v_setcolor(col);
         // filter control codes
         tty_charout(td, chr==9||chr==10||chr==13||chr==27?'.':chr);
      }
      buf = (u8t*)buf + pitch;
   }

   tty_strout(td, "\x1B[?7l\x1B[u");
   if (svcol!=td->color) v_setcolor(svcol);
   return 0;
}

static qserr _exicc vh_tty_readbuf(EXI_DATA, u32t col, u32t line, u32t width, u32t height, void *buf, u32t pitch) {
   return E_SYS_UNSUPPORTED;
}

static void _exicc vh_tty_fillrect(EXI_DATA, u32t col, u32t line, u32t width, u32t height, char ch, u16t color) {
   u16t  *vb;
   instance_void(td);
   if (!width || !height) return;
   if (!ch) ch = ' ';
   // alloc dynamic buffer
   vb = (u16t*)malloc(width*height*2);
   memsetw(vb, color<<8|ch, width*height);
   vh_tty_writebuf(data, 0, col, line, width, height, vb, width<<1);
   free(vb);
}

static void *vh_methods_list[] = { vh_tty_init, vh_tty_link, vh_tty_graphic,
   vh_tty_info, vh_tty_setmode, vh_tty_setmodeid, vh_tty_reset, vh_tty_clear,
   vh_tty_getmode, vh_tty_setcolor, vh_tty_setpos, vh_tty_setopts, vh_tty_state,
   vh_tty_charout, vh_tty_strout, vh_tty_writebuf, vh_tty_readbuf, vh_tty_memory,
   vh_tty_fillrect };
   
static void _std vh_tty_initialize(void *instance, void *data) {
   tty_data *td = (tty_data*)data;
   td->sign     = TTYH_SIGN;
   td->width    = 80;
   td->color    = VIO_COLOR_WHITE;
   td->self     = (qs_vh)instance;
   td->charout  = hlp_serialout;
   // other fields zeroed by caller
}

static void _std vh_tty_finalize(void *instance, void *data) {
   instance_void(td);
   td->sign = 0;
   if (portlist && td->port) {
      u32t baud;
      int idx = portlist->indexof(td->port,0);
      if (idx>=0) portlist->del(idx,1);
      // reset debug com port
      if (td->port==hlp_seroutinfo(&baud)) hlp_seroutset(td->port, baud);
   }
   if (td->link) {
      DELETE(td->link);
      td->link = 0;
   }
}

/** qs_kh_tty *********************************************/

static qserr _exicc kh_tty_init(EXI_DATA, const char *setup) {
   kh_instance_ret(kd, E_SYS_INVOBJECT);
   if (setup) return E_SYS_INVPARM;
   kd->bpos = 0;
   return 0;
}

static void* _exicc kh_tty_link(EXI_DATA) {
   kh_instance_ret(kd,0);
   return kd->link;
}

static u32t tty_charin(kh_data *kd) {
   static const u8t scan[0x60] = {
/*20h*/ 0x39,2,0x28,4,5,6,8,0x28,10,11,9,13,0x33,12,0x34,0x35,
/*30h*/ 11,2,3,4,5,6,7,8,9,10,0x27,0x27,0x33,13,0x34,0x35,
/*40h*/ 3,0x1E,0x30,0x2E,0x20,18,0x21,0x22,0x23,0x17,0x24,0x25,0x26,0x32,0x31,0x18,
/*50h*/ 0x19,16,19,0x1F,0x14,0x16,0x2F,17,0x2D,0x15,0x2C,0x1A,0x2B,0x1B,7,12,
/*60h*/ 0x29,0x1E,0x30,0x2E,0x20,18,0x21,0x22,0x23,0x17,0x24,0x25,0x26,0x32,0x31,0x18,
/*70h*/ 0x19,16,19,0x1F,0x14,0x16,0x2F,17,0x2D,0x15,0x2C,0x1A,0x2B,0x1B,0x29,14 };

   u8t chr = kd->charin(kd->port);
/*
   char buf[32];
   snprintf(buf, 32, "%02X(%c)", chr, chr>0x20?chr:0x20);
   vio_strout(buf);
   return 0; */
   if (!chr || chr>=0x80) return 0;

   if (chr==0x1B) {
      if (kd->bpos!=1) {
         kd->escb[0] = chr;
         kd->bpos = 1;
         return 0;
      } else {
         kd->bpos = 0;
         return 0x011B;
      }
   } else
   if (chr<0x20) { // ctrl-a..ctrl-z
      switch (chr) {
         case  8: return 0x0E08;  // backspace
         case  9: return 0x0F09;  // tab
         case 13: return 0x1C0D;  // enter
      }
      return (u32t)scan[chr+0x20]<<8|chr|(KEY_CTRL|KEY_CTRLLEFT)<<16;
   } else
   if (kd->bpos==1) {
      if (chr=='O' || chr=='[') { 
         kd->escb[1] = chr;
         kd->bpos++;
         return 0;
      } else
      if (chr==0x7F) {
         kd->bpos = 0;
         return 0x0E00;  // alt-backspace
      }
   } else
   if (kd->bpos>1) {
      if (kd->escb[1]=='O') {
         if (chr>='P' && chr<='[') { // F1..F10
            kd->bpos = 0;
            return (chr<'Z'?0x3B+(chr-'P'):0x85+(chr-'Z'))<<8;
         }
      } else
      if (chr>='0' && chr<='9' && kd->bpos<15 || chr=='[' && kd->bpos==2) {
         kd->escb[kd->bpos++] = chr;
         return 0;
      } else
      if (chr=='~') {
         int            chv;
#define KS ((KEY_SHIFT|KEY_SHIFTLEFT)<<16)
         static u32t recmtx[] = {
  /*  1 */  0x4700, 0x5200, 0x5300, 0x4F00, 0x4900, 0x5100, 0,0,0,0,
  /* 11 */  0x3B00, 0x3C00, 0x3D00, 0x3E00, 0x3F00, 0, 0x4000, 0x4100, 0x4200, 0x4300,
  /* 21 */  0x4400, 0, KS|0x5400, KS|0x5500, KS|0x5600, KS|0x5700, 0, KS|0x5800,
            KS|0x5900, 0,
  /* 31 */  KS|0x5A00, KS|0x5B00, KS|0x5C00, KS|0x5D00 };
         kd->escb[kd->bpos] = 0;
         chv = atoi(kd->escb+2);
         kd->bpos = 0;
         if (chv>0 && chv<=34) return recmtx[chv-1];
      } else
      if (chr>='A' && chr<='E') {    // arrows & F1..F5
         static u16t recmtx[] = { 0x48E0, 0x50E0, 0x4DE0, 0x4BE0, 0 };
         // [[A..[[E 
         if (kd->bpos==3) {
            kd->bpos = 0;
            return 0x3B+chr-'A'<<8;
         } else {
            kd->bpos = 0;
            return recmtx[chr-'A'];
         }
      } else
      if (chr=='Z') {                // shift-tab
         kd->bpos = 0;
         return KS|0x0F00;
      } else
      if (chr=='H' || chr=='K' || chr=='J') {    // home/end
         kd->bpos = 0;
         return chr=='H'?0x4700:0x4F00;
      }
   } else
   if (chr==0x7F) return 0x0E08;    // backspace

   kd->bpos = 0;
   // add a scan code (value < 0x20 was filtered above)
   return (u32t)scan[chr-0x20]<<8|chr;
}


static u16t _exicc kh_tty_read(EXI_DATA, int wait, u32t *status) {
   u32t   rc;
   kh_instance_ret(kd,0);
   rc = tty_charin(kd);
   while (wait && !rc) {
      usleep(32*1024);
      rc = tty_charin(kd);
   }
   if (status) *status = rc>>16;
   //if (rc) log_it(0, "tty: %04X\n", rc);
   return rc;
}

static u32t _exicc kh_tty_status(EXI_DATA) { return 0; }

static void *kh_methods_list[] = { kh_tty_init, kh_tty_link, kh_tty_read, kh_tty_status };

static void _std kh_tty_initialize(void *instance, void *data) {
   kh_data *kd = (kh_data*)data;
   kd->sign    = TTYH_SIGN;
   kd->charin  = hlp_serialin;
}

static void _std kh_tty_finalize(void *instance, void *data) {
   kh_instance_void(kd);
   kd->sign   = 0;
}

void exi_register_vh_tty(void) {
   if (sizeof(vh_methods_list)!=sizeof(_qs_vh) || sizeof(kh_methods_list)!=sizeof(_qs_kh))
      vio_msgbox("SYSTEM WARNING!", "tty: Fix me!", MSG_LIGHTRED|MSG_OK, 0);
   vh_classid = exi_register("qs_vh_tty", vh_methods_list, sizeof(vh_methods_list)/sizeof(void*),
      sizeof(tty_data), 0, vh_tty_initialize, vh_tty_finalize, 0);
   kh_classid = exi_register("qs_kh_tty", kh_methods_list, sizeof(kh_methods_list)/sizeof(void*),
      sizeof(kh_data), EXIC_PRIVATE, kh_tty_initialize, kh_tty_finalize, 0);
}
