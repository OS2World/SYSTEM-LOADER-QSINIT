#include "stdlib.h"
#include "errno.h"
#include "conint.h"
#include "direct.h"
#include "time.h"
#include "qcl/sys/qsvioint.h"

// default VGA palette for text mode + cursor color for text emu modes
u8t stdpal_bin[51] = { 0x00,0x00,0x00,0x00,0x00,0xAA,0x00,0xAA,0x00,
   0x00,0xAA,0xAA,0xAA,0x00,0x00,0xAA,0x00,0xAA,0xAA,0x55,0x00,0xAA,
   0xAA,0xAA,0x55,0x55,0x55,0x55,0x55,0xFF,0x55,0xFF,0x55,0x55,0xFF,
   0xFF,0xFF,0x55,0x55,0xFF,0x55,0xFF,0xFF,0xFF,0x55,0xFF,0xFF,0xFF,
   0xFF,0x7F,0x00 };

ptr_list sysfnt = 0;

void con_queryfonts(void) {
   if (sysfnt) return;
   sysfnt = NEW_G(ptr_list);
   // call "platform" function
   pl_addfont();
}

void con_freefonts(void) {
   mt_swlock();
   if (!sysfnt) return; else {
      u32t cnt = sysfnt->count(), ii;
      // free font data
      for (ii=0; ii<cnt; ii++) free(sysfnt->value(ii));
      // and list
      DELETE(sysfnt);
      sysfnt = 0;
   }
   mt_swunlock();
}

static fontbits *con_searchfont(int x, int y) {
   fontbits *rc = 0;
   mt_swlock();
   if (sysfnt) {
      u32t cnt = sysfnt->count(), ii, dx, dy, lpos = FFFF;
      // search for suitable font
      for (ii=0; ii<cnt; ii++) {
         fontbits *fb = (fontbits*)sysfnt->value(ii);
         if (lpos==FFFF || abs(fb->x-x)<=dx && abs(fb->y-y)<=dx) {
            dx = abs(fb->x-x);
            dy = abs(fb->y-y);
            lpos = ii;
         }
      }
      if (lpos!=FFFF) rc = (fontbits*)sysfnt->value(lpos);
   }
   mt_swunlock();
   return rc;
}

static void *con_getfont(int x, int y) {
   if (sysfnt) {
      fontbits *fb = con_searchfont(x,y);
      if (fb) return &fb->bin;
   }
   return 0;
}

u32t _std con_fontavail(int width, int height) {
   return con_getfont(width,height)?1:0;
}

void con_addfontmodes(int width, int height) {
   dd_list  ml = NEW(dd_list);
   u32t     ii;

   mt_swlock();
   for (ii=0; ii<mode_cnt; ii++)
      if ((modes[ii]->flags&CON_GRAPHMODE) && modes[ii]->bits>=8) {
         u32t wx = modes[ii]->width/width,
              wy = modes[ii]->height/height;
         if (wx>=80 && wy>=25) {
            u32t wa = modes[ii]->height<<16|modes[ii]->width;
            if (ml->indexof(wa,0)<0) ml->add(wa);
         }
      }
   mt_swunlock();
   // add all possible modes
   for (ii=0; ii<ml->count(); ii++) {
      u32t va = ml->value(ii),
          res = con_addtextmode(width, height, va&0xFFFF, va>>16);
      if (res) log_it(3, "addtextmode(%u,%u,%u,%u)==%u\n", width, height,
         va&0xFFFF, va>>16, res);
   }
   DELETE(ml);
}

void _std con_fontadd(int width, int height, void *data) {
   int ok = 0;
   mt_swlock();
   if (sysfnt && width && height && data) {
      u32t      cnt = sysfnt->count(), ii,
               bpln = BytesPerFont(width);
      fontbits  *fb = 0;
      // search for suitable font
      for (ii=0; ii<cnt; ii++) {
         fb = (fontbits*)sysfnt->value(ii);
         if (fb->x==width && fb->y==height) break;
            else fb = 0;
      }
      if (!fb) {
         fb = (fontbits*)malloc(height*256*bpln+8);
         fb->x = width;
         fb->y = height;
         sysfnt->add(fb);
      }
      memcpy(&fb->bin, data, height*256*bpln);
      ok = 1;
   }
   mt_swunlock();
   if (ok) con_addfontmodes(width, height);
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

   mem_zero(out);

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
   u8t  *fnt = (u8t*)con_getfont(ltw, lth);
   u32t  bpp = ltw>16?4:(ltw>9?2:1), xx, yy,
        mask = bpp==1?0x80:(bpp==2?0x8000:0x80000000);
   if (!fnt) return;

   for (yy=0; yy<my; yy++) {
      for (xx=0; xx<mx; xx++) {
         u32t   lx, ly;
         u16t   txv = txmem[yy*mx+xx];
         u8t   *chr = fnt + (txv&0xFF)*lth*bpp;
         u8t a_bgrd = txv>>12,
             a_forg = txv>>8 & 15;

         for (ly=0; ly<lth; ly++) {
            u8t *wp = dst + ((yy*lth+ly)*mx + xx)*ltw;
            u32t cw = 0;
            switch (bpp) {
               case 1: cw = *chr++; break;
               case 2: cw = *(u16t*)chr; chr+=2; break;
               case 4: cw = *(u32t*)chr; chr+=4; break;
            }
            for (lx=0; lx<ltw; lx++) {
               if (ltw==9 && lx>=8) { *wp = wp[-1]; wp++; } else {
                  *wp++ = cw&mask?a_forg:a_bgrd;
                  cw<<=1;
               }
            }
         }
      }
   }
}

static u32t   shots_on = 0;
static u32t  shots_idx = 1;

// trying to save stack here, because called from exit chain!
int _std con_makeshot(u32t sesno, const char *fname, int mute) {
   u32t           mx = 0, my, fnpos, fnx = 0, fny = 0;
   u16t       modeid = 0;
   FILE          *ff;
   int    rc, dev_id,
             locname;
   char        *name = (char*)fname;
   se_stats      *si = se_stat(sesno);
   qs_vh    scrncopy = 0;
   // this is detached session, may be?
   if (!si) return ENOTTY;
   /* query session`s active mode and first active device.
      We may have no active device if session is in the background. */
   mx     = si->mx;
   my     = si->my;
   dev_id = !si->handlers ? -1 : si->mhi[0].dev_id;
   modeid = !si->handlers ?  0 : si->mhi[0].reserved;
   free(si);
   if (!mx) return ENOTTY;
   /* try to query font size on the first active output device,
      simulate values for background session */
   if (dev_id>=0) {
      vio_mode_info *mi = vio_modeinfo(mx, my, dev_id), *mp = mi;
      if (!mi) dev_id = -1; else {
         if (modeid)
            while (mp->size)
               if ((mp->mode_id&0xFFFF)==modeid) break; else mp++;
         if (mp->size) {
            fnx = mp->fontx;
            fny = mp->fonty;
         } else
            dev_id = -1;
         free(mi);
      }
   }
   if (dev_id<0) {
      fnx = 8;
      fny = mx==80 && my<=30 ? 16 : 8;
   }
   // makes a BEEEEP :)
   if (!mute) vio_beep(700,40);
   // get a free file name
   locname = name==0;
   do {
      if (locname) {
         // alloc name buffer or get it from storage with directory string
         if (!name) {
            name = (char*)sto_datacopy(STOKEY_SHOTDIR,0);

            if (!name) name = (char*)calloc_thread(_MAX_PATH+1,1); else
               name = (char*)realloc_thread(name, _MAX_PATH+1);
            fnpos = strlen(name);
         }
         sprintf(name+fnpos, "GRAB%03d.GIF", shots_idx);
      }
      rc = _access(name,F_OK);
      if (!rc) {
         if (!locname) return EACCES; else
         // safe increment
         if (mt_safedadd(&shots_idx,1)>=1000) return EACCES;
      }
   } while (!rc);

   /* function returns "vio buffer" class with copy of session screen */
   scrncopy = se_screenshot(sesno);
   if (!scrncopy) return ENOTTY;

   rc = 0;
   ff = fopen(name, "wb");

   if (!ff) rc = errno; else {
      static char    str[128];
      time_t     now = time(0);
      int     rcsize = 0, ii,
                 svx = mx * fnx,
                 svy = my * fny;
      void   *gifmem = 0,
                *plt = malloc_th(sizeof(GIFPalette)),
              *grmem = malloc_th(svx*svy);
      strcpy(str, "QSINIT screen, saved at ");
      strcat(str, ctime(&now));
      str[strlen(str)-1] = 0;
      tx2img(mx, my, fnx, fny, scrncopy->memory(), grmem);
      // get default text mode palette
      memset(plt, 0, 768);
      memcpy(plt, stdpal_bin, 48);
      // encode GIF image
      gifmem = con_writegif(&rcsize, grmem, svx, svy, (GIFPalette*)plt, -1, str);
      free(grmem);
      free(plt);
      // and write it
      if (!gifmem) rc = EFAULT; else
         if (fwrite(gifmem,1,rcsize,ff)!=rcsize) rc = EIO;
      fclose(ff);
      log_it(3, "Screen shot %s %s (%ux%u, %u bytes)\n", name, rc?"save error":"saved",
         svx, svy, rcsize);
   }
   if (scrncopy) DELETE(scrncopy);
   if (locname) free(name);

   if (!mute) {
      while (vio_beepactive()) usleep(2000);
      vio_beep(600,40);
   }
   return rc;
}

void _std makeshot_cb(sys_eventinfo *ei) {
   se_stats *dl = se_deviceenum();
   u32t      ii;
   /* push Ctrl-P back to the active session`s queue if we got it on display
      screen */
   if ((u16t)ei->info==0x1910 && ei->info2==0) {
      se_keypush(se_foreground(), 0x1910, ei->info>>16);
      return;
   }
   /* search for the device, where hotkey was pressed and query active session
      in it */
   for (ii=0; ii<dl->handlers; ii++)
      if (dl->mhi[ii].dev_id==ei->info2)
         if (!dl->mhi[ii].reserved) return; else
            con_makeshot(dl->mhi[ii].reserved, 0, 0);
}

int  _std con_instshot(const char *path) {
   u32t  hkflags;
   char       np[_MAX_PATH+1];
   qserr      rc;

   if (path) {
      int len = strlen(path);
      if (len>=_MAX_PATH-16) return ENAMETOOLONG; else {
         dir_t pi;
         strcpy(np,path);
         if (len && (np[len-1]=='\\'||np[len-1]=='/')) np[len-1] = 0;
         // check for existence and directory attr
         if (_dos_stat(np,&pi)) return errno;
         if ((pi.d_attr&_A_SUBDIR)==0) return ENOTDIR;
         strcat(np,"\\");
      }
   } else
      np[0] = 0;
   shots_on = 1;
   hkflags  = SECB_GLOBAL|(mt_active()?SECB_THREAD:0);
   // Alt-G
   rc = sys_sethotkey(0x2200, KEY_ALT, hkflags, makeshot_cb);
   // Ctrl-P (filtered above to work in serial port sessions only)
   if (!rc) sys_sethotkey(0x1910, KEY_CTRL, hkflags, makeshot_cb);
   // save to storage key to make it thread-safe
   if (!rc) sto_save(STOKEY_SHOTDIR, np, strlen(np)+1, 1); else shots_on = 0;
   return rc?qserr2errno(rc):0;
}

void _std con_removeshot(void) {
   // make it thread safe ;)
   if (mt_cmpxchgd(&shots_on,0,1)!=1) return;
   // uninstall hotkey callback
   sys_sethotkey(0,0,0,makeshot_cb);
   // remove storage key
   sto_del(STOKEY_SHOTDIR);
}
