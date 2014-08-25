#include "stdlib.h"
#include "console.h"
#include "errno.h"
#include "conint.h"
#include "seldesc.h"
#include "qsmodext.h"
#include "qsinit_ord.h"
#include "writegif.h"
#include "direct.h"
#include "time.h"
#include "qsutil.h"
#include "qslist.h"
#include "vio.h"

// default VGA palette for text mode + cursor color for text emu modes
u8t stdpal_bin[51] = { 0x00,0x00,0x00,0x00,0x00,0xAA,0x00,0xAA,0x00,
   0x00,0xAA,0xAA,0xAA,0x00,0x00,0xAA,0x00,0xAA,0xAA,0x55,0x00,0xAA,
   0xAA,0xAA,0x55,0x55,0x55,0x55,0x55,0xFF,0x55,0xFF,0x55,0x55,0xFF,
   0xFF,0xFF,0x55,0x55,0xFF,0x55,0xFF,0xFF,0xFF,0x55,0xFF,0xFF,0xFF,
   0xFF,0x7F,0x00 };

typedef struct {
   int     x;
   int     y;
   u8t   bin[];
} fontbits;

static ptr_list pfnt = 0;

static fontbits *addfont(int ebx, fontbits *fb) {
   struct rmcallregs_s rr;
   u8t  *font;
   int     dy;

   memset(&rr, 0, sizeof(rr));
   rr.r_eax = 0x1130;
   rr.r_ebx = ebx<<8;
   if (!int10h(&rr)) return 0;
   font = (u8t*)(hlp_segtoflat(rr.r_es)+(rr.r_ebp&0xFFFF));

   switch (ebx) {
      case 3:
        fb = (fontbits*)malloc(8*256+8);
        dy=4; fb->x=8; fb->y=8;
        break;
      case 4:
        // high 128 characters
        memcpy(&fb->bin[1024], font, 1024);
        return 0;
      case 5:
      case 7: {
        int diffs = 0;
        fb = (fontbits*)memDup(fb);
        fb->x=9;
        // copy alternate font data
        while (*font) {
           //log_it(3,"diff for char %02X %c\n", *font, *font);

           dy = fb->y**font++;
           memcpy(&fb->bin[dy], font, fb->y);
           font += fb->y;
           diffs++;
        }
        log_it(3,"9x%d font: %d diffs\n", fb->y, diffs);
        pfnt->add(fb);
        return 0;
      }
      case 2:
      case 6:
        dy = ebx==2?14:16;
        fb = (fontbits*)malloc(dy*256+8);
        fb->x=8; fb->y=dy;
        break;
      default:
        return 0;
   }
   memcpy(&fb->bin, font, dy*256);
   pfnt->add(fb);
   return fb;
}

void con_queryfonts(void) {
   fontbits *fb;
   if (pfnt) return;
   pfnt   = (ptr_list)exi_create("ptr_list");

   fb = addfont(2,0);     // 8 x 14, 9 x 14
   if (fb) addfont(5,fb);
   fb = addfont(6,0);     // 8 x 16, 9 x 16
   if (fb) addfont(7,fb);
   fb = addfont(3,0);     // 8 x 8
   if (fb) addfont(4,fb); 
}

void con_freefonts(void) {
   if (!pfnt) return; else {
      u32t cnt = pfnt->count(), ii;
      // free font data
      for (ii=0; ii<cnt; ii++) free(pfnt->value(ii));
      // and list
      exi_free(pfnt);
      pfnt = 0;
   }
}

static fontbits *con_searchfont(int x, int y) {
   if (pfnt) {
      u32t cnt = pfnt->count(), ii, dx, dy, lpos = FFFF;
      // search for suitable font
      for (ii=0; ii<cnt; ii++) {
         fontbits *fb = (fontbits*)pfnt->value(ii);
         if (lpos==FFFF || abs(fb->x-x)<=dx && abs(fb->y-y)<=dx) {
            dx = abs(fb->x-x); 
            dy = abs(fb->y-y);
            lpos = ii;
         }
      }
      if (lpos!=FFFF) return (fontbits*)pfnt->value(lpos);
   }
   return 0;
}

static void *con_getfont(int x, int y) {
   if (pfnt) {
      fontbits *fb = con_searchfont(x,y);
      if (fb) return &fb->bin;
   }
   return 0;
}

void _std con_fontadd(int width, int height, void *data) {
   if (pfnt) {
      u32t     cnt = pfnt->count(), ii,
              bpln = BytesPerFont(width);
      fontbits *fb = 0;
      // search for suitable font
      for (ii=0; ii<cnt; ii++) {
         fb = (fontbits*)pfnt->value(ii);
         if (fb->x==width && fb->y==height) break; 
            else fb = 0;
      }
      if (!fb) {
         fb = (fontbits*)malloc(height*256*bpln+8);         
         fb->x = width; 
         fb->y = height;
         pfnt->add(fb);
      }
      memcpy(&fb->bin, data, height*256*bpln);
   }
}

void* con_buildfont(int width, int height) {
   fontbits *fb = con_searchfont(width, height);
   u32t    *out;
   int   ii, yy, bpf, ys;
   if (!fb) return 0;

   out = (u32t*)malloc(256*4*height);
   bpf = BytesPerFont(fb->x);
   ys  = fb->y; 
   if (ys>height) ys = height;

   memZero(out);

   for (ii=0; ii<256; ii++) {
      for (yy=0; yy<ys; yy++) {
         u32t bw32 = 0;
         switch (bpf) {
            case 1: bw32 = (u32t)fb->bin[ii*fb->y+yy]<<24;
               /* copy one bit in 9-pixels font
                 patch only number of symbols, because of wrong font tables in BIOS */
               if (fb->x==9)
                  if (ii==8 || ii==10 || ii>=0xC0 && ii<=0xDF)
                     bw32 |= bw32>>1 & 0x800000;
               break;
            case 2:
               bw32 = (u32t)((u16t*)&fb->bin)[ii*fb->y+yy]<<16;
               break;
            case 4:
               bw32 = ((u32t*)&fb->bin)[ii*fb->y+yy];
               break;
         }
         out[ii*height+yy] = bw32;
      }
   }
   return out;
}

static void tx2img(u32t mx, u32t my, u32t ltw, u32t lth, u16t *txmem, u8t *dst) {
   u8t    *fnt = (u8t*)con_getfont(ltw, lth);
   u32t xx, yy;
   if (!fnt) return;
   for (yy=0; yy<my; yy++) {
      for (xx=0; xx<mx; xx++) {
         u32t lx, ly;
         u16t txv = txmem[yy*mx+xx];
         u8t *chr = fnt + (txv&0xFF)*lth,
           a_bgrd = txv>>12,
           a_forg = txv>>8 & 15;

         for (ly=0; ly<lth; ly++) {
            u8t cw = *chr++,
               *wp = dst + ((yy*lth+ly)*mx + xx)*ltw;
            for (lx=0; lx<ltw; lx++) {
               if (lx>=8) { *wp = wp[-1]; wp++; } else {
                  *wp++ = (s8t)cw<0?a_forg:a_bgrd;
                  cw<<=1;
               }
            }
         }
      }
   }
}

static int    shots_on = 0;
static char *shots_dir = 0,
             *name_buf = 0;
static u32t  shots_idx = 1;

// trying to save stack here, because called from exit chain!
int _std con_makeshot(const char *name, int mute) {
   int   rc;
   FILE *ff;
   u32t mx, my, mfl;
   con_modeinfo *mi;
   // query current console mode
   con_getmode(&mx, &my, &mfl);
   mi = con_currentmode();
   if ((mfl&CON_GRAPHMODE)!=0 || !mi || !mi->font_x || !mi->font_y) 
      return ENOTTY;
   // makes a BEEEEP :)
   if (!mute) vio_beep(700,40);
   // get free file name
   do {
      if (!name) {
         int len = shots_dir ? strlen(shots_dir) : 0;
         if (len) memcpy(name_buf, shots_dir, len);
         sprintf(name_buf+len, "GRAB%03d.GIF", shots_idx);
      }
      rc = _access(name?name:name_buf,F_OK);
      if (!rc) {
         if (name) return EACCES; else
         if (++shots_idx==1000) return EACCES;
      }
   } while (!rc);

   rc = 0;
   ff = fopen(name?name:name_buf, "wb");
   if (!ff) return errno; else {
      static char    str[128];
      time_t     now = time(0);
      int     rcsize = 0, ii,
                 svx = mx * mi->font_x,
                 svy = my * mi->font_y;
      void   *gifmem = 0,
              *txmem = 0,
              *grmem = 0;
      strcpy(str, "QSINIT screen, saved at ");
      strcat(str, ctime(&now));
      str[strlen(str)-1] = 0;
      // get and convert text screen
      txmem = malloc(mx * my * 2);
      grmem = malloc(svx * svy);
      con_read(0, 0, mx, my, txmem, mx*2);
      tx2img(mx, my, mi->font_x, mi->font_y, txmem, grmem);
      // get default text mode palette
      memset(txmem, 0, 768);
      memcpy(txmem, stdpal_bin, 48);
      // encode GIF image
      gifmem = WriteGIF(&rcsize, grmem, svx, svy, (GIFPalette*)txmem, -1, str);
      free(txmem);
      free(grmem);
      // and write it
      if (!gifmem) rc = EFAULT; else
         if (fwrite(gifmem,1,rcsize,ff)!=rcsize) rc = EIO;
      fclose(ff);
   }

   if (!mute) {
      while (vio_beepactive()) usleep(2000);
      vio_beep(600,40);
   }
   return rc;
}

int _std hook_proc(mod_chaininfo *mc) {
   u16t key = mc->mc_regs->pa_eax;
   // react on Alt-G
   if (key==0x2200) con_makeshot(0,0);
   return 1;
}

int  _std con_instshot(const char *path) {
   char *np = 0;
   if (path) {
      if (strlen(path)>=_MAX_PATH-16) return ENAMETOOLONG;
      np = strdup(path);
      if (!np) return ENOMEM; else {
         dir_t pi;
         int len = strlen(np);
         if (len && (np[len-1]=='\\'||np[len-1]=='/'))
            np[len-1]=0;
         // check for existence and directory attr
         if (_dos_stat(np,&pi)) return errno;
         if ((pi.d_attr&_A_SUBDIR)==0) return ENOTDIR;
         strcat(np,"\\");
      }
   }
   if (shots_on) {
      if (shots_dir) { free(shots_dir); shots_dir = 0; }
   } else {
      u32t qsinit = mod_query(MODNAME_QSINIT, MODQ_NOINCR);
      if (qsinit) {
         mod_apichain(qsinit, ORD_QSINIT_key_read, APICN_ONEXIT|APICN_FIRSTPOS, hook_proc);
         mod_apichain(qsinit, ORD_QSINIT_key_wait, APICN_ONEXIT|APICN_FIRSTPOS, hook_proc);
      }
      name_buf = (char*)malloc(_MAX_PATH+1);
   }
   shots_dir = np;
   return 0;
}

void _std con_removeshot(void) {
   if (!shots_on) return; else {
      u32t qsinit = mod_query(MODNAME_QSINIT, MODQ_NOINCR);
      if (qsinit) {
         mod_apiunchain(qsinit, ORD_QSINIT_key_read, APICN_ONEXIT, hook_proc);
         mod_apiunchain(qsinit, ORD_QSINIT_key_wait, APICN_ONEXIT, hook_proc);
      }
      shots_on = 0;
      if (shots_dir) { free(shots_dir); shots_dir = 0; }
      if (name_buf) { free(name_buf); name_buf = 0; }
   }
}