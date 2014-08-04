//
// QSINIT
// screen support dll
//
#include "stdlib.h"
#include "qsutil.h"
#include "qslog.h"
#include "qsmod.h"
#include "vio.h"
#include "console.h"
#include "memmgr.h"
#include "conint.h"
#include "qsshell.h"
#include "qspage.h"
#include "errno.h"

int        lib_ready =  0; // lib is ready
const char *selfname = MODNAME_CONSOLE;
int     mode_changed =  0; // mode was changed by lib, at least once
con_modeinfo  *modes =  0; // public mode info array
u32t        mode_cnt =  0; // number of modes available
modeinfo       *mref =  0; // internal mode info array
VBEModeInfo   *vinfo =  0; // vesa mode info
u32t        vesa_cnt =  0; // vesa mode info array size
int     current_mode = -1, // current mode index in mode info
           real_mode = -1; // current REAL mode index in mode info
u32t   current_flags =  0; // current mode flags
int     global_nolfb =  0; // lfb disabled
u32t    vesa_memsize =  0; // vesa memory size
u32t     modelimit_x =  0,
         modelimit_y =  0;
u32t        txemu800 =  0, // text emulation modes
            txemumax =  0;

static u32t fbits[] = {1, 4, 8, 15, 16, 24, 32, 0};

void con_init(void) {
   struct VBEModeInfoBlock *mdinfo;
   char *vesakey = getenv("VESA");
   int      vesa = 1,
         modemax = -1,   // max resolution mode for text emulation
         mode800 = -1;   // 800x600 mode for text emulation
   u32t     ii, modemask = 0x5F;


   /* parse env key:
      "VESA = ON [, NOLFB][, MODES = 0x0F][, MAXW = 1024][, MAXH = 768]" */
   if (vesakey) {
      str_list *lst=str_split(vesakey,",");
      vesa = env_istrue("VESA");
      if (vesa<0) vesa=0;

      ii = 1;
      while (ii<lst->count) {
         if (stricmp(lst->item[ii],"NOLFB")==0) global_nolfb = 1; else
         if (strnicmp(lst->item[ii],"MODES=",6)==0)
            modemask = strtoul(lst->item[ii]+6, 0, 0); else
         if (strnicmp(lst->item[ii],"MAXW=",5)==0)
            modelimit_x = strtoul(lst->item[ii]+5, 0, 0); else
         if (strnicmp(lst->item[ii],"MAXH=",5)==0)
            modelimit_y = strtoul(lst->item[ii]+5, 0, 0);
         ii++;
      }
      free(lst);
   }

   mref     = (modeinfo*)malloc(sizeof(modeinfo)*(MAXVESA_MODES+PREDEFINED_MODES));
   vesa_cnt = 0;
   memZero(mref);

   if (vesa) {
      vinfo = (VBEModeInfo*) malloc(sizeof(VBEModeInfo)*MAXVESA_MODES);
      memZero(vinfo);
      vesa_cnt = vesadetect(vinfo, mref+PREDEFINED_MODES, modemask, &vesa_memsize);
      if (!vesa_cnt) { free(vinfo); vinfo = 0; }
   }
   // total number of available modes
   mode_cnt = PREDEFINED_MODES + vesa_cnt;
   modes = (con_modeinfo*)malloc(sizeof(con_modeinfo)*(mode_cnt+1+TEXTEMU_MODES));
   memZero(modes);
   // common text modes
   modes[0].width = modes[1].width = modes[2].width = 80;
   modes[0].height = 25; modes[0].font_x = 9; modes[0].font_y = 16; 
   modes[1].height = 43; modes[1].font_x = 8; modes[1].font_y = 8;
   modes[2].height = 50; modes[2].font_x = 8; modes[2].font_y = 8;

   for (ii = PREDEFINED_MODES; ii<mode_cnt; ii++) {
      u32t *fp;
      VBEModeInfo  *vi = vinfo + mref[ii].vesaref;
      con_modeinfo *mi = modes + ii;
      mi->width  = vi->XResolution;
      mi->height = vi->YResolution;

      if (vi->ModeAttributes&VSMI_GRAPHMODE) {
         mi->bits   = vi->NumberOfPlanes * vi->BitsPerPixel;
         mi->flags  = CON_GRAPHMODE;
         // CON_COLOR_XXXX flags
         fp = memchrd(fbits, mi->bits, 8);
         if (fp) mi->flags|= (fp-fbits)<<8;
         if (global_nolfb) mi->flags|=CON_NOLFB;

         if (mi->bits>8) {
            mi->rmask = (1<<vi->RedMaskSize)-1<<vi->RedFieldPosition;
            mi->gmask = (1<<vi->GreenMaskSize)-1<<vi->GreenFieldPosition;
            mi->bmask = (1<<vi->BlueMaskSize)-1<<vi->BlueFieldPosition;
            mi->amask = (1<<vi->RsvdMaskSize)-1<<vi->RsvdFieldPosition;
            // fix wrong BIOS value
            if (mi->bits==16 && mi->rmask==0x7C00 && mi->gmask==0x3E0) mi->bits=15;
         }
         mi->physaddr = vi->PhysBasePtr;
         mi->mempitch = vi->BytesPerScanline;
         mi->memsize  = vesa_memsize;
         // get 800x600 with lower possible bits per pixel
         if (mi->width==800 && mi->height==600 && mi->bits>=8)
            if (mode800<0 || modes[mode800].bits>mi->bits) mode800 = ii;
         // select max resolution mode
         if (modemax<0 && mi->width>800 || modemax>0 && modes[modemax].height<=mi->height
            && (modes[modemax].width<mi->width || modes[modemax].width==mi->width &&
               modes[modemax].bits>mi->bits && mi->bits>=8)) modemax = ii;
      } else {
         mi->physaddr = TEXTMEM_SEG<<4;
         mi->memsize  = 0x8000;
         mi->mempitch = vi->XResolution<<1;
         mi->font_x   = vi->XCharSize;
         mi->font_y   = vi->YCharSize;
      }
   }
   // init current_mode and reset to 80x25 if we`re not in text mode
   con_getmode(0, 0, 0);
   if (mode800>0) {
      con_modeinfo *mi = modes + mode_cnt++;
      mi->flags  = CON_EMULATED;
      mi->font_x = 8;
      mi->font_y = 16;
      mi->width  = 800/mi->font_x;
      mi->height = 600/mi->font_y;
#if 1
      mi = modes + mode_cnt++;
      mi->flags  = CON_EMULATED;
      mi->font_x = 8;
      mi->font_y = 14;
      mi->width  = 800/mi->font_x;
      mi->height = 600/mi->font_y;
#endif
      txemu800   = mode800;
   }
   if (modemax>0) {
      con_modeinfo *mi = modes + mode_cnt++;
      mi->flags  = CON_EMULATED;
      mi->font_x = 9;
      mi->font_y = 16;
      mi->width  = modes[modemax].width /mi->font_x;
      mi->height = modes[modemax].height/mi->font_y;
      txemumax   = modemax;
   }
   for (ii = 0; ii<mode_cnt; ii++) modes[ii].infosize = sizeof(con_modeinfo);
   con_queryfonts();
   con_insthooks();
   lib_ready = 1;
}

int search_mode(u32t x, u32t y, u32t flags) {
   u32t ii;
   flags&=~(CON_NOSCREENCLEAR|CON_NOLFB);
   // isolate known flags
   flags&=0xF0F;
   for (ii=0;ii<mode_cnt;ii++)
      if (modes[ii].width==x && modes[ii].height==y &&
         (modes[ii].flags&~(CON_NOLFB|CON_EMULATED))==flags) return ii;
   return -1;
}

void _std con_getmode(u32t *x, u32t *y, u32t *flags) {
   if (!mode_changed) {
      u32t cols=0, lines=0;
      u8t  istext = vio_getmode(&cols,&lines);
      // if we`re do not know current mode - just change it to 80x25 ;)
      if (!istext) con_setmode(cols=80,lines=25,0);
      if (flags) *flags = 0;
      if (x) *x = cols;
      if (y) *y = lines;
      if (current_mode<0) real_mode = current_mode = search_mode(cols,lines,0);
   } else {
      if (flags) *flags = current_flags;
      if (x) *x = modes[current_mode].width;
      if (y) *y = modes[current_mode].height;
   }
}

con_modeinfo *_std con_currentmode(void) {
   if (current_mode<0) return 0;
   return modes + current_mode;
}

u32t _std con_modeavail(u32t x, u32t y, u32t flags) {
   return search_mode(x,y,flags)>=0;
}

/* free all assigned to current mode arrays */
static void con_unsetmode() {
   if (mode_changed) {
      con_modeinfo *mi = modes + real_mode;
      log_it(3,"con_unsetmode()\n");
      if (mi->physmap) {
         pag_physunmap(mi->physmap);
         mi->physmap = 0;
      }
      if (mi->shadow) {
         hlp_memfree(mi->shadow);
         mi->shadow  = 0;
      }
      mi->shadowpitch = 0;

      evio_shutdown();
   }
}

#if 1  // one min emu
void _std vio_getmodefast(u32t *cols, u32t *lines);

void hook_modeset(void) {
   u32t lines;
   con_unsetmode();
   vio_getmodefast(0,&lines);
   real_mode = current_mode = (lines-25)/12;
   log_it(3,"lines = %d\n",lines);
   mode_changed = 1;
}
#endif

static void *alloc_shadow(con_modeinfo *cmi, int noclear) {
   cmi->shadowpitch = (cmi->flags&CON_GRAPHMODE)==0 ? cmi->width*2 :
      Round4(BytesBP(cmi->bits) * cmi->width);
   // shadow buffer
   cmi->shadow = hlp_memalloc(cmi->shadowpitch * cmi->height, 
       QSMA_RETERR | (noclear?QSMA_NOCLEAR:0));
   if (!cmi->shadow) cmi->shadowpitch = 0;
   return cmi->shadow;
}

u32t __stdcall con_setmode(u32t x, u32t y, u32t flags) {
   if ((flags&CON_GRAPHMODE)==0 && x==80 && (y==25||y==43||y==50)) {
      vio_setmode(y);
      // con_unsetmode() is called from hook here
      current_mode = (y-25)/12;
      real_mode    = current_mode;
      mode_changed = 1;
      return 1;
   } else {
      int  idx = search_mode(x,y,flags),
        actual = idx;
      /* this is an emulated text mode? select graphic mode and force
         "shadow buffer" flag */
      if (idx>=0 && (modes[idx].flags&CON_EMULATED)!=0) {
         actual = modes[idx].width*8==800?txemu800:txemumax;
         flags |= CON_SHADOW;
      }

      if (idx>=0) {
         con_modeinfo *m_pub = modes + idx,    // public mode info
                      *m_act = modes + actual; // actual mode info
         VBEModeInfo  *mi    = vinfo + mref[actual].vesaref;

         if (global_nolfb) flags|=CON_NOLFB;
         // set mode
         if (!vesasetmode(mi,mref[actual].modenum,flags)) return 0;
         // free previous mode data
         con_unsetmode();
         // trying to update mode info
         vesamodeinfo(mi,mref[actual].modenum);
         // and pointers (for graphic/emulated text mode, not real text)
         if ((m_act->flags&CON_GRAPHMODE)!=0) {
            m_act->physaddr = mi->PhysBasePtr;
            m_act->mempitch = mi->BytesPerScanline;
            m_act->physmap  = (u8t*)pag_physmap(mi->PhysBasePtr,
               mi->BytesPerScanline*mi->YResolution,0);
            log_it(3,"%08X %d %d %d\n", mi->PhysBasePtr, mi->BytesPerScanline,
               mi->XResolution, mi->YResolution);
         }
         // allocate shadow buffer (allowed both for text & graphic mode)
         if (flags&CON_SHADOW)
            if (m_act->bits<8 || !alloc_shadow(m_act,flags&CON_NOSCREENCLEAR)) 
               flags&=~CON_SHADOW;

         current_mode   = idx;
         real_mode      = actual;
         current_flags  = flags;
         mode_changed   = 1;
         // setup text emulation
         if ((m_pub->flags&CON_EMULATED)!=0) {
            alloc_shadow(m_pub,0);
            evio_newmode();
         }

         return 1;
      }
   }
   return 0;
}

con_modeinfo *_std con_querymode(u32t *pmodes) {
   if (pmodes) *pmodes = mode_cnt;
   return modes;
}

u32t _std con_write(u32t x, u32t y, u32t dx, u32t dy, void *src, u32t pitch) {
   con_modeinfo *mi;
   if (current_mode<0||!src||!pitch) return 0;
   // mode info
   mi = modes + current_mode;
   // drop bad coordinates
   if ((long)x<0||(long)y<0||(long)dx<0||(long)dy<0) return 0;
   // is rect inside screen?
   if (x>=mi->width || y>=mi->height) return 0;
   if (x+dx>mi->width)  dx=mi->width -x;
   if (y+dy>mi->height) dy=mi->height-y;
   // copying memory
   if ((mi->flags&CON_GRAPHMODE)==0) {
      // normal or emulated text mode?
      if ((mi->flags&CON_EMULATED)==0) {
         u16t *b800 = (u16t*)hlp_segtoflat(TEXTMEM_SEG);
         MoveBlock(src,b800+y*mi->width+x,dx<<1,dy,pitch,mi->width<<1);
         // update shadow buffer
         if (mi->shadow) MoveBlock(src, mi->shadow + y*mi->shadowpitch + x*2,
            dx<<1, dy, pitch, mi->shadowpitch);
      } else
         evio_writebuf(x,y,dx,dy,src,pitch);

      return 1;
   } else {
      return vesamemcpy(modes+current_mode, vinfo+current_mode, x, y, dx, dy,
         src, pitch, 1);
   }
}

u32t _std con_read(u32t x, u32t y, u32t dx, u32t dy, void *dst, u32t pitch) {
   con_modeinfo *mi;
   u32t       flags;
   if (current_mode<0||!dst||!pitch) return 0;
   // mode info
   mi = modes + current_mode;
   // is rect inside screen?
   if (x>=mi->width || y>=mi->height || x+dx>mi->width || y+dy>mi->height) return 0;
   // copying memory
   flags = modes[current_mode].flags;

   if ((flags&CON_GRAPHMODE)==0) {
      // normal or emulated text mode?
      if ((flags&CON_EMULATED)==0) {
         u16t *b800 = (u16t*)hlp_segtoflat(TEXTMEM_SEG);
         MoveBlock(b800+y*mi->width+x,dst,dx<<1,dy,mi->width<<1,pitch);
      } else
         evio_readbuf(x,y,dx,dy,dst,pitch);

      return 1;
   } else {
      return vesamemcpy(modes+current_mode, vinfo+current_mode, x, y, dx, dy,
         dst, pitch, 0);
   }
}

u32t _std shl_mkshot(const char *cmd, str_list *args) {
   int rc = EINVAL;
   if (args->count==1 && strcmp(args->item[0],"/?")==0) {
      cmd_shellhelp(cmd,CLR_HELP);
      return 0;
   }
   cmd_printseq(0,1,0);

   if (args->count>0) {
      char *fp = args->item[0];
      if (stricmp(fp,"OFF")==0) {
         con_removeshot();
         rc = 0;
      } else
      if (stricmp(fp,"ON")==0 && args->count<=2) {
         rc = con_instshot(args->count==2?args->item[1]:0);
      }
   }
   if (rc) cmd_shellerr(rc,0);
   return rc;
}

u32t _std con_handler(const char *cmd, str_list *args) {
   int rc = EINVAL;
   if (args->count>=2) {
      u32t  ii, cnt;
      void *fntfile = 0;
      char      *fp = args->item[1];
      // init pause counter
      cmd_printseq(0,1,0);

      if (stricmp(fp,"LIST")==0) {
         cmd_printf(
            "Available console modes:\n"
            " ÄÄÄÄÂÄÄÄÄÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÄÄÄÄ\n"
            "  ## ³ mode size ³ font size \n"
            " ÄÄÄÄÅÄÄÄÄÄÂÄÄÄÄÄÅÄÄÄÄÄÄÄÄÄÄÄ\n");

         for (ii=0, cnt=0; ii<mode_cnt; ii++) {
            con_modeinfo *mi = modes + ii;
            if ((mi->flags&CON_GRAPHMODE)==0) {
               char etext[64];
               // emulated mode info
               if ((mi->flags&CON_EMULATED)==0) etext[0] = 0; else {
                  u32t mnum = mi->width*8==800?txemu800:txemumax;
                  sprintf(etext,"(emulated in %ux%u)", modes[mnum].width,
                     modes[mnum].height);
               }
               // print with pause
               cmd_printf(" %3u ³%4u |%4u ³%3u x %u  %s\n", ++cnt, mi->width,
                  mi->height,  mi->font_x, mi->font_y, etext);
            }
         }
         rc = 0;
      } else
      if (stricmp(fp,"STATUS")==0) {
         static const u16t cps[] = {20,21,23,25,27,30,33,37,40,42,46,50,54,60,
            66,75,80,85,92,100,109,120,133,150,160,171,184,200,218,240,266,300};
         u8t  rate, delay;
         u32t   xx, yy;

         key_getspeed(&rate, &delay);
         con_getmode(&xx, &yy, 0);

         rate = 0x1F - (rate&0x1F);

         cmd_printf(" Screen cols    : %u\n"
                    " Screen lines   : %u\n", xx, yy);
         cmd_printf(" Keyboard rate  : %u.%u characters/sec\n"
                    " Keyboard delay : %u ms\n", cps[rate]/10UL,
                    cps[rate]%10UL, (u32t)(delay+1)*250);
         rc = 0;
      } else {
         u32t cols=0, lines=0, rate=FFFF, delay=0,
              fntx=0, fnty=0;
         ii = 1;
         while (ii<args->count) {
            if (strnicmp(args->item[ii],"COLS=",5)==0)
               cols  = strtoul(args->item[ii]+5, 0, 0); else
            if (strnicmp(args->item[ii],"LINES=",6)==0)
               lines = strtoul(args->item[ii]+6, 0, 0); else
            if (strnicmp(args->item[ii],"RATE=",5)==0)
               rate  = strtoul(args->item[ii]+5, 0, 0); else
            if (strnicmp(args->item[ii],"DELAY=",6)==0)
               delay = strtoul(args->item[ii]+6, 0, 0); else
            if (strnicmp(args->item[ii],"FONT=",5)==0) {
               // check FONT=x,y,file parameter
               char *cp = args->item[ii]+5;
               if (strccnt(cp,',')==2) {
                  str_list* fsp = str_split(cp,",");
                  if (fsp->count==3) {
                     fntx = strtoul(fsp->item[0], 0, 0);
                     fnty = strtoul(fsp->item[1], 0, 0);

                     if (!fntx || fntx>32) {
                        cmd_printf("Invalid font WIDTH value\n");
                     } else
                     if (!fnty || fnty>128) {
                        cmd_printf("Invalid font HEIGHT value\n");
                     } else {
                        u32t fntsize;
                        fntfile = freadfull(fsp->item[2],&fntsize);
                        if (fntfile) {
                           u32t reqs = BytesPerFont(fntx) * fnty * 256;
                           if (fntsize!=reqs) {
                              cmd_printf("Font file size mismatch (%d instead of %d)\n",
                                 fntsize, reqs);
                              hlp_memfree(fntfile);
                              fntfile = 0;
                           }
                        }
                     }
                  }
                  free(fsp);
               }
               if (!fntfile) { ii = 0; break; }
            } else { ii = 0; break; }
            ii++;
         }
         if (ii) {
            if (fntx && fnty && fntfile) {
               con_fontadd(fntx, fnty, fntfile);
               hlp_memfree(fntfile);
               fntfile = 0;
            }
            if (cols || lines) {
               u32t oldcols, oldlines;
               con_getmode(&oldcols, &oldlines, 0);
               if (!cols)  cols  = oldcols;
               if (!lines) lines = oldlines;

               if (!con_modeavail(cols,lines,0))
                  cmd_printf("There is no such mode.\n");
               else
                  con_setmode(cols, lines, 0);
               rc = 0;
            }
            if (rate!=FFFF || delay) {
               u8t oldrate, olddelay;
               key_getspeed(&oldrate, &olddelay);
               if (rate==FFFF) rate = oldrate;
               delay = delay ? delay/250-1 : olddelay;

               if (rate>31 || delay%250 || delay/250>4)
                  cmd_printf("Invalid kerboard mode parameters.\n");
               else
                  key_speed(rate, delay);
               rc = 0;
            }
         }
      }
      if (fntfile) hlp_memfree(fntfile);
   }
   if (rc) cmd_shellerr(rc,0);
   return rc;
}

void on_exit(void) {
   if (!lib_ready) return;
   /* reset mode if it was changed, installed hook will also make call to
      con_unsetmode() */
   if (mode_changed) { 
      vio_resetmode(); 
      mode_changed = 0; 
   }
   if (modes) { free(modes); modes=0; }
   if (vinfo) { free(vinfo); vinfo=0; }
   if (mref)  { free(mref); mref=0; }
   evio_shutdown();
   con_removeshot();
   cmd_shellrmv("MKSHOT",shl_mkshot);
   cmd_modermv("CON",con_handler);
   con_freefonts();
   con_removehooks();

   vesa_cnt = 0;
   mode_cnt = 0;
   lib_ready = 0;
}

unsigned __cdecl LibMain( unsigned hmod, unsigned termination ) {
   if (!termination) {
      if (mod_query(selfname,MODQ_LOADED|MODQ_NOINCR)) {
         log_printf("%s already loaded!\n",selfname);
         return 0;
      }
      con_init();
      exit_handler(&on_exit,1);
      // install screenshot shell command
      cmd_shelladd("MKSHOT",shl_mkshot);
      // install "mode con" shell command
      cmd_modeadd("CON",con_handler);
      // we are ready! ;)
      log_printf("console.dll is loaded!\n");
   } else {
      exit_handler(&on_exit,0);
      on_exit();
      log_printf("console.dll unloaded!\n");
   }
   return 1;
}
