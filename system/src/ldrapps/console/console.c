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

int          lib_ready =  0; // lib is ready
const char   *selfname = MODNAME_CONSOLE;
int       mode_changed =  0; // mode was changed by lib, at least once
con_modeinfo    *modes =  0; // public mode info array
con_intinfo      *mref =  0; // internal mode info array
u32t          mode_cnt =  0; // number of modes available
int       current_mode = -1, // current mode index in mode info
             real_mode = -1; // current REAL mode index in mode info
u32t     current_flags =  0; // current mode flags
int           textonly =  0;
u32t       modelimit_x =  0,
           modelimit_y =  0,
      enabled_modemask = 0x5F,
        fbaddr_enabled =  1; // "physmap" enabled (EFI) / LFB enabled (BIOS)
u32t       graph_modes =  0; // number of available graphic modes
u32t        mode_limit =  0; // size of modeinfo array (it ptr assumed as const)

platform_setmode     pl_setmode = 0;
platform_leavemode pl_leavemode = 0;
platform_copy           pl_copy = 0;
platform_flush         pl_flush = 0;
platform_scroll       pl_scroll = 0;
platform_clear         pl_clear = 0;
platform_addfonts    pl_addfont = 0;
platform_dirblit     pl_dirblit = 0;
platform_dirclear   pl_dirclear = 0;
platform_setup         pl_setup = 0;
platform_close         pl_close = 0;

void _std vio_getmodefast(u32t *cols, u32t *lines);

void con_init(void) {
   char *vesakey = getenv("VESA");
   int   modemax = -1;   // max resolution mode for text emulation
   u32t       ii;

   /* parse env key:
      "VESA = ON [, MODES = 0x0F][, MAXW = 1024][, MAXH = 768][, NOFB]" */
   if (vesakey) {
      str_list *lst = str_split(vesakey,",");
      int      vesa = env_istrue("VESA");
      if (vesa<=0) textonly = 1;
      
      ii = 1;
      while (ii<lst->count) {
         char *lp = lst->item[ii];
         if (strnicmp(lp,"MODES=",6)==0) enabled_modemask = strtoul(lp+6, 0, 0);
            else
         if (strnicmp(lp,"MAXW=",5)==0) modelimit_x = strtoul(lp+5, 0, 0);
            else
         if (strnicmp(lp,"MAXH=",5)==0) modelimit_y = strtoul(lp+5, 0, 0);
            else
         if (stricmp(lp,"NOFB")==0) fbaddr_enabled = 0;

         ii++;
      }
      free(lst);
   }
   // mref array allocated here, but "modes" - in platform code
   mref = (con_intinfo*)malloc(sizeof(con_intinfo)*(MAXVESA_MODES+PREDEFINED_MODES+MAXEMU_MODES+1));
   memZero(mref);
   // call platform mode detection (BIOS of EFI)
   pl_setup();
   // init current_mode and reset to 80x25 if we`re not in text mode
   con_getmode(0, 0, 0);

   graph_modes = 0;
   for (ii = 0; ii<mode_cnt; ii++) {
      modes[ii].infosize = sizeof(con_modeinfo);
      if ((modes[ii].flags&CON_GRAPHMODE)!=0) graph_modes++;
   }
   con_queryfonts();

   con_addtextmode(8,16,800,600);
   con_addtextmode(8,14,800,600);
   // select max resolution mode
   for (ii=0; ii<mode_cnt; ii++) {
      con_modeinfo *mi = modes + ii;
      if ((mi->flags&CON_GRAPHMODE)!=0)
         if (modemax<0 && mi->width>800 || modemax>0 && modes[modemax].height<=mi->height
            && (modes[modemax].width<mi->width || modes[modemax].width==mi->width &&
               modes[modemax].bits>mi->bits && mi->bits>=8)) modemax = ii;
   }
   // and add font to it
   if (modemax>0) con_addtextmode(9,14,modes[modemax].width,modes[modemax].height);

   con_insthooks();
   lib_ready = 1;
}

int search_mode(u32t x, u32t y, u32t flags) {
   u32t ii, idx,
     index = (flags&CON_INDEX_MASK)>>16;
   flags  &=~(CON_NOSCREENCLEAR);
   // isolate known flags
   flags  &= 0xF0F;
   for (ii=0,idx=0; ii<mode_cnt; ii++)
      if (modes[ii].width==x && modes[ii].height==y && (modes[ii].flags&
         ~(CON_EMULATED))==flags) 
             if (idx++==index) return ii;
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

int bpp2bit(u32t bpp) {
   static u32t bbits[] = {8, 15, 16, 24, 32, 1, 4, 0};
   u32t    *bp = bbits;
   int  mvalue = 1;
   while (*bp)
      if (*bp==bpp) break; else { bp++; mvalue<<=1; }
   // unknown number of pixels
   if (!*bp) return -1;
   return mvalue;
}

con_modeinfo *_std con_currentmode(void) {
   if (current_mode<0) return 0;
   return modes + current_mode;
}

u32t _std con_modeavail(u32t x, u32t y, u32t flags) {
   return search_mode(x,y,flags)>=0;
}

/* free all assigned to current mode arrays */
void con_unsetmode() {
   if (mode_changed) {
      con_modeinfo *mi = 0;
      log_it(3,"con_unsetmode()\n");
      evio_shutdown();
      /* one pass if real_mode==current_mode, else 2 passes */
      do {
         mi = modes + (mi?current_mode:real_mode);

         if (mi->shadow) {
            hlp_memfree(mi->shadow);
            mi->shadow  = 0;
         }
         mi->shadowpitch = 0;
      } while (mi!=modes+current_mode);
      // call platform specific
      pl_leavemode();
   }
}

/* this hook called after vio_setmode() on any host type, but only for
   native console modes (only these modes turned on by hooked vio calls) */
void hook_modeset(void) {
   u32t   cols, lines, ii;
   con_unsetmode();
   vio_getmodefast(&cols, &lines);

   for (ii=0; ii<mode_cnt; ii++)
      if (modes[ii].width==cols && modes[ii].height==lines &&
         (modes[ii].flags&(CON_EMULATED|CON_GRAPHMODE))==0)
            { current_mode = real_mode = ii; break; }
   log_it(3,"m:%d, %dx%d\n", real_mode, cols, lines);
   mode_changed = 1;
}

void *alloc_shadow(con_modeinfo *cmi, int noclear) {
   cmi->shadowpitch = (cmi->flags&CON_GRAPHMODE)==0 ? cmi->width*2 :
      Round4(BytesBP(cmi->bits) * cmi->width);
   // shadow buffer
   cmi->shadow = hlp_memallocsig(cmi->shadowpitch * cmi->height, "con",
       QSMA_RETERR | (noclear?QSMA_NOCLEAR:0));
   if (!cmi->shadow) cmi->shadowpitch = 0;
   return cmi->shadow;
}

u32t __stdcall con_setmode(u32t x, u32t y, u32t flags) {
   if (!x||!y) return 0;
   // modify CON_EMULATED flag to actual value
   if ((flags&CON_GRAPHMODE)==0) {
      int modeidx = search_mode(x, y, flags);
      if (modeidx<0) return 0;
      // mode exist, check flag match
      if ((modes[modeidx].flags&CON_EMULATED^flags&CON_EMULATED)) flags^=CON_EMULATED;
   }
   return pl_setmode(x,y,flags);
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
   return pl_copy(current_mode, x, y, dx, dy, src, pitch, 1);
}

u32t _std con_clear(u32t x, u32t y, u32t dx, u32t dy, u32t color) {
   con_modeinfo *mi;
   if (current_mode<0) return 0;
   // mode info
   mi = modes + current_mode;
   // drop bad coordinates
   if ((long)x<0||(long)y<0||(long)dx<0||(long)dy<0) return 0;
   // is rect inside screen?
   if (x>=mi->width || y>=mi->height) return 0;
   if (x+dx>mi->width)  dx=mi->width -x;
   if (y+dy>mi->height) dy=mi->height-y;
   // clear memory
   return pl_clear(current_mode, x, y, dx, dy, color);
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
   return pl_copy(current_mode, x, y, dx, dy, dst, pitch, 0);
}


int _std con_addtextmode(u32t fntx, u32t fnty, u32t modex, u32t modey) {
   u32t charx, chary, ii;
   con_modeinfo      *mi;
   int   midx;
   if (!fntx || fntx>32 || !fnty || fnty>128 || !modex || !modey) return EINVAL;

   for (ii=CON_COLOR_8BIT; ii<=CON_COLOR_32BIT; ii+=0x100)
      if ((midx=search_mode(modex, modey, CON_GRAPHMODE|ii))>=0)
         break;
   if (midx<0) return ENODEV;

   if (!con_fontavail(fntx,fnty)) return ENOTTY;

   charx = modex / fntx,
   chary = modey / fnty;

   // check for the same resolutions (even with different BPP)
   for (ii=0; ii<mode_cnt; ii++)
      if (modes[ii].width==charx && modes[ii].height==chary &&
         (modes[ii].flags&(CON_EMULATED|CON_GRAPHMODE))==CON_EMULATED &&
            modes[mref[ii].realmode].width==modex &&
               modes[mref[ii].realmode].height==modey) return EEXIST;
   if (mode_cnt >= mode_limit-1) return ENOSPC;
   // add a new one
   mi = modes + mode_cnt;
   mi->infosize = sizeof(con_modeinfo);
   mi->flags    = CON_EMULATED;
   mi->font_x   = fntx;
   mi->font_y   = fnty;
   mi->width    = charx;
   mi->height   = chary;
   mref[mode_cnt++].realmode = midx;
   // done
   return 0;
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
   int rc = -1;
   if (args->count>=2) {
      u32t  ii, cnt;
      char      *fp = args->item[1];
      // init pause counter
      cmd_printseq(0,1,0);

      if (stricmp(fp,"LIST")==0) {
         cmd_printf(
            " Available console modes:\n"
            " ÄÄÄÄÄÂÄÄÄÄÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÄÄÄÄ\n"
            "   ## ³ mode size ³ font size \n"
            " ÄÄÄÄÄÅÄÄÄÄÄÂÄÄÄÄÄÅÄÄÄÄÄÄÄÄÄÄÄ\n");
                
         for (ii=0, cnt=0; ii<mode_cnt; ii++) {
            con_modeinfo *mi = modes + ii;
            if ((mi->flags&CON_GRAPHMODE)==0) {
               char etext[64];
               // emulated mode info
               if ((mi->flags&CON_EMULATED)==0) etext[0] = 0; else {
                  u32t mnum = mref[ii].realmode;
                  sprintf(etext,"(emulated in %ux%u)", modes[mnum].width,
                     modes[mnum].height);
               }
               vio_setcolor(VIO_COLOR_LGREEN);
               printf(ii==current_mode ? " \x1A" : "  ");
               vio_setcolor(VIO_COLOR_RESET);
               // print with pause
               cmd_printf("%3u ³%4u |%4u ³%3u x %u  %s\n", ++cnt, mi->width,
                  mi->height,  mi->font_x, mi->font_y, etext);
            }
         }
         rc = 0;
      } else
      if (stricmp(fp,"GMLIST")==0) {
         int verbose = 0;
         if (args->count==3 && stricmp(args->item[2],"/v")==0) verbose = 1;
            else
         if (args->count>2) verbose = -1;

         if (graph_modes==0) {
            cmd_printf("There is no graphic modes available\n");
            rc = 0;
         } else
         if (verbose==0) {
            dq_list lst = NEW(dq_list);
            // get list of modes and sort it by HEIGHT / WIDTH
            for (ii=0; ii<mode_cnt; ii++) {
               con_modeinfo *mi = modes + ii;
               if ((mi->flags&CON_GRAPHMODE)!=0 && mi->width<0xFFFF && mi->height<0xFFFF) {
                  int bit = bpp2bit(mi->bits);
                  if (bit>=0) {
                     u64t va = (u64t)mi->height<<48|(u64t)mi->width<<32;
                     // search for the same resolution
                     for (cnt=0; cnt<lst->count(); cnt++)
                        if ((lst->value(cnt)&0xFFFFFFFF00000000)==va) {
                           lst->array()[cnt]|=(u64t)bit;
                           bit = 0;
                           break;
                        }
                     // if not exist - add a new one
                     if (bit) lst->add(va|(u64t)bit);
                  }
               }
            }
            // sort it to make it nice ;)
            lst->sort(0,1);

            cmd_printf(
               " Available graphic modes:\n"
               " ÄÄÄÄÄÄÄÄÄÄÄÄÄÂÄÄÄÄÄÂÄÄÄÄÄÂÄÄÄÄÄÂÄÄÄÄÄÂÄÄÄÄÄÂÄÄÄÄÄ\n"
               "   mode size  ³  4  ³  8  ³  15 ³  16 ³  24 ³  32 \n"
               " ÄÄÄÄÄÄÂÄÄÄÄÄÄÅÄÄÄÄÄÅÄÄÄÄÄÅÄÄÄÄÄÅÄÄÄÄÄÅÄÄÄÄÄÅÄÄÄÄÄ\n");
            for (cnt=0; cnt<lst->count(); cnt++) {
               u64t va = lst->value(cnt);
               u8t  bv = (u8t)va;
               cmd_printf(" %5u |%5u ³  %c  ³  %c  ³  %c  ³  %c  ³  %c  ³  %c\n",
                  (u32t)(va>>32&0xFFFF), (u32t)(va>>48&0xFFFF),
                     bv&0x40?'*':' ', bv&1?'*':' ', bv&2?'*':' ', bv&4?'*':' ',
                         bv&8?'*':' ',  bv&0x10?'*':' ');
            }
            DELETE(lst);
            rc = 0;
         } else
         if (verbose>0) {
            cmd_printf(
               " Available graphic modes:\n"
               " ÄÄÄÂÄÄÄÄÄÄÄÄÄÄÄÄÄÂÄÄÄÄÂÄÄÄÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÄÄÄ\n"
               " ## ³  mode size  ³bits³    red   ³   green  ³   blue   ³  alpha   \n"
               " ÄÄÄÅÄÄÄÄÄÄÂÄÄÄÄÄÄÅÄÄÄÄÅÄÄÄÄÄÄÄÄÄÄÅÄÄÄÄÄÄÄÄÄÄÅÄÄÄÄÄÄÄÄÄÄÅÄÄÄÄÄÄÄÄÄÄ\n");
            for (ii=0, cnt=0; ii<mode_cnt; ii++) {
               char rgbuf[64];
               con_modeinfo *mi = modes + ii;
               if ((mi->flags&CON_GRAPHMODE)!=0) {
                  if (mi->bits>8) {
                     int pos;
                     if (mi->bits>16) {
                        pos = snprintf(rgbuf, 64, "%08X ³ %08X ³ %08X ³ ",
                           mi->rmask, mi->gmask, mi->bmask);
                        if (mi->amask) sprintf(rgbuf + pos, "%08X", mi->amask);
                     } else {
                        pos = snprintf(rgbuf, 64, "    %04X ³     %04X ³     %04X ³ ",
                           mi->rmask, mi->gmask, mi->bmask);
                        if (mi->amask) sprintf(rgbuf + pos, "    %04X", mi->amask);
                     }
                  } else
                     strcpy(rgbuf,"         ³          ³          ³");

                  // print with pause
                  cmd_printf("%3u ³%5u |%5u ³ %2u ³ %s\n", ++cnt,
                     mi->width, mi->height, mi->bits, rgbuf);
               }
            }
            rc = 0;
         }
      } else
      if (stricmp(fp,"STATUS")==0) {
         static const u16t cps[] = {20,21,23,25,27,30,33,37,40,42,46,50,54,60,
            66,75,80,85,92,100,109,120,133,150,160,171,184,200,218,240,266,300};
         u8t  rate, delay;
         u32t   xx, yy;

         key_getspeed(&rate, &delay);
         con_getmode(&xx, &yy, 0);

         rate = 0x1F - (rate&0x1F);

         cmd_printf("Screen cols    : %u\n"
                    "Screen lines   : %u\n", xx, yy);
         cmd_printf("Keyboard rate  : %u.%u characters/sec\n"
                    "Keyboard delay : %u ms\n", cps[rate]/10UL,
                    cps[rate]%10UL, (u32t)(delay+1)*250);
         rc = 0;
      } else {
         u32t cols=0, lines=0, rate=FFFF, delay=0, index=0;
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
            if (strnicmp(args->item[ii],"INDEX=",6)==0) {
               index = strtoul(args->item[ii]+6, 0, 0); 
            } else
            if (strnicmp(args->item[ii],"ADD=",4)==0) {
               // check add=fx,fy,mx,my parameter
               char *cp = args->item[ii]+4;
               if (strccnt(cp,',')==3) {
                  str_list* fsp = str_split(cp,",");
                  if (fsp->count==4) {
                     u32t  fntx = strtoul(fsp->item[0], 0, 0),
                           fnty = strtoul(fsp->item[1], 0, 0),
                             mx = strtoul(fsp->item[2], 0, 0),
                             my = strtoul(fsp->item[3], 0, 0);

                     rc = con_addtextmode(fntx, fnty, mx, my);
                     switch (rc) {
                        case EINVAL:
                           cmd_printf("Invalid font size specified\n");
                           break;
                        case ENODEV:
                           cmd_printf("There is no suitable graphic mode with resolution %dx%d\n", mx, my);
                           break;
                        case EEXIST:
                           cmd_printf("Mode with the same resolution already exists!\n");
                           break;
                        case ENOTTY:
                           cmd_printf("There is no installed font of size %dx%d\n", fntx, fnty);
                           break;
                     }
                  }
                  free(fsp);
               }
            } else
            if (strnicmp(args->item[ii],"FONT=",5)==0) {
               // check FONT=x,y,file parameter
               void *fntfile = 0;
               char      *cp = args->item[ii]+5;
               u32t     fntx = 0, fnty = 0;

               if (strccnt(cp,',')==2) {
                  str_list* fsp = str_split(cp,",");
                  if (fsp->count==3) {
                     fntx = strtoul(fsp->item[0], 0, 0);
                     fnty = strtoul(fsp->item[1], 0, 0);

                     if (!fntx || fntx>32) {
                        cmd_printf("Invalid font WIDTH value\n");
                        rc = EINVAL;
                     } else
                     if (!fnty || fnty>128) {
                        cmd_printf("Invalid font HEIGHT value\n");
                        rc = EINVAL;
                     } else {
                        u32t fntsize;
                        fntfile = freadfull(fsp->item[2],&fntsize);
                        if (fntfile) {
                           u32t reqs = BytesPerFont(fntx) * fnty * 256;
                           if (fntsize!=reqs) {
                              cmd_printf("Font file size mismatch (%d instead of %d)\n",
                                 fntsize, reqs);
                              hlp_memfree(fntfile);
                              fntfile = 0; rc = EINVAL;
                           }
                        }
                     }
                  }
                  free(fsp);
               }
               if (!fntfile) { ii=0; break; } else {
                  con_fontadd(fntx, fnty, fntfile);
                  hlp_memfree(fntfile);
                  fntfile = 0;
                  rc = 0;
               }
            } else { ii = 0; break; }
            ii++;
         }
         if (ii) {
            if (cols || lines) {
               u32t oldcols, oldlines, 
                      flags = index<<16&CON_INDEX_MASK;
               con_getmode(&oldcols, &oldlines, 0);
               if (!cols)  cols  = oldcols;
               if (!lines) lines = oldlines;

               if (!con_modeavail(cols, lines, flags))
                  cmd_printf("There is no such mode.\n");
               else
               if (!con_setmode(cols, lines, flags))
                  cmd_printf("Error while setting specified mode (%dx%d).\n", cols, lines);
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
   }
   // show message only if error code was not set above
   if (rc<0) cmd_shellerr(rc=EINVAL,0);
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
   con_removeshot();
   cmd_shellrmv("MKSHOT",shl_mkshot);
   cmd_modermv("CON",con_handler);
   evio_shutdown();
   con_freefonts();
   con_removehooks();
   if (modes) { free(modes); modes=0; }
   if (mref)  { free(mref); mref=0; }
   pl_close();

   mode_cnt = 0;
   lib_ready = 0;
}

unsigned __cdecl LibMain( unsigned hmod, unsigned termination ) {
   if (!termination) {
      u32t  host;
      if (mod_query(selfname,MODQ_LOADED|MODQ_NOINCR)) {
         log_printf("%s already loaded!\n",selfname);
         return 0;
      }
      // select output type and init appropriate code
      host = hlp_hosttype();

      if (host==QSHT_EFI) {
         if (!plinit_efi()) return 0;
      } else
      if (host==QSHT_BIOS) {
         if (!plinit_vesa()) return 0;
      } else {
         log_printf("Unknown host type!\n");
         return 0;
      }
      con_init();
      exit_handler(&on_exit,1);
      // install screenshot shell command
      cmd_shelladd("MKSHOT",shl_mkshot);
      // install "mode con" shell command
      cmd_modeadd("CON",con_handler);
      // exec shell commands on load
      cmd_execint(selfname,0);
      // we are ready! ;)
      log_printf("console.dll is loaded!\n");
   } else {
      exit_handler(&on_exit,0);
      on_exit();
      log_printf("console.dll unloaded!\n");
   }
   return 1;
}
