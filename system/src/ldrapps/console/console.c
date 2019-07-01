//
// QSINIT
// screen support dll
//
#include "stdlib.h"
#include "qcl/qsextdlg.h"
#include "conint.h"
#include "vioext.h"
#include "errno.h"
#include "pci.h"

const char   *selfname = MODNAME_CONSOLE;
int       mode_changed =  0; // mode was changed by lib, at least once
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
u32t        mode_limit =  0; // size of modeinfo array
u32t     fbaddr_forced =  0, // forced memory address
           fbaddr_size =  0; // forced memory size
u32t         vmem_addr =  0, // final detected/forced memory address
             vmem_size =  0;
vio_handler      *cvio =  0;
int          in_native =  0;   /// native console mode active (vh_vio)
u32t    setmode_locked =  0;

platform_setmode     pl_setmode = 0;
platform_setmodeid pl_setmodeid = 0;
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

modeinfo*      modes[MAX_MODES];

/** Query video memroy size and address.
    @param  [out] fbaddr   Video memory address, can be 0. Returned value may
                           be zero too - if address is unknown.
    @return memory size in bytes.*/
u32t _std con_vmemsize(u64t *fbaddr) {
   if (fbaddr) *fbaddr = vmem_addr;
   return vmem_size;
}

static void forced_fb_size(const char *str) {
   char     *err = 0;
   fbaddr_forced = strtoul(str, &err, 16);
   fbaddr_size   = 0;

   if (err || fbaddr_forced<sys_endofram()) {
      fbaddr_forced = 0;
      log_it(2, "(invalid) FBADDR value ignored\n");
   } else {
      pci_location dev;
      int        index = hlp_pcifindaddr(fbaddr_forced, &dev);
      if (index-->0) {
         u64t  bases[6], 
               sizes[6];
         u32t   bacount = hlp_pcigetbase(&dev, bases, sizes);
         if (bacount>index)
            if ((bases[index]&PCI_ADDR_SPACE)==PCI_ADDR_SPACE_MEMORY && bases[index]<FFFF)
               fbaddr_size = sizes[index];
      }
      if (!fbaddr_size) {
         log_it(2, "fbaddr forced to %08X, size unknown\n", fbaddr_forced);
         fbaddr_size = _16MB;
      } else
         log_it(2, "fbaddr forced to %08X, size %ukb\n", fbaddr_forced,
            fbaddr_size>>10);
      fbaddr_enabled = 1;
   }
}

int con_init(void) {
   char *vesakey = getenv("VESA");
   u32t       ii;

   /* parse env key:
      "VESA = ON [, MODES = 0x0F][, MAXW = 1024][, MAXH = 768][, NOFB][, FBADDR=...]" */
   if (vesakey) {
      str_list *lst = str_split(vesakey,",");
      int      vesa = env_istrue("VESA");
      if (vesa<=0) textonly = 1;

      ii = 1;
      while (ii<lst->count) {
         char *lp = lst->item[ii];
         if (strnicmp(lp,"MODES=",6)==0) enabled_modemask = strtoul(lp+6, 0, 0);
            else
         if (strnicmp(lp,"MAXW=" ,5)==0) modelimit_x = strtoul(lp+5, 0, 0);
            else
         if (strnicmp(lp,"MODEX=",6)==0) modelimit_x = strtoul(lp+6, 0, 0);
            else
         if (strnicmp(lp,"MAXH=" ,5)==0) modelimit_y = strtoul(lp+5, 0, 0);
            else
         if (strnicmp(lp,"MODEY=",6)==0) modelimit_y = strtoul(lp+6, 0, 0);
            else
         if (stricmp(lp,"NOFB")==0) fbaddr_enabled = 0;
            else
         if (strnicmp(lp,"FBADDR=",7)==0) forced_fb_size(lp+7);

         ii++;
      }
      free(lst);
   }
   /* call platform mode detection (BIOS of EFI).
      At least VESA code uses disk buffer for own needs, so lock it! */
   mt_swlock();
   pl_setup();
   mt_swunlock();

   graph_modes = 0;
   for (ii = 0; ii<mode_cnt; ii++)
      if ((modes[ii]->flags&CON_GRAPHMODE)!=0) graph_modes++;
   con_queryfonts();

   if (!con_catchvio()) return 0;
   return 1;
}

int search_mode(u32t x, u32t y, u32t flags) {
   u32t ii, idx,
     index = (flags&CON_INDEX_MASK)>>16;
   // isolate known flags
   flags  &= 0xF00|CON_GRAPHMODE;

   for (ii=0,idx=0; ii<mode_cnt; ii++) {
#if 0
      if (modes[ii]->width==x && modes[ii]->height==y)
         log_it(3, "search_mode(%u,%u) ==> %X %X\n", x, y, modes[ii]->flags, flags);
#endif
      if (modes[ii]->width==x && modes[ii]->height==y &&
         (modes[ii]->flags&~(CON_EMULATED|CON_SHADOW))==flags)
             if (idx++==index) return ii;
   }
   return -1;
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

/* free all assigned to current mode arrays */
void con_unsetmode() {
   if (mode_changed) {
      modeinfo *mi = 0;
      log_it(3,"con_unsetmode()\n");
      evio_shutdown();
      /* one pass if real_mode==current_mode, else 2 passes */
      do {
         mi = modes[mi?current_mode:real_mode];

         if (mi->shadow) {
            hlp_memfree(mi->shadow);
            mi->shadow  = 0;
         }
         mi->shadowpitch = 0;
      } while (mi!=modes[current_mode]);
      // call platform specific
      pl_leavemode();
   }
}

int alloc_shadow(modeinfo *cmi, int noclear) {
   cmi->shadowpitch = (cmi->flags&CON_GRAPHMODE)==0 ? cmi->width*2 :
      Round4(BytesBP(cmi->bits) * cmi->width);
   // shadow buffer
   cmi->shadow = hlp_memallocsig(cmi->shadowpitch * cmi->height, "con",
       QSMA_RETERR | (noclear?QSMA_NOCLEAR:0));
   if (!cmi->shadow) {
      log_it(1, "no memory for shadow buffer: %ux%ux%u (%u kb)\n", cmi->width,
         cmi->height, cmi->bits, cmi->shadowpitch * cmi->height>>10);
      cmi->shadowpitch = 0;
   }
   return cmi->shadow?1:0;
}

u32t __stdcall con_setmode(u32t x, u32t y, u32t flags) {
   if (!x||!y) return 0;
   // modify CON_EMULATED flag to actual value
   if ((flags&CON_GRAPHMODE)==0) {
      int modeidx = search_mode(x, y, flags);
      if (modeidx<0) return 0;
      // mode exist, check flag match
      if ((modes[modeidx]->flags&CON_EMULATED^flags&CON_EMULATED)) flags^=CON_EMULATED;
   }
   return pl_setmode(x,y,flags);
}

con_mode_info* _std con_gmlist(void) {
   con_mode_info *rc = 0;
   u32t      ii, idx;

   rc = (con_mode_info*)malloc_th(sizeof(con_mode_info)*mode_cnt + 4);
   mem_zero(rc);
   /* mode_cnt can grow by "emulated" modes only - so it is safe to lock MT
      after malloc() */
   mt_swlock();

   for (ii=0, idx=0; ii<mode_cnt; ii++) {
      modeinfo  *ci = modes[ii];

      if ((ci->flags&CON_EMULATED)==0) {
         con_mode_info *mi = rc+idx;

         mi->size    = sizeof(con_mode_info);
         mi->mx      = ci->width;
         mi->my      = ci->height;
         mi->mode_id = ii+1;
         if ((ci->flags&CON_GRAPHMODE)==0) {
            mi->pitch   = mi->mx<<1;
         } else {
            mi->pitch   = ci->mempitch;
            mi->bits    = ci->bits;
            mi->flags   = VMF_GRAPHMODE|(ci->bits==8?VMF_VGAPALETTE:0);
            mi->rmask   = ci->rmask;
            mi->gmask   = ci->gmask;
            mi->bmask   = ci->bmask;
            mi->amask   = ci->amask;
         }
         idx++;
      }
   }
   mt_swunlock();

   // realloc output buffer to smaller size
   if (idx<mode_cnt)
      rc = (con_mode_info*)realloc(rc, sizeof(con_mode_info)*idx + 4);
   // change owner to process
   mem_localblock(rc);

   return rc;
}

qserr _std con_exitmode(u32t modeid) {
   qserr rc = E_CON_BADMODEID;
   mt_swlock();
   if (modeid) {
      rc = pl_setmodeid(modeid-1)?0:E_CON_MODERR;
      if (!rc) setmode_locked = modeid;
   }
   mt_swunlock();
   return rc;
}

int con_addtextmode(u32t fntx, u32t fnty, u32t modex, u32t modey) {
   u32t   charx, chary, ii;
   modeinfo        *mi;
   int            midx;
   if (!fntx || fntx>32 || !fnty || fnty>128 || !modex || !modey) return EINVAL;
   if (mode_cnt >= MAX_MODES) return ENOSPC;

   for (ii=CON_COLOR_8BIT; ii<=CON_COLOR_32BIT; ii+=0x100)
      if ((midx=search_mode(modex, modey, CON_GRAPHMODE|ii))>=0)
         break;
   if (midx<0) return ENODEV;

   if (!con_fontavail(fntx,fnty)) return ENOTTY;

   charx = modex / fntx,
   chary = modey / fnty;

   // check for the same resolutions (even with different BPP)
   for (ii=0; ii<mode_cnt; ii++)
      if (modes[ii]->width==charx && modes[ii]->height==chary &&
         (modes[ii]->flags&(CON_EMULATED|CON_GRAPHMODE))==CON_EMULATED &&
            modes[modes[ii]->realmode]->width==modex &&
               modes[modes[ii]->realmode]->height==modey) return EEXIST;
   mt_swlock();
   // add a new one
   mi = (modeinfo*)malloc(sizeof(modeinfo));
   memset(mi, 0, sizeof(modeinfo));
   modes[mode_cnt] = mi;

   mi->flags    = CON_EMULATED;
   mi->font_x   = fntx;
   mi->font_y   = fnty;
   mi->width    = charx;
   mi->height   = chary;
   mi->realmode = midx;
   mode_cnt++;
   mt_swunlock();
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
   if (rc) cmd_shellerr(EMSG_CLIB,rc,0);
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
         vio_mode_info *ml = vio_modeinfo(0, 0, 0), *mp = ml;
         u32t      svstate, sesx = 0, sesy = 0,
                      cmid = 0;
         se_sysdata    *sd = 0;
         dq_list       lst = NEW(dq_list);
         int       current = -1,
                   ioredir = !isatty(fileno(stdout));

         while (mp->size) {
            lst->add((u64t)mp->my<<48|(u64t)mp->mx<<32|(mp->mode_id&0xFFFF)<<16
               |(mp-ml));
            mp++;
         }
         lst->sort(0,1);
         // direct read of session data
         mt_swlock();
         sd = se_dataptr(se_sesno());
         if (sd) {
            cmid = sd->vs_devdata[0];
            sesx = sd->vs_x;
            sesy = sd->vs_y;
            sd   = 0;
         }
         mt_swunlock();
         // we have sorted by height/width/modeid list of modes here
         rc      = 0;
         svstate = vio_setansi(1);

         cmd_printf(
            " Available console modes:\n"
            " ÄÄÄÄÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÄÂÄÄÄÄÄÄ\n"
            "  mode size ³  font  ³  id  \n"
            " ÄÄÄÄÄÂÄÄÄÄÄÅÄÄÄÄÄÄÄÄÅÄÄÄÄÄÄ\n");
         for (ii=0; ii<lst->count(); ii++) {
            u64t           va = lst->value(ii);
            vio_mode_info *mi = ml + (u16t)va;
            char   etext[64];
            // emulated mode info
            if ((mi->flags&VMF_GRAPHMODE)==0) etext[0] = 0; else
               snprintf(etext, 64, "(emulated in %ux%ux%u)", mi->gmx,
                  mi->gmy, mi->gmbits);
            /* if session has custom mode id - we check it match, else
               check resolution only - this should be our`s mode */
            if (current<0 && (cmid && (va>>16&0xFFFF)==cmid || !cmid &&
               sesx==mi->mx && sesy==mi->my)) current = 1;
            // print with pause
            if (cmd_printseq("%s%4u |%4u ³ %3ux%-3u³ %4u  %s", 0, 0,
               current>0 ? (ioredir? "*" : ANSI_LGREEN "\x1A" ANSI_RESET) : " ", 
                  mi->mx, mi->my, mi->fontx, mi->fonty, (u32t)va>>16, etext))
                     { rc = EINTR; break; }
            if (current>0) current = 0;
         }
         DELETE(lst);
         vio_setansi(svstate);
      } else
      if (stricmp(fp,"RESET")==0) {
         vio_resetmode();
         rc = 0;
      } else
      if (stricmp(fp,"SELECT")==0) {
         qs_extcmd  ec = NEW(qs_extcmd);
         qserr     res = 0;

         if (ec) {
            u32t id = ec->selectmodedlg("Select mode", MSG_BLUE, 0);
            if (id) res = vio_setmodeid(id);
            DELETE(ec);
         } else
            res = E_SYS_UNSUPPORTED;
         if (res) cmd_shellerr(EMSG_QS, res, 0);
         rc = 0;
      } else
      if (stricmp(fp,"GMLIST")==0) {
         int verbose = 0;
         char   *out = 0;

         if (args->count==3 && stricmp(args->item[2],"/v")==0) verbose = 1;
            else
         if (args->count>2) verbose = -1;

         /* set MT lock because of direct "modes" array access.
            Listing collects data into the huge string and prints it
            only after unlock */
         mt_swlock();

         if (graph_modes==0) {
            out = sprintf_dyn("There is no graphic modes available\n");
            rc = 0;
         } else
         if (verbose==0) {
            dq_list lst = NEW(dq_list);
            // get list of modes and sort it by HEIGHT / WIDTH
            for (ii=0; ii<mode_cnt; ii++) {
               modeinfo *mi = modes[ii];
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

            out = sprintf_dyn(
               " Available graphic modes:\n"
               " ÄÄÄÄÄÄÄÄÄÄÄÄÄÂÄÄÄÄÄÂÄÄÄÄÄÂÄÄÄÄÄÂÄÄÄÄÄÂÄÄÄÄÄÂÄÄÄÄÄ\n"
               "   mode size  ³  4  ³  8  ³  15 ³  16 ³  24 ³  32 \n"
               " ÄÄÄÄÄÄÂÄÄÄÄÄÄÅÄÄÄÄÄÅÄÄÄÄÄÅÄÄÄÄÄÅÄÄÄÄÄÅÄÄÄÄÄÅÄÄÄÄÄ\n");
            for (cnt=0; cnt<lst->count(); cnt++) {
               u64t  va = lst->value(cnt);
               u8t   bv = (u8t)va;
               char *pl = sprintf_dyn(" %5u |%5u ³  %c  ³  %c  ³  %c  ³  %c  ³  %c  ³  %c\n",
                  (u32t)(va>>32&0xFFFF), (u32t)(va>>48&0xFFFF),
                     bv&0x40?'*':' ', bv&1?'*':' ', bv&2?'*':' ', bv&4?'*':' ',
                         bv&8?'*':' ',  bv&0x10?'*':' ');
               out = strcat_dyn(out, pl);
               free(pl);
            }
            DELETE(lst);
            rc = 0;
         } else
         if (verbose>0) {
            out = sprintf_dyn(
               " Available graphic modes:\n"
               " ÄÄÄÂÄÄÄÄÄÄÄÄÄÄÄÄÄÂÄÄÄÄÂÄÄÄÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄÄÄÄÂÄÄÄÄÄÄÄ\n"
               " ## ³  mode size  ³bits³    red   ³   green  ³   blue   ³  alpha   ³ pitch \n"
               " ÄÄÄÅÄÄÄÄÄÄÂÄÄÄÄÄÄÅÄÄÄÄÅÄÄÄÄÄÄÄÄÄÄÅÄÄÄÄÄÄÄÄÄÄÅÄÄÄÄÄÄÄÄÄÄÅÄÄÄÄÄÄÄÄÄÄÅÄÄÄÄÄÄÄ\n");
            for (ii=0, cnt=0; ii<mode_cnt; ii++) {
               modeinfo *mi = modes[ii];
               if ((mi->flags&CON_GRAPHMODE)!=0) {
                  char rgbuf[96], *pl;
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
                     if (!mi->amask) strcat(rgbuf, "        ");
                  } else
                     strcpy(rgbuf,"         ³          ³          ³         ");

                  // print with pause
                  pl = sprintf_dyn("%3u ³%5u |%5u ³ %2u ³ %s ³%5u\n", ++cnt,
                     mi->width, mi->height, mi->bits, rgbuf, mi->mempitch);
                  out = strcat_dyn(out, pl);
                  free(pl);
               }
            }
            rc = 0;
         }
         mt_swunlock();

         if (!rc) {
            u32t svstate = vio_setansi(1);
            // del trailing \n
            if (strclast(out)=='\n') out[strlen(out)-1] = 0;
            if (cmd_printseq(out,0,0)) rc = EINTR;
            vio_setansi(svstate);
         }
         if (out) free(out);
      } else
      if (stricmp(fp,"STATUS")==0) {
         static const u16t cps[] = {20,21,23,25,27,30,33,37,40,42,46,50,54,60,
            66,75,80,85,92,100,109,120,133,150,160,171,184,200,218,240,266,300};
         u8t  rate, delay;
         u32t   xx, yy;

         key_getspeed(&rate, &delay);
         vio_getmode(&xx, &yy);

         rate = 0x1F - (rate&0x1F);

         cmd_printf("Screen cols    : %u\n"
                    "Screen lines   : %u\n", xx, yy);
         cmd_printf("Keyboard rate  : %u.%u characters/sec\n"
                    "Keyboard delay : %u ms\n", cps[rate]/10UL,
                    cps[rate]%10UL, (u32t)(delay+1)*250);
         rc = 0;
      } else {
         u32t cols=0, lines=0, rate=FFFF, delay=0, modeid=0;
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
            if (strnicmp(args->item[ii],"ID=",3)==0) {
               modeid = strtoul(args->item[ii]+3, 0, 0);
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
            if (modeid) {
               qserr err = vio_setmodeid(modeid);
               // allow both id & cols/lines
               if (err) cmd_shellerr(EMSG_QS, err, 0); else
                  cols = lines = 0;
               rc = 0;
            }
            if (cols || lines) {
               u32t         ox, oy;
               vio_mode_info   *mi;
               vio_getmode(&ox, &oy);
               if (!cols)  cols  = ox;
               if (!lines) lines = oy;

               mi = vio_modeinfo(cols, lines, 0);
               if (!mi) cmd_printf("There is no such mode.\n"); else {
                  free(mi);
                  if (!vio_setmodeex(cols, lines))
                     cmd_printf("Unable to set the specified mode (%dx%d).\n",
                        cols, lines);
               }
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
   if (rc<0) cmd_shellerr(EMSG_CLIB,rc=EINVAL,0);
   return rc;
}

int con_fini(void) {
   mt_swlock();
   /* this should be only se_deviceswap() failure, when some sessions
      uses graphic console mode */
   if (!con_releasevio()) {
      mt_swunlock();
      return 0;
   }
   con_freefonts();
   con_removeshot();

   pl_close();
   mode_cnt = 0;
   mt_swunlock();
   return 1;
}

unsigned __cdecl LibMain( unsigned hmod, unsigned termination ) {
   if (!termination) {
      u32t  host;
      if (mod_query(selfname,MODQ_LOADED|MODQ_NOINCR)) {
         log_printf("%s already loaded!\n",selfname);
         return 0;
      }
      cvio = VHTable[VHI_CVIO];
      if (!cvio) { log_it(1, "cvio==0?\n"); return 0; }
      // even if BSS should be clean
      memset(&modes, 0, sizeof(modes));

      // check & setup host
      switch (hlp_hosttype()) {
         case QSHT_EFI : if (!plinit_efi()) return 0;
                         break;
         case QSHT_BIOS: if (!plinit_vesa()) return 0;
                         break;
         default:        log_printf("Wrong host type!\n");
                         return 0;
      }
      if (!con_init()) {
         log_printf("con_init() failed!\n");
         con_fini();
         return 0;
      }
      // install screenshot shell command
      cmd_shelladd("MKSHOT", shl_mkshot);
      // install "mode con" shell command
      cmd_modeadd("CON", con_handler);
      // exec shell commands on load
      cmd_execint(selfname,0);
      // we are ready! ;)
      log_printf(MODNAME_CONSOLE " is ready!\n");
   } else {
      if (!con_fini()) {
         log_printf("con_fini() failed!\n");
         return 0;
      }
      cmd_modermv("CON", con_handler);
      cmd_shellrmv("MKSHOT", shl_mkshot);

      log_printf(MODNAME_CONSOLE " unloaded!\n");
   }
   return 1;
}
