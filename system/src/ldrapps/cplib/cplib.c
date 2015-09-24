//
// QSINIT
// single byte code page support module
// ChaN`s code from FatFs partially used
//
/*  437   U.S. (OEM)
    720   Arabic (OEM)
    1256  Arabic (Windows)
    737   Greek (OEM)
    1253  Greek (Windows)
    1250  Central Europe (Windows)
    775   Baltic (OEM)
    1257  Baltic (Windows)
    850   Multilingual Latin 1 (OEM)
    852   Latin 2 (OEM)
    1252  Latin 1 (Windows)
    855   Cyrillic (OEM)
    1251  Cyrillic (Windows)
    866   Russian (OEM)
    857   Turkish (OEM)
    1254  Turkish (Windows)
    858   Multilingual Latin 1 + Euro (OEM)
    862   Hebrew (OEM)
    1255  Hebrew (Windows)
    874   Thai (OEM, Windows)
    1258  Vietnam (OEM, Windows)
*/

#include "stdlib.h"
#include "qsint.h"
#include "qcl/cplib.h"
#include "qsshell.h"
#include "errno.h"
#include "qsutil.h"
#include "qsmodext.h"
#include "vio.h"

#include "cptables.h"

#define CPAGES    (21)

static wchar_t *ctables[] = { Tbl0437, Tbl0720, Tbl0737, Tbl0775, Tbl0850,
   Tbl0852, Tbl0855, Tbl0857, Tbl0858, Tbl0862, Tbl0866, Tbl0874, Tbl1250,
   Tbl1251, Tbl1252, Tbl1253, Tbl1254, Tbl1255, Tbl1256, Tbl1257, Tbl1258};

static u8t     *utables[] = { Upr0437, Upr0720, Upr0737, Upr0775, Upr0850,
   Upr0852, Upr0855, Upr0857, Upr0858, Upr0862, Upr0866, Upr0874, Upr1250,
   Upr1251, Upr1252, Upr1253, Upr1254, Upr1255, Upr1256, Upr1257, Upr1258};

static u16t cpages[CPAGES] = { 437, 720, 737, 775, 850, 852, 855, 857, 858,
   862, 866, 874, 1250, 1251, 1252, 1253, 1254, 1255, 1256, 1257, 1258};

#define CPLIB_SIGN        0x49534C4E          // NLSI

#define instance_ret(inst,err)              \
   cpconv_data *inst = (cpconv_data*)data;  \
   if (inst->sign!=CPLIB_SIGN) return err;

#define instance_void(inst)                 \
   cpconv_data *inst = (cpconv_data*)data;  \
   if (inst->sign!=CPLIB_SIGN) return;

codepage_info  cpisys = { 0 };
u32t          classid = 0;
u16t            cpcur = 0;
u32t            cpidx = 0;

typedef struct {
   u32t         sign;
   u16t     codepage;
   u32t        index;
} cpconv_data;

wchar_t _std towlower(wchar_t chr) {
   wchar_t *lwr = memchrw(tbl_upper, chr, UPPER_TABLE_SIZE);
   return lwr ? tbl_lower[lwr-tbl_upper] : chr;
}

wchar_t _std towupper(wchar_t chr) {
   wchar_t *upr = memchrw(tbl_lower, chr, UPPER_TABLE_SIZE);
   return upr ? tbl_upper[upr-tbl_lower] : chr;
}

static wchar_t convert(wchar_t *table, wchar_t chr, int dir) {
   wchar_t cc;
   if (chr<0x80) cc = chr; else {
      if (dir) { // OEMCP to Unicode
         cc = chr>=0x100 ? 0 : table[chr-0x80];
      } else {   // Unicode to OEMCP
         for (cc = 0; cc<0x80; cc++)
            if (chr==table[cc]) break;
         cc = cc+0x80 & 0xFF;
      }
   }
   return cc;
}

static u16t _std ff_convert(u16t src, int to_unicode) {
   if (!cpcur) return src>=0x80?0:src;
   return convert(ctables[cpidx], src, to_unicode);
}

static u16t _std ff_wtoupper(u16t src) {
   return towupper(src);
}

u32t _std cpconv_setcp(void *data, u16t cp) {
   u16t *pos;
   instance_ret(cpd,0);
   pos = memchrw(cpages, cp, CPAGES);
   if (!pos) return 0;
   cpd->codepage = cp;
   cpd->index    = pos - cpages;
   return 1;
}

wchar_t _std cpconv_convert(void *data, wchar_t chr, int dir) {
   instance_ret(cpd,0);
   if (!cpd->codepage) return chr;
   return convert(ctables[cpd->index], chr, dir);
}

char _std cpconv_toupper(void *data, char chr) {
   instance_ret(cpd,chr);
   if (!cpd->codepage) return chr;
   return chr>=0x80 ? utables[cpd->index][chr-0x80] : toupper(chr);
}

char _std cpconv_tolower(void *data, char chr) {
   wchar_t cc;
   instance_ret(cpd,chr);
   if (!cpd->codepage) return chr;
   // long way: oem->1200->upper->oem
   cc = cpconv_convert(data, chr, 1);
   if (!cc) return chr;
   cc = towlower(cc);
   cc = cpconv_convert(data, cc, 0);
   return cc ? cc : chr;
}

u16t* _std cpconv_cplist(void *data) {
   u16t* rc = (u16t*)malloc((CPAGES+1)*2);
   memcpy(rc, &cpages, CPAGES*2);
   rc[CPAGES] = 0;
   return rc;
}

u16t _std cpconv_getsyscp(void *data) {
   return cpcur;
}

u8t* _std cpconv_uprtab(void *data, u16t cp) {
   instance_ret(cpd,0);
   if (!cp) cp = cpcur;
   if (cp) {
      u16t *pos = memchrw(cpages, cp, CPAGES);
      if (!pos) return 0;
      return utables[pos - cpages];
   }
   return 0;
}

u32t _std cpconv_setsyscp(void *data, u16t cp) {
   u16t *pos = memchrw(cpages, cp, CPAGES);
   if (!pos) return 0;
   cpcur = cp;
   cpidx = pos - cpages;
   cpisys.cpnum    = cp;
   cpisys.oemupr   = utables[cpidx];
   cpisys.convert  = ff_convert;
   cpisys.wtoupper = ff_wtoupper;
   // setup page support in FatFs
   hlp_setcpinfo(&cpisys);
   return 1;
}

static void *methods_list[] = { cpconv_setcp, cpconv_convert, cpconv_toupper,
   cpconv_tolower, cpconv_cplist, cpconv_getsyscp, cpconv_setsyscp, cpconv_uprtab };

static void _std cpconv_init(void *instance, void *data) {
   cpconv_data *cpd = (cpconv_data*)data;
   cpd->sign     = CPLIB_SIGN;
   cpd->codepage = cpcur;
   cpd->index    = cpidx;
}

static void _std cpconv_done(void *instance, void *data) {
   instance_void(cpd);
   cpd->sign = 0;
}

u32t _std shl_chcp(const char *cmd, str_list *args) {
   if (args->count==1 && strcmp(args->item[0],"/?")==0) {
      cmd_shellhelp(cmd,CLR_HELP);
      return 0;
   }
   cmd_printseq(0,1,0);

   if (args->count<=1) {
      if (args->count==0) {
         if (!cpcur) printf("There is no code page selected\n");
            else printf("Active code page : %hd\n", cpcur);
         return 0;
      } else
      if (stricmp(args->item[0],"LIST")==0) {
         char *cpstr = strdup("Supported code pages : "), buf[16];
         int ii;
         for (ii=0; ii<CPAGES; ii++) {
            sprintf(buf, "%hd", cpages[ii]);
            if (ii<CPAGES-1) strcat(buf, ", ");
            cpstr = strcat_dyn(cpstr, buf);
         }
         cmd_printtext(cpstr,0,0,0);
         free(cpstr);
         return 0;
      } else {
         long cp = str2long(args->item[0]);
         if (cp>0)
            if (!cpconv_setsyscp(0,cp)) {
               printf("There is no code page %d\n", cp);
               return EINVAL;
            } else
               return 0;
      }
   }
   cmd_shellerr(EINVAL,0);
   return EINVAL;
}

unsigned __cdecl LibMain(unsigned hmod, unsigned termination) {
   if (!termination) {
      if (mod_query(mod_getname(hmod,0), MODQ_LOADED|MODQ_NOINCR)) {
         vio_setcolor(VIO_COLOR_LRED);
         printf("%s already loaded!\n", mod_getname(hmod,0));
         vio_setcolor(VIO_COLOR_RESET);
         return 0;
      }
      classid = exi_register("qs_cpconvert", methods_list,
         sizeof(methods_list)/sizeof(void*), sizeof(cpconv_data), cpconv_init,
            cpconv_done, 0);
      if (!classid) {
         log_printf("unable to register cpconvert class!\n");
         return 0;
      }
      cmd_shelladd("CHCP",shl_chcp);
   } else {
      // remove cp class
      if (classid)
         if (exi_unregister(classid)) classid = 0;
      // DENY unload if class was not unregistered
      if (classid) return 0;

      cmd_shellrmv("CHCP",shl_chcp);
      // drop code page support in FatFs
      hlp_setcpinfo(0);
   }
   return 1;
}
