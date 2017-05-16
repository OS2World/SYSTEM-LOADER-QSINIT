//
// QSINIT "boot menu" module
// temporary menu implementation
//
#include "qsbase.h"
#include "qsdm.h"
#include "stdlib.h"
#include "ksline.h"
#include "vioext.h"
#include "time.h"

#define ML_Y   (1)           ///< vertical menu position
#define HELP_Y (-2)          ///< offset of help str from the bottom of screen
#define SLEEP_TIME (20)      ///< key_wait interval

static const char *palette_key = "MENUPALETTE",
                   *msg_header = "Boot menu";

static char  *kmenu_help = 0,
             *cmenu_help = 0,
             *pmenu_help = 0,
             *eline_help = 0,
               *FXaction[12];
static int      m_inited = 0,
             firstlaunch = 1;
static int      no_clock = 0;

union {
   u32t pl;
   u8t  pa[4];
} MenuPalette;

#define isFXkey(keyh)    (keyh>=0x3B && keyh<=0x44 || keyh==0x85 || keyh==0x86)
#define getFXindex(keyh) (keyh>=0x3B && keyh<=0x44? keyh-0x3B: keyh-0x85+10)
#define isFXaction(keyh) (isFXkey(keyh) && FXaction[getFXindex(keyh)])

static void DrawMenuBorder(int line, int chr) {
   u8t buf[160], *bptr = buf;
   int ii;
   for (ii=0;ii<78;ii++) { *bptr++=chr; *bptr++=MenuPalette.pa[3]; }
   vio_writebuf(1,line,78,1,buf,0);
}

static void DrawMenuHeader(int line, int clock, char *str) {
   u8t  buf[160], *bptr = buf,
        col = MenuPalette.pa[3]<<4|MenuPalette.pa[3]>>4;
   int  len = strlen(str);
   if (!len || len>50) return;

   *bptr++=' '; *bptr++=col;
   *bptr++=toupper(*str++); *bptr++=col;
   while (*str) { *bptr++=*str++; *bptr++=col; }
   *bptr++=' '; *bptr++=col;
   vio_writebuf((clock?77:(no_clock?74:69))-len,line,bptr-buf>>1,1,buf,0);
}

static void DrawMenuClock(int line) {
   char  cstr[16];
   struct tm  tmd;
   time_t     now;
   if (no_clock) return;
   time(&now); localtime_r(&now, &tmd);
   snprintf(cstr, 16, "%02u:%02u", tmd.tm_hour, tmd.tm_min);
   DrawMenuHeader(line, 1, cstr);
}

static void DrawMenuLine(int line) {
   u8t buf[160], *bptr = buf;
   int ii;
   for (ii=0;ii<78;ii++) {
      *bptr++=0xDB;
      *bptr++=MenuPalette.pa[ii==0||ii==77?3:2];
   }
   vio_writebuf(1,line,78,1,buf,0);
}

static void DrawColoredText(int line, int column, char *text, u8t color) {
   u8t buf[160], *bptr = buf;
   while (*text) {
      *bptr++=*text++; *bptr++=color;
      if (bptr-buf>=156) break;
   }
   vio_writebuf(column,line,bptr-buf>>1,1,buf,0);
}

static void DrawKernelMenuText(int line, int idx, int sel, str_list *items) {
   u8t   buf[160], *bptr = buf,
         col = sel?MenuPalette.pa[0]<<4|MenuPalette.pa[1]:
               MenuPalette.pa[2]<<4|MenuPalette.pa[0];
   char *txt = strchr(items->item[idx-1],'=');
   char   cc;
   int    ii;
   if (txt) txt++;

   *bptr++=' '; *bptr++=col;
   *bptr++=(idx<10?'0':'A'-10)+idx; *bptr++=col;
   *bptr++='.'; *bptr++=col;
   *bptr++=' '; *bptr++=col;
   cc=1;
   for (ii=0;ii<72;ii++) {
      if (cc&&txt) cc=*txt++;
      if (cc==',') cc=0;
      *bptr++=cc?cc:' '; *bptr++=col;
   }
   vio_writebuf(2,line,76,1,buf,0);
}

static void DrawCommonMenuText(int line, int idx, int sel, str_list *items, int mark) {
   u8t   buf[160], *bptr = buf,
         col = sel?MenuPalette.pa[0]<<4|MenuPalette.pa[1]:
               MenuPalette.pa[2]<<4|MenuPalette.pa[0];
   char *txt = items->item[idx-1];

   if (txt[0]=='-' && txt[1]==0) {
      memsetw((u16t*)bptr, MenuPalette.pa[2]<<12|MenuPalette.pa[3]<<8|0xCD, 76);
   } else {
     char   cc;
     int    ii;

     *bptr++=' '; *bptr++=col;
     if (mark) { *bptr++=0xF9; *bptr++=col; } else { *bptr++=' '; *bptr++=col; }
     for (ii=0;ii<74;ii++) {
        if ((cc=*txt)!=0) txt++;
        *bptr++=cc?cc:' '; *bptr++=col;
     }
   }
   vio_writebuf(2,line,76,1,buf,0);
}

static int KeyToKNumber(char key, int kerncnt) {
   if (key<'1') return 0;
   if (key>'9') {
      if (kerncnt>=10) {
         if (key>='a'&&key<='z') key-=0x20;
         if (key>='A'&&key<='A'+kerncnt-10) return key-'A'+10;
      }
      return 0;
   }
   return key<='0'+kerncnt?key-'0':0;
}

static void VioEmptyLine(void) {
   char buf[80];
   memset(buf,' ',76);
   buf[76]=0;
   vio_strout(buf);
}

void InitParameters(void) {
   u32t ii;
   if (m_inited) return;
   m_inited   = 1;
   kmenu_help = m_ini->getstr("common", "km_help", "[Esc] - shell, menu.ini is missing!");
   cmenu_help = m_ini->getstr("common", "cm_help", "");
   pmenu_help = m_ini->getstr("common", "pm_help", "");
   eline_help = m_ini->getstr("common", "nw_help", "");
   for (ii=0; ii<12; ii++) {
      char key[12];
      snprintf(key,12,"F%d",ii+1);
      FXaction[ii] = m_ini->getstr("common",key,0);
   }
   no_clock = env_istrue("menu_no_clock");
   if (no_clock<0) no_clock = 0;
}

void DoneParameters(void) {
   u32t ii;
   if (!m_inited) return;
   m_inited = 0;
   if (kmenu_help) { free(kmenu_help); kmenu_help=0; }
   if (cmenu_help) { free(cmenu_help); cmenu_help=0; }
   if (pmenu_help) { free(pmenu_help); pmenu_help=0; }
   if (eline_help) { free(eline_help); eline_help=0; }
   for (ii=0; ii<12; ii++)
      if (FXaction[ii]) { free(FXaction[ii]); FXaction[ii]=0; }
}

static void run_onmenu(void) {
   char *cmd = m_ini->getstr("common","on_menu",0);
   if (cmd) {
      cmd_state cst = cmd_init(cmd,0);
      cmd_run(cst,CMDR_ECHOOFF);
      cmd_close(cst);
      free(cmd);
   }
}

static void play(u32t hz, u32t ms) {
   vio_beep(hz, ms);
   usleep(ms*1000);
}

// show "waiting..." if no kernels/partitions to select from
static int NoMenuWait(char *rcline) {
   int ii = 0, key;
   firstlaunch=0;
   vio_charout('\n');
   vio_strout(eline_help);

   while (ii++<3) {
      u8t keyh;
      key  = key_wait(1);
      keyh = key>>8;
      if (isFXkey(keyh)) {
         int idx = getFXindex(keyh);
         if (FXaction[idx]) {
            strncpy(rcline,FXaction[idx],1024); rcline[1023] = 0;
            run_onmenu();
            return 1;
         }
      } else
      if (log_hotkey(key)) { ii--; continue; }
      vio_charout('.');
   }
   vio_charout('\n');
   return 0;
}

int IsMenuPresent(const char *section) {
   return ld_ini->secexist(section, GETSEC_NOEMPTY|GETSEC_NOEKEY);
}

int MenuKernel(char *rcline, int errors) {
   str_list* kl = ld_ini->getsec("kernel",GETSEC_NOEMPTY|GETSEC_NOEKEY|GETSEC_NOEVALUE);
   int   defcfg = ld_ini->getint("config", "DEFAULT", 1),
        timeout = ld_ini->getint("config", "TIMEOUT",-1),
        usebeep = ld_ini->getint("config", "USEBEEP", 0),
       forcekey = 0, viodebug = 0, nologo = 0, preload = 0, optmenu = 0,
         goedit = 0,
         tmwmax = timeout;
   u32t   lines = 25;
   char *initln = rcline,
      *cmdstart = 0;
   MenuPalette.pl = ld_ini->getuint("config", palette_key, 0x0F070F01);
   // fix wrong number
   if (defcfg<=0 || defcfg>kl->count) defcfg=1;

   vio_getmode(0,&lines);

   if (kl->count >= 1) {
      int  key = 0;
      int  ii;

      vio_clearscr();
      DrawMenuBorder(ML_Y,0xDC);
      DrawMenuClock(ML_Y);
      for (ii = 1; ii <= kl->count; ii++) {
         DrawMenuLine(ML_Y+ii);
         DrawKernelMenuText(ML_Y+ii, ii, ii == defcfg, kl);
      }
      DrawMenuBorder(ML_Y+1+kl->count,0xDF);
      DrawColoredText(lines+HELP_Y, 2, kmenu_help, 0x08);
      vio_setpos(ML_Y+1+kl->count, 0);
      vio_charout('\n');
      if (usebeep) { play(600,100); play(500,100); play(600,100); }
      // drop timeout on second and later menu launch
      if (firstlaunch) {
         firstlaunch=0;
         run_onmenu();
         if (!timeout) timeout=-1;
      } else timeout=-1;

      ii = 0;
      while (1) {
         u8t keyh,keyl;
         // wait timeout
         if (timeout>0) {
            key = key_wait(1);

            if (!key) {
               char buf[128];
               snprintf(buf, 128, "\rNo selection within %d seconds, boots "
                  "default (esc - stop timer) ", --timeout);
               vio_strout(buf);
               if (timeout <= 0) break;
               if (usebeep) play(5000+5000*(tmwmax-timeout)/tmwmax,60);
               DrawMenuClock(ML_Y);
            }
         }
         keyh = key>>8;
         keyl = key&0xFF;

         if (keyl==0x1B||keyh==0x0E||keyh==0x47||keyh==0x48||keyh==0x4F||
             keyh==0x50||keyh>=0x68&&keyh<=0x6F||keyh==0x12||timeout<0)
         {
            // empty line
            vio_charout(13); VioEmptyLine(); vio_charout(13);

            while (!KeyToKNumber(keyl, kl->count) && keyl!='\r' && keyl!=' '
               && key!=0x1C0A && key!=0xE00A && keyh!=0x4D && !isFXaction(keyh)) 
            {
               if (keyh>=0x68&&keyh<=0x6C||keyh==0x12) {
                  forcekey=forcekey==key?0:key;
                  vio_charout(13); VioEmptyLine(); vio_charout(13);
                  if (keyh==0x12) vio_strout("\rConfig editor "); else {
                     char buf[16];
                     snprintf(buf, 16, "\rAltF%c ",keyh-0x68+0x31);
                     vio_strout(buf);
                  }
                  vio_strout(forcekey?"on":"off");
               }
               if (keyh==0x48||keyh==0x50||keyh==0x47||keyh==0x4F) {
                  int prev=defcfg;
                  switch (keyh) {
                     case 0x47: defcfg=1; break;
                     case 0x48: defcfg--; break;
                     case 0x4F: defcfg=kl->count; break;
                     case 0x50: defcfg++; break;
                  }
                  if (!defcfg) defcfg=kl->count; else
                  if (defcfg>kl->count) defcfg=1;
                  DrawKernelMenuText(ML_Y+prev, prev, 0, kl);
                  DrawKernelMenuText(ML_Y+defcfg, defcfg, 1, kl);

                  if (usebeep)
                     if (defcfg!=1) play(5000,100); else {
                        play(400,50); play(800,50);
                     }
               } else
               if (keyh==0x6D && !viodebug) {
                  viodebug = 1;
                  vio_charout(13); VioEmptyLine(); vio_charout(13);
                  vio_strout("\rVIO debug on\n");
               } else
               if (keyh==0x6E && !nologo) {
                  nologo = 1;
                  vio_charout(13); VioEmptyLine(); vio_charout(13);
                  vio_strout("\rLogo off\n");
               } else
               if (keyh==0x6F && !preload) {
                  preload = 1;
                  vio_charout(13); VioEmptyLine(); vio_charout(13);
                  vio_strout("\rBoot files preload on\n");
               }
               // wait next key
               do {
                  key  = no_clock ? key_read() : key_wait(SLEEP_TIME);
                  DrawMenuClock(ML_Y);
               } while (!key);
               keyh = key>>8;
               keyl = key&0xFF;
               log_printf("key=%04X\n",key);

               // escape pressed second time
               if (keyl==0x1B||keyh==0x0E) {
                  vio_clearscr();
                  return 0;
               }
            }
         }

         if (keyl && KeyToKNumber(keyl, kl->count)) {
            defcfg = KeyToKNumber(keyl, kl->count);
            break;
         } else
         if ((keyl=='\r'||keyl==' '||keyh==0x4D||key==0x1C0A||key==0xE00A) && defcfg) {
            optmenu = keyh==0x4D;
            goedit  = keyl==0x0A;
            break;
         } else
         if (isFXkey(keyh)) {
            int idx = getFXindex(keyh);
            if (FXaction[idx]) {
               strncpy(rcline,FXaction[idx],1024); rcline[1023] = 0;
               free(kl);
               return 1;
            }
         }
      }
   } else {
      if (NoMenuWait(rcline)) {
         free(kl);
         return 1;
      }
      // no kernel and no menu!
      if (errors) optmenu = 1;
   }

   if (goedit) strcpy(rcline,"cmd /e "); else rcline[0] = 0;
   cmdstart = rcline+strlen(rcline);
   strcpy(cmdstart, optmenu?"sysview.exe /boot ":"bootos2.exe ");
   rcline+=strlen(rcline);

   if (defcfg<=0) strcpy(rcline,"OS2KRNL "); else
   {
      char *args = 0, *modname = rcline, *argpos;
      // save last pos
      if (optmenu) ld_ini->setint("config", "DEFAULT", defcfg);
      // kernel file name
      if (!kl->count||defcfg>kl->count) {
         strcpy(rcline,"OS2KRNL ");
      } else {
         args = strchr(kl->item[defcfg-1],'=');
         *args++=0;
         strcpy(rcline,kl->item[defcfg-1]);
         strcat(rcline," ");
      }
      rcline+=strlen(rcline);
      argpos =rcline;
      // options from menu
      if (viodebug) strcat(rcline,"DBPORT=0,");
      if (preload) strcat(rcline,"PRELOAD,");
      if (nologo) strcat(rcline,"NOLOGO,");
      if (forcekey) sprintf(rcline+strlen(rcline),"PKEY=0x%04X,",forcekey);
      if (forcekey==0x1200||viodebug) strcat(rcline,"NODBCS,");
      // merge with config
      if (cmd_mergeopts(rcline,args,ld_fname)<0) {
         // restart specified?
         *argpos--=0;
         while (*argpos==' ') *argpos--=0;
         if (optmenu) {
            // change to "sysview.exe /rest "
            memcpy(modname-5,"rest",4);
         } else 
         if (goedit) {
            strcpy(cmdstart, "restart \"");
            strcat(cmdstart, modname);
            strcat(cmdstart, "\"");
         } else {
            char buf[256];
            log_printf("[%s]\n",modname);
            exit_restart(modname);
            sprintf(buf,"echo\necho Unable to find OS2LDR file \"%s\"!\necho\npause",modname);
            strcpy(initln,buf);
         }
      }
   }

   free(kl);
   return 1;
}

#define is_menu_item(idx) (values->item[(idx)-1][0]=='@')

int MenuCommon(char *menu, char *rcline, u32t pos) {
   str_list *keys, *values;
   u32t ii, selected = pos?pos:1, rc = 1, lines=25;
   if (!menu) return 0;
   keys = m_ini->keylist(menu, &values);
   if (!keys) return 0;
   if (!keys->count) { free(keys); free(values); return 0; }

   vio_getmode(0,&lines);

   MenuPalette.pl = m_ini->getuint("common", palette_key, 0x0F070F01);
   if (selected>keys->count) selected = 1;

   vio_clearscr();
   DrawMenuBorder(ML_Y, 0xDC);
   DrawMenuHeader(ML_Y, 0, menu);
   DrawMenuClock (ML_Y);
   for (ii = 1; ii <= keys->count; ii++) {
      DrawMenuLine(ML_Y+ii);
      DrawCommonMenuText(ML_Y+ii, ii, ii == selected, keys, is_menu_item(ii));
   }
   DrawMenuBorder(ML_Y+1+keys->count,0xDF);
   DrawColoredText(lines+HELP_Y, 2, cmenu_help, 0x08);
   vio_setpos(ML_Y+1+keys->count, 0);
   vio_charout('\n');
   if (firstlaunch) { firstlaunch=0; run_onmenu(); }

   ii = 0;
   while (1) {
      u16t key  = no_clock ? key_read() : key_wait(SLEEP_TIME);
      u8t  keyh = key>>8,
           keyl = key&0xFF;

      DrawMenuClock(ML_Y);
      if (!key) continue;

      if (keyh==0x48||keyh==0x50||keyh==0x47||keyh==0x4F) {
         int prev=selected, incv=0;
         switch (keyh) {
            case 0x47: selected=1; break;
            case 0x48: selected--; incv=-1;  break;
            case 0x4F: selected=keys->count; break;
            case 0x50: selected++; incv= 1;  break;
         }
         // skip split line in menu if this is possible
         if (incv && selected>0 && selected<=keys->count)
            if (strcmp(keys->item[selected-1],"-")==0) selected+=incv;
         // check and update menu lines
         if (!selected) selected=keys->count; else
         if (selected>keys->count) selected=1;
         DrawCommonMenuText(ML_Y+prev, prev, 0, keys, is_menu_item(prev));
         DrawCommonMenuText(ML_Y+selected, selected, 1, keys, is_menu_item(selected));
      } else
      // escape/backspace pressed second time
      if (keyl==0x1B||keyh==0x0E) {
         vio_clearscr();
         rc = 0;
         break;
      } else
      if ((keyl=='\r' || keyl==' ' || key==0x1C0A || key==0xE00A) && selected) {
         char *src = values->item[selected-1];
         int em_on = keyl==0x0A;
         // ctrl-enter was pressed?
         if (em_on && *src=='@') em_on = 0; else
         if (em_on && *src=='~') src++;

         if (em_on) strcpy(rcline,"cmd /e "); else *rcline = 0;
         if (strlen(src)>1000) src[1000] = 0;
         strcat(rcline,src);
         break;
      } else
      if (isFXkey(keyh)) {
         int idx = getFXindex(keyh);
         if (FXaction[idx]) {
            strncpy(rcline,FXaction[idx],1024);
            rcline[1023] = 0;
            break;
         }
      }
   }
   if (rc) rc = selected;
   free(values);
   free(keys);
   return rc;
}

int MenuPtBoot(char *rcline) {
   str_list* kl = ld_ini->getsec("partition", GETSEC_NOEMPTY|GETSEC_NOEKEY|GETSEC_NOEVALUE);
   int   defcfg = ld_ini->getint("config", "default_partition", 1),
        timeout = ld_ini->getint("config", "TIMEOUT",-1),
        usebeep = ld_ini->getint("config", "USEBEEP", 0),
         goedit = 0,
         tmwmax = timeout,
             rc = 1,
           done = 0;
   u32t   lines = 25;
   char *initln = rcline;
   MenuPalette.pl = m_ini->getuint("common", "ptpalette", 0x0F070F01);

   vio_getmode(0,&lines);
   // fix wrong number
   if (defcfg<=0 || defcfg>kl->count) defcfg=1;

   do {
      if (kl->count >= 1) {
         int  key = 0;
         int  ii;
   
         vio_clearscr();
         DrawMenuBorder(ML_Y,0xDC);
         DrawMenuClock(ML_Y);
         for (ii = 1; ii <= kl->count; ii++) {
            DrawMenuLine(ML_Y+ii);
            DrawKernelMenuText(ML_Y+ii, ii, ii == defcfg, kl);
         }
         DrawMenuBorder(ML_Y+1+kl->count,0xDF);
         DrawColoredText(lines+HELP_Y, 2, pmenu_help, 0x08);
         vio_setpos(ML_Y+1+kl->count, 0);
         vio_charout('\n');
         if (usebeep) { play(600,100); play(500,100); play(600,100); }
         // drop timeout on second and later menu launch
         if (firstlaunch) {
            firstlaunch=0;
            run_onmenu();
            if (!timeout) timeout=-1;
         } else timeout=-1;
   
         ii = 0;
         while (1) {
            u8t  keyh, keyl;
            // wait timeout
            if (timeout>0) {
               key = key_wait(1);

               if (!key) {
                  char buf[128];
                  snprintf(buf, 128, "\rNo selection within %d seconds, boots "
                     "default (esc - stop timer) ", --timeout);
                  vio_strout(buf);
                  if (timeout <= 0) break;
                  if (usebeep) play(5000+5000*(tmwmax-timeout)/tmwmax,60);
                  DrawMenuClock(ML_Y);
               }
            }
            keyh = key>>8;
            keyl = key&0xFF;
   
            if (keyl==0x1B||keyh==0x0E||keyh==0x47||keyh==0x48||keyh==0x4F||
                keyh==0x50||timeout<0)
            {
               // empty line
               vio_charout(13); VioEmptyLine(); vio_charout(13);
   
               while (!KeyToKNumber(keyl, kl->count) && keyl!='\r'&& keyl!=' '
                  && key!=0x1C0A && key!=0xE00A && !isFXaction(keyh)) {
   
                  if (keyh==0x48||keyh==0x50||keyh==0x47||keyh==0x4F) {
                     int prev=defcfg;
                     switch (keyh) {
                        case 0x47: defcfg=1; break;
                        case 0x48: defcfg--; break;
                        case 0x4F: defcfg=kl->count; break;
                        case 0x50: defcfg++; break;
                     }
                     if (!defcfg) defcfg=kl->count; else
                     if (defcfg>kl->count) defcfg=1;

                     DrawKernelMenuText(ML_Y+prev, prev, 0, kl);
                     DrawKernelMenuText(ML_Y+defcfg, defcfg, 1, kl);
   
                     if (usebeep)
                        if (defcfg!=1) play(5000,100); else {
                           play(400,50); play(800,50);
                        }
                  }

                  do {
                     key  = no_clock ? key_read() : key_wait(SLEEP_TIME);
                     DrawMenuClock(ML_Y);
                  } while (!key);
                  keyh = key>>8;
                  keyl = key&0xFF;
                  log_printf("key=%04X\n",key);
   
                  // escape/backspace pressed second time
                  if (keyl==0x1B||keyh==0x0E) {
                     vio_clearscr();
                     return 0;
                  }
               }
            }
   
            if (keyl && KeyToKNumber(keyl, kl->count)) {
               defcfg = KeyToKNumber(keyl, kl->count);
               break;
            } else
            if ((keyl=='\r'||keyl==' '||key==0x1C0A||key==0xE00A) && defcfg) {
               goedit = keyl==0x0A;
               break;
            } else
            if (isFXkey(keyh)) {
               int idx = getFXindex(keyh);
               if (FXaction[idx]) {
                  strncpy(rcline,FXaction[idx],1024); rcline[1023] = 0;
                  free(kl);
                  return 1;
               }
            }
         }
      } else {
         vio_msgbox(msg_header, "There is no partition menu defined!",
            MSG_OK|MSG_GRAY, 0);
         rc = 0;
      }
      // dmgr mbr hd0 boot 1
      if (defcfg>0 && defcfg<=kl->count) {
         str_list *dn;
         char errtext[256], *pti, *errpt,
              *dskt = strdup(kl->item[defcfg-1]),
                *ep = strchr(dskt,'='),
              *keys = ep ? strchr(ep+1, ',') : 0,
             *rcout = rcline;
         int  index = -1;
         u32t  disk;
         errtext[0] = 0;

         if (goedit) strcpy(rcout,"cmd /e "); else rcout[0] = 0;

         /* check for CPUCLOCK=x and NOMTRR keys and put it as common shell
            commands to the execution line */
         if (keys) {
            str_list *largs = str_split(++keys, ",");
            if (largs->count>0) {
               char *cmstr = str_findkey(largs, "CPUCLOCK", 0);
               if (cmstr) {
                  strcat(rcout,"mode sys cm=");
                  strcat(rcout,cmstr);
                  strcat(rcout," noreset & ");
               }
               if (str_findkey(largs, "NOMTRR", 0)) strcat(rcout,"mtrr reset & ");
            }

            free(largs);
         }
   
         strcat(rcout,"dmgr mbr ");
         rcout+=strlen(rcout);
   
         *ep = 0;
         // function trim spaces for us as a bonus
         dn = str_split(dskt,",");

         if (dn->count>2 || dn->count==2 && strlen(dn->item[1])>_MAX_PATH)
            snprintf(errtext, 256, "Invalid disk string \"%s\"!", dskt);
         free(dskt);

         pti = strrchr(dn->item[0],'/');
         if (pti) {
            index = strtol(++pti, &errpt, 10);
            if (*errpt) snprintf(errtext, 256, "Invalid partition index "
               "in string \"%s\"!", pti);
            pti[-1] = 0;
         }
         disk = dsk_strtodisk(dn->item[0]);
         if (disk==FFFF) {
            snprintf(errtext, 256, "Invalid disk name: \"%s\"!", dn->item[0]);
         } else 
         if (disk&QDSK_VOLUME) {
            snprintf(errtext, 256, "Invalid disk type: \"%s\"!", dn->item[0]);
         }
   
         if (errtext[0]) {
            vio_msgbox(msg_header, errtext, MSG_OK|MSG_RED, 0);
            free(dn);
            continue;
         } else {
            strcpy(rcout, dn->item[0]);
            strcat(rcout, dn->count==2?" bootfile ":" boot ");
            rcout+=strlen(rcout);
            if (index>=0) {
               itoa(index, rcout, 10);
               // append file name
               if (dn->count==2) {
                  strcat(rcout, " ");
                  strcat(rcout, dn->item[1]);
               }
            }
            free(dn);
         }
      }
      done = 1;
   } while (!done);
   free(kl);
   return rc;
}
