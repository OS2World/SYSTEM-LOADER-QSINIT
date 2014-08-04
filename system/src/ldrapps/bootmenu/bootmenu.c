//
// QSINIT "boot menu" module
//
#include "stdlib.h"
#include "qsshell.h"
#include "qsint.h"
#include "qsutil.h"
#include "vio.h"
#include "qslist.h"

int  MenuKernel(char *rcline, int errors);
int  MenuCommon(char *menu, char *rcline, u32t pos);
int  MenuPtBoot(char *rcline);
void InitParameters(void);
void DoneParameters(void);

char *menu_ini = 0;

void main(int argc,char *argv[]) {
   char cmdline[1024], cmenu[128], *eptr;
   ptr_list  history;
   int        rc = 1,
        kernfail = 0;

   if (argc>1 && strcmp(argv[1],"/?")==0) {
      printf("usage: bootmenu [menu_name]\n");
      return;
   }

   history = NEW(ptr_list);

   if (argc>1) strcpy(cmenu,argv[1]); else {
      eptr = getenv("start_menu");
      if (eptr) { strncpy(cmenu,eptr,128); cmenu[127]=0; } else cmenu[0]=0;
   }
   eptr = getenv("menu_source");
   menu_ini = strdup(eptr?eptr:"1:\\menu.ini");

   InitParameters();
   while (rc) {
      char *idxpos = strchr(cmenu,'#');
      u32t     idx = 0;
      int iskernel = !*cmenu || stricmp(cmenu,"bootos2")==0,
          isptboot = stricmp(cmenu,"bootpart")==0 || stricmp(cmenu,"pt")==0;

      if (idxpos) { *idxpos++=0; idx=atoi(idxpos); }
      // launch menu
      rc = iskernel ? MenuKernel(cmdline,kernfail) : (
           isptboot ? MenuPtBoot(cmdline) : MenuCommon(cmenu,cmdline,idx));
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
   DELETE(history);
   free(menu_ini);
}
