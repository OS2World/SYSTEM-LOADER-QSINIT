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

#define HELP_Y (-2)          ///< offset of help str from the bottom of screen
#define SLEEP_TIME (20)      ///< key_wait interval

static const char *palette_key = "MENUPALETTE",
                  *noclock_key = "menu_no_clock",
                   *msg_header = "Boot menu";

static char  *kmenu_help = 0,
             *cmenu_help = 0,
             *pmenu_help = 0,
             *eline_help = 0,
               *FXaction[12],
               *wait_msg = 0,
              *wait_msg1 = 0,
               *stop_msg = 0;
static int      m_inited = 0,
             firstlaunch = 1;
static int      no_clock = 0,
                no_title = 0;
static int       m_width = 78,
                m_height = 0,
                m_center = 0,
                  m_view = 0;
static u32t in_menu_wait = 0,
                  mpos_x = 1,
                  mpos_y = 1;
static qsclock inm_clock = 0;

union {
   u32t pl;
   u8t  pa[4];
} MenuPalette;

static char  menu_fp[16],
          menu_title[48],
           menu_time[16];

#define isFXkey(keyh)    (keyh>=0x3B && keyh<=0x44 || keyh==0x85 || keyh==0x86)
#define getFXindex(keyh) (keyh>=0x3B && keyh<=0x44? keyh-0x3B: keyh-0x85+10)
#define isFXaction(keyh) (isFXkey(keyh) && FXaction[getFXindex(keyh)])

static void DrawMenuLine(int line) {
   u8t buf[160], *bptr = buf;
   int   bl_pos = m_view==1?1:0, ii;

   for (ii=0; ii<m_width; ii++) {
      int border = ii==bl_pos || ii==m_width-1-bl_pos;
      if (m_view==1) {
         *bptr++ = border ? 0xB3 : ' ';
         *bptr++ = MenuPalette.pa[3];
      } else {
         *bptr++ = 0xDB;
         *bptr++ = MenuPalette.pa[border?3:2];
      }
   }
   if (m_view==1) line++;
   vio_writebuf(mpos_x, mpos_y+line, m_width, 1, buf, 0);
}

static void DrawMenuBorder(int bottom) {
   u8t buf[160], *bptr = buf;
   int ii;
   
   for (ii=0; ii<m_width; ii++) {
      u8t col = MenuPalette.pa[3];
      if (m_view==1) {
         if (ii==0 || ii==m_width-1) {
            col = MenuPalette.pa[3];
            *bptr++ = ' ';
         } else
         if (ii==1) *bptr++ = bottom?0xC0:0xDA; else
         if (ii==m_width-2) *bptr++ = bottom?0xD9:0xBF; else
            *bptr++ = 0xC4;
      } else
         *bptr++=bottom?0xDF:0xDC;
      *bptr++=col;
   }
   vio_writebuf(mpos_x, mpos_y+(bottom?m_height-1:0), m_width, 1, buf, 0);
   // empty line in alt. view mode
   if (m_view==1) DrawMenuLine(bottom?m_height-3:0);
}

static void DrawMenuTopText(int pos, char *str) {
   u8t  buf[160], *bp = buf,
        col = MenuPalette.pa[3]<<4|MenuPalette.pa[3]>>4;
   int  len = strlen(str);
   if (!len || len>m_width-10) return;
   if (pos<0) pos = -pos-len-2;

   *bp++=' '; *bp++=col;
   *bp++=toupper(*str++); *bp++=col;
   while (--len) { *bp++=*str?*str++:' '; *bp++=col; }
   *bp++=' '; *bp++=col;
   vio_writebuf(mpos_x+pos, mpos_y, bp-buf>>1, 1, buf, 0);
}

static void DrawMenuTop(int full) {
   if (no_clock>0) {
      if (!full) return;
      DrawMenuBorder(0);
      if (!no_title && menu_title[0])
         DrawMenuTopText(-mpos_x-m_width+3, menu_title);
   } else {
      char  cstr[16];
      struct tm  tmd;
      time_t     now;
      time(&now); localtime_r(&now, &tmd);
      strftime(cstr, sizeof(cstr), "%R", &tmd);
      
      if (!full) full = strcmp(cstr, menu_time);
      if (full) {
         int pmove = m_view==1?2:0;
         strcpy(menu_time, cstr);
         // Fri,13  May,30 (qsinit`s birthday ;) Dec,31 & Jan, 1
         if (no_clock<0 || tmd.tm_mday==13 && tmd.tm_wday==5 || tmd.tm_mday==30
            && tmd.tm_mon==4 || tmd.tm_mday==31 && tmd.tm_mon==11 ||
               tmd.tm_mday==1 && tmd.tm_mon==0)
                  strftime(menu_fp, sizeof(menu_time), "%a,%e", &tmd);
                     else menu_fp[0] = 0;
         DrawMenuBorder(0);
         DrawMenuTopText(-m_width+pmove, menu_time);
         if (menu_fp[0]) DrawMenuTopText(pmove, menu_fp);
         if (!no_title && menu_title[0])
            DrawMenuTopText(-m_width+8+pmove, menu_title);
      }
   }
}

// help string draw
static void DrawColoredText(int line, int column, char *text, u8t color) {
   u8t buf[160], *bptr = buf;
   while (*text) {
      *bptr++=*text++; *bptr++=color;
      if (bptr-buf>=156) break;
   }
   vio_writebuf(column,line,bptr-buf>>1,1,buf,0);
}

// line = 1..n
static void DrawKernelMenuText(int line, int idx, int sel, str_list *items) {
   u8t   buf[160],
         col = sel && m_view!=1?MenuPalette.pa[0]<<4|MenuPalette.pa[1]:
               MenuPalette.pa[2]<<4|MenuPalette.pa[0];
   char *txt = strchr(items->item[idx-1],'=');
   int    bw = m_view==1?2:1, ii,
        txtw = m_width - bw*2,
        spcw = m_view==1?4:2;
   if (txt) txt++;

   memsetw((u16t*)&buf, (u16t)col<<8|' ', txtw);

   if (m_view==1 && sel) {
      buf[2*2] = 0x10;
      buf[(txtw-2)*2] = 0x11;
   }
   if (m_view==0) {
      buf[1*2] = (idx<10?'0':'A'-10)+idx;
      buf[2*2] = '.';
   }
   for (ii=0; ii<txtw-4-spcw; ii++) {
      char cc = *txt++;
      if (!cc || cc==',') break;
      buf[4+ii<<1] = cc;
   }
   if (m_view==1) line++;
   vio_writebuf(mpos_x+bw, mpos_y+line, txtw, 1, buf, 0);
}

static void DrawCommonMenuLine(int line, char *txt, int sel, int mark) {
   u8t   buf[160],
         col = sel && m_view==0?MenuPalette.pa[0]<<4|MenuPalette.pa[1]:(
               sel && m_view==1?MenuPalette.pa[1]:(
               MenuPalette.pa[2]<<4|MenuPalette.pa[0]));
   int    bw = m_view==1?2:1,
        txtw = m_width - bw*2;

   if (txt[0]=='-' && txt[1]==0) {
      u16t lcol = MenuPalette.pa[2]<<12|MenuPalette.pa[3]<<8;
      if (m_view==1) {
         txtw+=2;
         bw  -=1;
         memsetw((u16t*)&buf, lcol|0xC4, txtw);
         buf[0] = 0xC3;
         buf[txtw-1<<1] = 0xB4;
      } else
         memsetw((u16t*)&buf, lcol|0xCD, txtw);
   } else {
      int spcw = m_view==1?4:2, ii;
      memsetw((u16t*)&buf, (u16t)col<<8|' ', txtw);

      if (m_view==1 && sel) {
         buf[2*2] = 0x10;
         buf[(txtw-2)*2] = 0x11;
      }
      if (m_view==0 && mark) buf[1*2] = 0xF9;

      for (ii=0; ii<txtw-spcw*2; ii++) {
         char cc = *txt++;
         if (!cc) break;
         buf[spcw+ii<<1] = cc;
      }
   }
   if (m_view==1) line++;
   vio_writebuf(mpos_x+bw, mpos_y+line, txtw, 1, buf, 0);
}

static void DrawCommonMenuText(int line, int idx, int sel, str_list *items, int mark) {
   DrawCommonMenuLine(line, items->item[idx-1], sel, mark);
}

static void DrawInMenuTimeout(int timeout, int init) {
   char     *str = 0;
   str_list *lst;
   if (timeout>0) {
      if (timeout==1) str = wait_msg1;
      if (!str) str = wait_msg;
      str = sprintf_dyn(str, timeout);
   } else
      str = strdup(stop_msg);
   lst = cmd_splittext(str, m_width - (m_view==1?6:3)*2, SplitText_NoAnsi);

   if (init) {
      int ii;
      for (ii=0; ii<3; ii++) DrawMenuLine(m_height-4-ii);
   }
   DrawCommonMenuLine(m_height-6, "", 0, 0);
   DrawCommonMenuLine(m_height-5, lst->item[0], 0, 0);
   DrawCommonMenuLine(m_height-4, lst->count>1?lst->item[1]:"", 0, 0);

   free(lst);
   free(str);
   inm_clock = sys_clock() + CLOCKS_PER_SEC - CLOCKS_PER_SEC/20;
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
   kmenu_help = m_ini->getstr("common", "km_help", "");
   cmenu_help = m_ini->getstr("common", "cm_help", "");
   pmenu_help = m_ini->getstr("common", "pm_help", "");
   eline_help = m_ini->getstr("common", "nw_help", "");
   for (ii=0; ii<12; ii++) {
      char key[12];
      snprintf(key,12,"F%d",ii+1);
      FXaction[ii] = m_ini->getstr("common",key,0);
   }
   // env_istrue() return -1 if no string, so check it first
   if (m_ini->getuint("common", "noclock", 0)) no_clock = 1; else
      if (getenv(noclock_key)) no_clock = env_istrue("menu_no_clock");

   no_title = m_ini->getuint("common", "notitle", 0);
   m_width  = m_ini->getuint("common", "width", 78);
   m_center = m_ini->getuint("common", "center", 0);
   m_view   = m_ini->getuint("common", "view", 0);
   if (m_view>1) m_view = 0;
   if (m_width>78) m_width = 78;
   if (m_width<20) m_width = 20;

   in_menu_wait = m_ini->getuint("common", "in_menu_wait", 0);
   if (in_menu_wait) {
      if (in_menu_wait<3) in_menu_wait = 3;
      wait_msg1 = m_ini->getstr("common", "wait_msg_1s", 0);
      wait_msg  = m_ini->getstr("common", "wait_msg" , "");
      stop_msg  = m_ini->getstr("common", "stop_msg" , "");
   }
}

void DoneParameters(void) {
   u32t ii;
   if (!m_inited) return;
   m_inited = 0;
   if (kmenu_help) { free(kmenu_help); kmenu_help=0; }
   if (cmenu_help) { free(cmenu_help); cmenu_help=0; }
   if (pmenu_help) { free(pmenu_help); pmenu_help=0; }
   if (eline_help) { free(eline_help); eline_help=0; }
   if (wait_msg1) { free(wait_msg1); wait_msg1=0; }
   if (wait_msg) { free(wait_msg); wait_msg=0; }
   if (stop_msg) { free(stop_msg); stop_msg=0; }

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

static void update_menu_pos(int mtype, u32t lines) {
   u32t mx, my;
   vio_getmode(&mx,&my);
   lines += m_view==1?4:2;
   if (in_menu_wait && firstlaunch && mtype==1) lines+=3;

   if (m_width>mx) m_width = mx-2;
   if (lines>my) lines = my-2;
   m_height = lines;

   if (!m_center) {
      mpos_x = 1;
      mpos_y = 1;
   } else {
      mpos_x = mx-m_width>>1;
      mpos_y = my-m_height>>1;
   }
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
      }
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
   update_menu_pos(0, kl->count);

   if (kl->count >= 1) {
      int  key = 0;
      int  ii;

      vio_clearscr();
      // reset title
      menu_title[0] = 0;
      // and draw menu with or without clock
      DrawMenuTop(1);
      for (ii = 1; ii <= kl->count; ii++) {
         DrawMenuLine(ii);
         DrawKernelMenuText(ii, ii, ii == defcfg, kl);
      }
      DrawMenuBorder(1);
      DrawColoredText(lines+HELP_Y, 2, kmenu_help, 0x08);
      vio_setpos(mpos_y+m_height, 0);
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
               DrawMenuTop(0);
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
                  DrawKernelMenuText(prev, prev, 0, kl);
                  DrawKernelMenuText(defcfg, defcfg, 1, kl);

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
                  key  = no_clock>0 ? key_read() : key_wait(SLEEP_TIME);
                  DrawMenuTop(0);
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
            exit_restart(modname, 0, 0);
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
   str_list   *keys, *values;
   u32t    selected = pos?pos:1, rc = 1, lines=25, ii;
   int      timeout = -1;

   if (!menu) return 0;
   keys = m_ini->keylist(menu, &values);
   if (!keys) return 0;

   /* process conditions in form:
      @@[!]b/e|Line = ...   - bios/efi host only
      @@[!][i]%var%value|Line = ...   - check env. var value
      conditions can be ANDed in form
      @@c1@@c2@@c3|Line = ....
      ! mean NOT operator
      i mean ignore case
      no spaces allowed until |, except in "value" */
   for (ii=0; ii<keys->count; ) {
      char *key = keys->item[ii];
      if (key[0]=='@' && key[1]=='@') {
         char *ep = strchr(key, '|');
         int  del = 0;
         if (ep) {
            *ep++ = 0;
            while (key[0]=='@' && key[1]=='@' && del==0) {
               int _std (*cmp)(const char*, const char*) = strcmp;
               int inv = 0;
               key  += 2;
               while (*key=='!' || *key=='i' || *key=='I') {
                  if (*key=='!') inv=1; else cmp=stricmp;
                  key++;
               }
               if (*key=='%') {
                  char *ve = strchr(++key, '%');
                  if (!ve || (u32t)key>(u32t)ep || (u32t)ve>(u32t)ep) del = -1; else {
                     char *estr;
                     *ve++ = 0;
                     estr  = env_getvar(key, 1);
                     key   = strstr(ve,"@@");
                     if (key) *key = 0;
                     del   = cmp(estr?estr:"", ve)?1:0;
                     if (estr) free(estr);
                     // point it to next @@ or to 0
                     if (key) *key = '@'; else key = ve-1;
                  }
               } else
               if (*key=='e' || *key=='E') { del = hlp_hosttype()!=QSHT_EFI; key++; }
                  else
               if (*key=='b' || *key=='B') { del = hlp_hosttype()!=QSHT_BIOS; key++; }
                  else del = -1;
               // all cases except wrong condition
               if (del>=0 && inv) del = !del;
            }
            while (*ep==' ') ep++;
            keys->item[ii] = ep;
         }
         if (del) {
            if (del<0) log_printf("bad cond. in \"%s\" in [%s]\n", ep, menu);
            str_delentry(keys, ii);
            str_delentry(values, ii);
            continue;
         }
      }
      ii++;
   }

   if (!keys->count) { free(keys); free(values); return 0; }

   vio_getmode(0,&lines);
   update_menu_pos(1, keys->count);

   MenuPalette.pl = m_ini->getuint("common", palette_key,
                                   m_view==1?0xF80F0708:0x0F070F01);
   if (selected>keys->count) selected = 1;

   strncpy(menu_title, menu, sizeof(menu_title));
   menu_title[sizeof(menu_title)-1] = 0;
   // try to fix position to horz. line
   while (strcmp(keys->item[selected-1],"-")==0 && selected<keys->count) selected++;

   vio_clearscr();
   DrawMenuTop(1);
   for (ii = 1; ii <= keys->count; ii++) {
      DrawMenuLine(ii);
      DrawCommonMenuText(ii, ii, ii == selected, keys, is_menu_item(ii));
   }
   DrawMenuBorder(1);
   DrawColoredText(lines+HELP_Y, 2, cmenu_help, 0x08);
   vio_setpos(mpos_y+m_height, 0);
   vio_charout('\n');
   if (firstlaunch) { 
      firstlaunch=0;
      run_onmenu();
      if (in_menu_wait) DrawInMenuTimeout(timeout = in_menu_wait, 1);
   }
   vio_setshape(VIO_SHAPE_NONE);

   ii = 0;
   while (1) {
      u16t key  = no_clock>0 && timeout<0 ? key_read() :
                  key_wait(timeout>=0?1:SLEEP_TIME);
      u8t  keyh = key>>8,
           keyl = key&0xFF;

      DrawMenuTop(0);
      if (timeout>0 && sys_clock()>inm_clock)
         if (--timeout==0) { selected=1; break; }
            else DrawInMenuTimeout(timeout, 0);
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
         DrawCommonMenuText(prev, prev, 0, keys, is_menu_item(prev));
         DrawCommonMenuText(selected, selected, 1, keys, is_menu_item(selected));
      } else
      // escape/backspace pressed second time
      if (keyl==0x1B||keyh==0x0E) {
         // first esc stops timer
         if (keyl==0x1B && timeout>0) {
            DrawInMenuTimeout(timeout = 0, 0);
            continue;
         }
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
   vio_setshape(VIO_SHAPE_LINE);
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
   update_menu_pos(0, kl->count);
   // fix wrong number
   if (defcfg<=0 || defcfg>kl->count) defcfg=1;

   do {
      if (kl->count >= 1) {
         int  key = 0;
         int  ii;
   
         vio_clearscr();
         menu_title[0] = 0;
         DrawMenuTop(1);
         for (ii = 1; ii <= kl->count; ii++) {
            DrawMenuLine(ii);
            DrawKernelMenuText(ii, ii, ii == defcfg, kl);
         }
         DrawMenuBorder(1);
         DrawColoredText(lines+HELP_Y, 2, pmenu_help, 0x08);
         vio_setpos(mpos_y+m_height, 0);
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
                  DrawMenuTop(0);
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

                     DrawKernelMenuText(prev, prev, 0, kl);
                     DrawKernelMenuText(defcfg, defcfg, 1, kl);
   
                     if (usebeep)
                        if (defcfg!=1) play(5000,100); else {
                           play(400,50); play(800,50);
                        }
                  }

                  do {
                     key  = no_clock>0 ? key_read() : key_wait(SLEEP_TIME);
                     DrawMenuTop(0);
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
