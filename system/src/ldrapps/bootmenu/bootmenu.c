//
// QSINIT "boot menu" module
//
#include "stdlib.h"
#include "signal.h"
#include "qsbase.h"
#include "qcl/qslist.h"
#include "ksline.h"
#include "qsmodext.h"

int  MenuKernel(char *rcline, int errors);
int  MenuCommon(char *menu, char *rcline, u32t pos);
int  MenuPtBoot(char *rcline);
void InitParameters(void);
void DoneParameters(void);
int  IsMenuPresent(const char *section);

static void _std push_updn(sys_eventinfo *info) {
   // push "up + down" - to stop the timer without any side effects
   key_push(0x4800);
   key_push(0x5000);
}

char       *menu_fname = 0;
const char   *ld_fname = "b:\\qsinit.ini";
qs_inifile       m_ini = 0,    // menu ini file
                ld_ini = 0;    // ldr ini file

void main(int argc,char *argv[]) {
   char cmdline[1024], cmenu[128], *eptr, *defmenu = 0;
   ptr_list  history;
   int        rc = 1,
        kernfail = 0,
       noextmenu = 0;

   if (argc>1 && strcmp(argv[1],"/?")==0) {
      printf("usage: bootmenu [menu_name]\n");
      return;
   }
   // disable Ctrl-C
   signal(SIGINT, SIG_IGN);

   history = NEW(ptr_list);
   m_ini   = NEW(qs_inifile);
   ld_ini  = NEW(qs_inifile);

   if (argc>1) strcpy(cmenu,argv[1]); else {
      eptr = getenv("start_menu");
      if (eptr) { strncpy(cmenu,eptr,128); cmenu[127] = 0; } else 
         { cmenu[0] = 0; noextmenu = 1; }
   }
   eptr       = getenv("menu_source");
   menu_fname = strdup(eptr?eptr:"b:\\menu.ini");
   // open files (ignore presence, actually)
   m_ini->open(menu_fname, QSINI_READONLY);
   ld_ini->open(ld_fname, QSINI_READONLY);
   // reading color & help strings from menu.ini
   InitParameters();
   // default menu for uefi & single mode
   if (hlp_hosttype()==QSHT_EFI)
      defmenu = m_ini->getstr("common", "def_efi", 0); else
   if (hlp_boottype()==QSBT_SINGLE)
      defmenu = m_ini->getstr("common", "def_single", 0); 
   /* there is no altered menu name - check for existence of [kernel] and
      [partition] and switching to the partition menu when suitable */
   if (noextmenu)
      if (!IsMenuPresent("kernel") && IsMenuPresent("partition"))
         strcpy(cmenu,"bootpart");
   // still has no menu name - then get defaults for "single" or EFI
   if (!*cmenu && defmenu) strncpy(cmenu, defmenu, 128);
   if (defmenu) { free(defmenu); defmenu = 0; }

   while (rc) {
      char *idxpos = strchr(cmenu,'#');
      u32t     idx = 0;
      int iskernel = !*cmenu || stricmp(cmenu,"bootos2")==0,
          isptboot = stricmp(cmenu,"bootpart")==0 || stricmp(cmenu,"pt")==0;
      // reset title every time before menu processing
      se_settitle(se_sesno(), "Boot menu");

      if (idxpos) { *idxpos++=0; idx=atoi(idxpos); }

      /* a bit crazy hack for handling MT activation by a hotkey.
         When user presses Ctrl-Esc - he goes into a task list, but timer
         here continue to count! This cause background kernel launch.
         So we just catch MT mode activation ONLY during menu navigation
         and post up+down key presses in this case (to stop timer cycle) */
      sys_notifyevent(SECB_MTMODE|SECB_THREAD, push_updn);
      // launch menu
      rc = iskernel ? MenuKernel(cmdline,kernfail) : (
           isptboot ? MenuPtBoot(cmdline) : MenuCommon(cmenu,cmdline,idx));
      // unset notification
      sys_notifyevent(0, push_updn);

      if (rc) {
         log_it(2,"action=\"%s\"\n",cmdline);
         // save position
         if (rc>1) snprintf(cmenu+strlen(cmenu),12,"#%d",rc);

         if (cmdline[0]=='@') { 
            // push item to history list
            history->add(strdup(cmenu));
            strcpy(cmenu, cmdline+1); 
            continue; 
         } else {
            char    *cptr = cmdline;
            cmd_state cst;
            // clear screen or not?
            if (cptr[0]=='~') cptr++; else vio_clearscr();
            /* special processing for "cmd /e" to leave all "&" symbols in line
               untouched */
            if (strncmp(cptr,"cmd /e ",7)==0) {
               str_list sl;
               sl.count = 1;
               sl.item[0] = cptr;
               cst = cmd_initbatch(&sl, 0);
            } else
               cst = cmd_init(cptr,0);
            cmd_run(cst,CMDR_ECHOOFF);
            cmd_close(cst);
            // count kernel launching errors
            if (iskernel) kernfail++;
         }
      } else {
         // go back in history
         if (history->count()>0) {
            strcpy(cmenu, history->value(history->max())); 
            history->del(history->max(),1);
            rc = 1;
         }
      }
   }
   DoneParameters();
   history->freeitems(0,FFFF);
   DELETE(history); DELETE(ld_ini); DELETE(m_ini);
   free(menu_fname);
}
