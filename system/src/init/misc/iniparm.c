//
// QSINIT
// critical ini parameters read
//
#include "clib.h"
#include "qsutil.h"
#include "qsint.h"
#include "qsstor.h"
#include "vio.h"

static const char *strList[]={"DBPORT", "DISKSIZE", "BAUDRATE", "RESETMODE",
   "NOAF", "UNZALL", 0};

extern u32t    BaudRate,
              Disk1Size;
extern u16t ComPortAddr;
extern u8t  useint13ext,
                useAFio,
               safeMode,
              mod_delay;
extern char  aboutstr[];

#ifdef INITDEBUG
void earlyserinit(void);
#endif

static int cmpname(const char *name, char *pos, u32t len) {
   pos--;
   while ((*pos==' '||*pos=='\t')&&len) { pos--; len--; }
   return len?strnicmp(name,pos-len+1,len):1;
}
        
void get_ini_parm(void) {
   u32t  size = 0;
   void*  ini = 0;
   u32t btype = hlp_boottype();
   if (btype==QSBT_FAT || btype==QSBT_SINGLE) ini = hlp_freadfull("QSINIT.INI",&size,0);
   if (!ini) ini = hlp_freadfull("OS2LDR.INI",&size,0);
   
   if (ini) {
      char* cfgp = (char*)hlp_memrealloc(ini, size+1);
      char skipeol = 0, lstart = 1, backch = 0,
             error = 0, // error in string
            reason = 0, // collecting now: 1 - section name, 2 - key name, 3 - key
           section = 0; // 1 - config
      int    value = 0,
           errline = 1,
             strln =-1;
      if (!cfgp) return; else ini=cfgp;
      cfgp[size++]=0;
      sto_save(STOKEY_INIDATA,cfgp,size,0);

      while (size--) {
         char cc=*cfgp++;
         if (!size) cc='\n';
         if (skipeol && cc=='\n') skipeol=0;

         if (!skipeol)
         switch (cc) {
            case ' ' :
            case '\t':
               if (strln>0) strln++;
               break;
            case '\n':
               if (strln>0 && reason<3) { backch=1; error=1; break; }
               errline++;
            case '\r': {
               if (strln > 0 || value) {
                  long rc = str2long(cfgp-1-strln);
                  switch (value) {
                     // "DBPORT"
                     case 1:
                        if (rc>0 && !safeMode) hlp_seroutset(rc,0);
                        break;
                     // "DISKSIZE"
                     case 2: if (rc>0) Disk1Size=(u32t)rc<<10; break;
                     // "BAUDRATE"
                     case 3: 
                        hlp_seroutset(0,rc);
                        break;
                     // "RESETMODE"
                     case 4:
                        if (rc) {
                           if (rc!=43 && rc!=50) rc=25;
                           vio_setmode(rc);
                        }
                        break;
                     // "NOAF"
                     case 5: useAFio=0; break;
                     // "UNZALL"
                     case 6: mod_delay=0; break;
                  }
                  strln=0; value=0;
               }
               lstart=1; reason=0;
               break;
             }
            case '=' :
               if (reason==2 && strln>0) {
                  value=0;
                  if (section==1) {     // "config"
                     int ii=0;
                     while (strList[ii])
                        if (!cmpname(strList[ii++], cfgp-1, strln)) {
                           value=ii;
                           break;
                        }
                  }
                  if (!value) skipeol=1; else reason=3;
                  strln=0;
               } else error=1;
               break;
            case ';' : if (lstart) skipeol=1; else
                  if ( strln> 0 ) strln++;
               break;
            case '[' : if (lstart) { strln=0; reason=1; section=0; } 
                  else error=1;
               break;
            case ']' :
               if (reason==1 && !lstart) {
                  if (!cmpname("CONFIG", cfgp-1, strln)) section=1; else
                     section=0;
                  strln=0; skipeol=1;
               } else
                  error=1;
               reason=0;
               break;
            default:
               if (lstart==1 && section) { reason=2; strln=0; }
               lstart=0;
               if (strln>=0) strln++;
         }

         if (backch) { *--cfgp=cc; size++; backch=0; }
         if (error) { skipeol=1; error=0; reason=0; strln=0; }
      }
      //hlp_memfree(ini);
   } else {
#ifdef INITDEBUG
      ComPortAddr=0x2F8;
      rmcall(earlyserinit,0);
#endif
   }
   log_printf("hi!\n");
   log_printf("%s\n",aboutstr);
}
