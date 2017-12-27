//
// QSINIT "start" module
// vio handler : text mode buffer
//
#include "qcl/sys/qsvioint.h"
#include "qsutil.h"
#include "qserr.h"
#include "syslocal.h"

#define VBUF_SIGN        0x46554256          // VBUF
#define DBUF_SIGN        0x46554244          // DBUF

#define vb_instance_ret(inst,err)          \
   vbuf_data *inst = (vbuf_data*)data;     \
   if (inst->sign!=VBUF_SIGN) return err;

#define db_instance_ret(inst,err)          \
   draw_data *inst = (draw_data*)data;     \
   if (inst->sign!=DBUF_SIGN) return err;

#define vb_instance_void(inst)             \
   vbuf_data *inst = (vbuf_data*)data;     \
   if (inst->sign!=VBUF_SIGN) return;

#define db_instance_void(inst)             \
   draw_data *inst = (draw_data*)data;     \
   if (inst->sign!=DBUF_SIGN) return;

#define instance_ready(inst)            \
   if (inst->mx==0) vb_setmode(data, 0, 80, 25, 0);

#define MoveBlock(Sour,Dest,d_X,d_Y,LineLen_S,LineLen_D) {  \
   u8t *dest=(u8t*)(Dest), *sour=(u8t*)(Sour);              \
   u32t d_y=d_Y,d_x=d_X,l_d=LineLen_D,l_s=LineLen_S;        \
   for (;d_y>0;d_y--) {                                     \
     memcpy(dest,sour,d_x); dest+=l_d; sour+=l_s;           \
   }                                                        \
}

typedef struct {
   u32t         sign;
   u16t          *bm;
   u32t        mx,my;
   u32t        px,py;    ///< output position
   u32t    linecount;
   u32t         mode;
   int        extmem;
   qs_vh    instance;
   u16t        shape;
   u8t         color;
} vbuf_data;

static void vb_free(vbuf_data *vb) {
   if (!vb->extmem && vb->bm) free(vb->bm);
   vb->bm=0; vb->mx=0; vb->my=0; vb->px=0; vb->py=0;
   vb->extmem    = 0;
   vb->color     = VIO_COLOR_WHITE;
   vb->shape     = VIO_SHAPE_LINE;
   vb->linecount = 0;
   vb->mode      = 0;
}

static void vb_scrollup(vbuf_data *vb) {
   u32t lline = vb->mx*(vb->my-1);
   memmove(vb->bm, vb->bm+vb->mx, lline<<1);
   memsetw(vb->bm+lline, 0x20|vb->color<<8, vb->mx);
   if (vb->py>=vb->my) vb->py = vb->my-1;
}

static void _exicc vb_clear(EXI_DATA) {
   vb_instance_void(vb);
   memsetw(vb->bm, 0x20|VIO_COLOR_WHITE<<8, vb->mx*vb->my);
   vb->px = 0;
   vb->py = 0;
}

static u32t _exicc vb_strout(EXI_DATA, const char *str);

static qs_kh _exicc vb_link(EXI_DATA) { return 0; }
static qs_gh _exicc vb_graphic(EXI_DATA) { return 0; }

static u32t _exicc vb_info(EXI_DATA, vio_mode_info **modes, char *prnname) {
   if (modes) *modes = 0;
   if (prnname) strcpy(prnname, QS_VH_BUFFER);
   return VHI_ANYMODE|VHI_MEMORY;
}

static qserr _exicc vb_setmode(EXI_DATA, u32t modex, u32t modey, void *extmem) {
   vb_instance_ret(vb, E_SYS_INVOBJECT);

   if (modex<40 || modex>32768 || modey<25) return E_SYS_INVPARM;
   vb_free(vb);

   if (!extmem) {
      u32t memsz = modex*modey<<1;
      if (memsz>=_256KB) {
         u32t max;
         hlp_memavail(&max, 0);
         if (max<memsz) return E_SYS_NOMEM;
      }
      vb->bm = (u16t*)malloc(memsz);
      vb->extmem = 0;
   } else {
      vb->bm = (u16t*)extmem;
      vb->extmem = 1;
   }
   vb->mx = modex;
   vb->my = modey;
   if (!vb->extmem) vb_clear(data, 0);
   return 0;
}

static qserr _exicc vb_setmodeid(EXI_DATA, u32t mode_id) {
   return E_SYS_UNSUPPORTED;
}

static qserr _exicc vb_init(EXI_DATA, const char *setup) {
   vb_instance_ret(vb, E_SYS_INVOBJECT);
   return 0;
}

static qserr _exicc vb_reset(EXI_DATA) {
   vb_instance_ret(vb, E_SYS_INVOBJECT);
   if (vb->mx!=80 || vb->my!=25) 
      return vb_setmode(data, 0, 80, 25, 0);

   vb_clear(data, 0);
   vb->linecount = 0;
   vb->color     = VIO_COLOR_WHITE;
   vb->shape     = VIO_SHAPE_LINE;
   vb->mode      = 0;
   return 0;
}

static qserr _exicc vb_getmode(EXI_DATA, u32t *modex, u32t *modey, u16t *modeid) {
   vb_instance_ret(vb, E_SYS_INVOBJECT);
   instance_ready(vb);

   if (modex) *modex = vb->mx;
   if (modey) *modey = vb->my;
   if (modeid) *modeid = 0;
   return 0;
}

static void _exicc vb_setcolor(EXI_DATA, u16t color) {
   vb_instance_void(vb);
   instance_ready(vb);

   vb->color = color;
}

static void _exicc vb_setpos(EXI_DATA, u32t line, u32t col) {
   vb_instance_void(vb);
   instance_ready(vb);

   if (line>=vb->my || col>=vb->mx) return;
   vb->py = line;
   vb->px = col;
}

/** set output options.
    @param   opts    flags (VBMODE_*)
    @return 0 on success or error code on incorrect parameter */
static qserr _exicc vb_setopts(EXI_DATA, u32t opts) {
   vb_instance_ret(vb, E_SYS_INVOBJECT);
   // this is line counter setup call
   if (opts&VHSET_LINES) {
      vb->linecount = opts & x7FFF;
      return 0;
   }
   // save shape settings
   if (opts&VHSET_SHAPE) {
      vb->shape = (u16t)opts;
      opts &= ~(VHSET_SHAPE|0xFFFF);
   }
   // and validate remain bits
   if (opts&~(VHSET_WRAPX|VHSET_BLINK)) return E_SYS_INVPARM;
   vb->mode = opts;
   return 0;
}

static inline void vb_drawchar(vbuf_data *vb, u16t value, u32t x, u32t y) {
   if (x>=vb->mx || y>=vb->my) return;
   vb->bm[y*vb->mx+x] = value;
}

static u32t vb_chrprint(vbuf_data *vb, char ch) {
   switch (ch) {
      case  9: return vb_strout(vb, 0, "    ");
      case 13:
         vb->px = 0;
         return 0;
      default:
         vb_drawchar(vb, ch|vb->color<<8, vb->px, vb->py);
         if (++vb->px<vb->mx) break;
         vb->px = 0;
         if (vb->mode&VHSET_WRAPX) break;
      case 10:
         vb->px = 0;
         vb->linecount++;
         if (++vb->py>=vb->my) vb_scrollup(vb);
         return 1;
   }
   return 0;
}

static u32t _exicc vb_charout(EXI_DATA, char ch) {
   vb_instance_ret(vb,0);
   instance_ready(vb);
   return vb_chrprint(vb, ch);
}

static u32t _exicc vb_strout(EXI_DATA, const char *str) {
   u32t eolc = 0;
   vb_instance_ret(vb,0);
   instance_ready(vb);
   if (str)
      while (*str) eolc+=vb_chrprint(vb, *str++);
   return eolc;
}

static qserr _exicc vb_writebuf(EXI_DATA, u32t x, u32t y, u32t dx, u32t dy, void *buf, u32t pitch) {
   vb_instance_ret(vb, E_SYS_INVOBJECT);
   instance_ready(vb);
   // drop bad coordinates
   if ((long)x<0||(long)y<0||(long)dx<0||(long)dy<0) return E_SYS_INVPARM;
   // is rect inside screen?
   if (x>=vb->mx || y>=vb->my) return E_SYS_INVPARM;
   if (x+dx>vb->mx) dx = vb->mx - x;
   if (y+dy>vb->my) dy = vb->my - y;
   // copying memory
   MoveBlock(buf, vb->bm+y*vb->mx+x, dx<<1, dy, pitch, vb->mx<<1);
   return 0;
}

static qserr _exicc vb_readbuf(EXI_DATA, u32t x, u32t y, u32t dx, u32t dy, void *buf, u32t pitch) {
   vb_instance_ret(vb, E_SYS_INVOBJECT);
   instance_ready(vb);
   // is rect inside screen?
   if (x>=vb->mx || y>=vb->my || x+dx>vb->mx || y+dy>vb->my) return E_SYS_INVPARM;
   // copying memory
   MoveBlock(vb->bm+y*vb->mx+x, buf, dx<<1, dy, vb->mx<<1, pitch);
   return 0;
}

static u16t* _exicc vb_memory(EXI_DATA) {
   vb_instance_ret(vb,0);
   instance_ready(vb);

   return vb->bm;
}

static void _exicc vb_fillrect(EXI_DATA, u32t x, u32t y, u32t dx, u32t dy, char ch, u16t color) {
   u16t fv = (ch?ch:' ') | color<<8;
   vb_instance_void(vb);
   instance_ready(vb);
   // drop bad coordinates
   if ((long)x<0||(long)y<0||(long)dx<0||(long)dy<0) return;
   // is rect inside screen?
   if (x>=vb->mx || y>=vb->my) return;
   if (x+dx>vb->mx) dx = vb->mx - x;
   if (y+dy>vb->my) dy = vb->my - y;
   // fill it finally
   if (dx==vb->mx) memsetw(vb->bm+y*vb->mx, fv, dy*dx); else
      for (;dy>0;dy--) memsetw(vb->bm+y++*vb->mx+x, fv, dx);
}

static qserr _exicc vb_state(EXI_DATA, vh_settings *sb) {
   vb_instance_ret(vb,E_SYS_INVOBJECT);
   if (!sb) return E_SYS_ZEROPTR;
   // fill values
   sb->color    = vb->color;
   sb->shape    = vb->shape;
   sb->px       = vb->px;
   sb->py       = vb->py;
   sb->opts     = vb->mode;
   sb->ttylines = vb->linecount;
   return 0;
}

static void *vb_methods_list[] = { vb_init, vb_link, vb_graphic, vb_info,
   vb_setmode, vb_setmodeid, vb_reset, vb_clear, vb_getmode, vb_setcolor,
   vb_setpos, vb_setopts, vb_state, vb_charout, vb_strout, vb_writebuf,
   vb_readbuf, vb_memory, vb_fillrect };

static void _std vb_initialize(void *instance, void *data) {
   vbuf_data *vb = (vbuf_data*)data;
   vb->sign     = VBUF_SIGN;
   vb->color    = VIO_COLOR_WHITE;
   vb->shape    = VIO_SHAPE_LINE;
   vb->instance = (qs_vh)instance;
   // other fields zeroed by caller
}

static void _std vb_finalize(void *instance, void *data) {
   vb_instance_void(vb);
   vb_free(vb);
   vb->sign = 0;
}

void exi_register_vio(void) {
   // check # of methods at least
   if (sizeof(vb_methods_list)!=sizeof(_qs_vh))
      vio_msgbox("SYSTEM WARNING!", "viobuf: Fix me!", MSG_LIGHTRED|MSG_OK, 0);
   // register buffer class
   exi_register(QS_VH_BUFFER, vb_methods_list, sizeof(vb_methods_list)/sizeof(void*),
      sizeof(vbuf_data), 0, vb_initialize, vb_finalize, 0);
}
