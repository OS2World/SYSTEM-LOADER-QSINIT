//
// QSINIT
// single byte code page support module
// ChaN`s code from FatFs partially used
//
/*  437   U.S.
    720   Arabic
    737   Greek
    771   KBL
    775   Baltic
    850   Latin 1
    852   Latin 2
    855   Cyrillic
    857   Turkish
    860   Portuguese
    861   Icelandic
    862   Hebrew
    863   Canadian French
    864   Arabic
    865   Nordic
    866   Russian
    869   Greek 2
*/

#include "stdlib.h"
#include "qcl/cplib.h"
#include "qsshell.h"
#include "errno.h"
#include "qsutil.h"
#include "qsmodext.h"
#include "qssys.h"
#include "qsint.h"
#include "vio.h"

#include "cptables.h"

#define CPAGES    (17)

static wchar_t *ctables[] = { Tbl0437, Tbl0720, Tbl0737, Tbl0771, Tbl0775,
   Tbl0850, Tbl0852, Tbl0855, Tbl0857, Tbl0860, Tbl0861, Tbl0862, Tbl0863,
   Tbl0864, Tbl0865, Tbl0866, Tbl0869 };

static u8t     *utables[] = { Upr0437, Upr0720, Upr0737, Upr0771, Upr0775,
   Upr0850, Upr0852, Upr0855, Upr0857, Upr0860, Upr0861, Upr0862, Upr0863,
   Upr0864, Upr0865, Upr0866, Upr0869 };

static u16t cpages[CPAGES] = { 437, 720, 737, 771, 775, 850, 852, 855, 857,
   860, 861, 862, 863, 864, 865, 866, 869 };

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

void _std sys_notifyexec(u32t eventtype, void *info);

wchar_t _std towlower(wchar_t chr) {
  if (chr < 0x80) {
     return chr>=0x61 && chr<=0x7A ? chr-0x20 : chr;
  } else {
     wchar_t *lwr = memchrw(tbl_upper, chr, UPPER_TABLE_SIZE);
     return lwr ? tbl_lower[lwr-tbl_upper] : chr;
  }
}

#if 0
wchar_t _std towupper(wchar_t chr) {
  if (chr < 0x80) {
     return chr>=0x61 && chr<=0x7A ? chr-0x20 : chr;
  } else {
     wchar_t *upr = memchrw(tbl_lower, chr, UPPER_TABLE_SIZE);
     return upr ? tbl_upper[upr-tbl_lower] : chr;
  }
}
#else
/* UGLY thing here - towlower() uses OLD code & towupper() has many additional
   characters, i.e. reverse convertion would fail. towlower() should be rewritten
   too, but this complex array seems too crazy to revert ;) */
wchar_t _std towupper(wchar_t chr) {
   /* Compressed upper conversion table */
   static const wchar_t cvt1[] = {   /* U+0000 - U+0FFF */
      /* Basic Latin */
      0x0061,0x031A,
      /* Latin-1 Supplement */
      0x00E0,0x0317,  0x00F8,0x0307,  0x00FF,0x0001,0x0178,
      /* Latin Extended-A */
      0x0100,0x0130,  0x0132,0x0106,  0x0139,0x0110,  0x014A,0x012E,  0x0179,0x0106,
      /* Latin Extended-B */
      0x0180,0x004D,0x0243,0x0181,0x0182,0x0182,0x0184,0x0184,0x0186,0x0187,0x0187,0x0189,0x018A,0x018B,0x018B,0x018D,0x018E,0x018F,0x0190,0x0191,0x0191,0x0193,0x0194,0x01F6,0x0196,0x0197,0x0198,0x0198,0x023D,0x019B,0x019C,0x019D,0x0220,0x019F,0x01A0,0x01A0,0x01A2,0x01A2,0x01A4,0x01A4,0x01A6,0x01A7,0x01A7,0x01A9,0x01AA,0x01AB,0x01AC,0x01AC,0x01AE,0x01AF,0x01AF,0x01B1,0x01B2,0x01B3,0x01B3,0x01B5,0x01B5,0x01B7,0x01B8,0x01B8,0x01BA,0x01BB,0x01BC,0x01BC,0x01BE,0x01F7,0x01C0,0x01C1,0x01C2,0x01C3,0x01C4,0x01C5,0x01C4,0x01C7,0x01C8,0x01C7,0x01CA,0x01CB,0x01CA,
      0x01CD,0x0110,  0x01DD,0x0001,0x018E,  0x01DE,0x0112,  0x01F3,0x0003,0x01F1,0x01F4,0x01F4,  0x01F8,0x0128,
      0x0222,0x0112,  0x023A,0x0009,0x2C65,0x023B,0x023B,0x023D,0x2C66,0x023F,0x0240,0x0241,0x0241,  0x0246,0x010A,
      /* IPA Extensions */
      0x0253,0x0040,0x0181,0x0186,0x0255,0x0189,0x018A,0x0258,0x018F,0x025A,0x0190,0x025C,0x025D,0x025E,0x025F,0x0193,0x0261,0x0262,0x0194,0x0264,0x0265,0x0266,0x0267,0x0197,0x0196,0x026A,0x2C62,0x026C,0x026D,0x026E,0x019C,0x0270,0x0271,0x019D,0x0273,0x0274,0x019F,0x0276,0x0277,0x0278,0x0279,0x027A,0x027B,0x027C,0x2C64,0x027E,0x027F,0x01A6,0x0281,0x0282,0x01A9,0x0284,0x0285,0x0286,0x0287,0x01AE,0x0244,0x01B1,0x01B2,0x0245,0x028D,0x028E,0x028F,0x0290,0x0291,0x01B7,
      /* Greek, Coptic */
      0x037B,0x0003,0x03FD,0x03FE,0x03FF,  0x03AC,0x0004,0x0386,0x0388,0x0389,0x038A,  0x03B1,0x0311,
      0x03C2,0x0002,0x03A3,0x03A3,  0x03C4,0x0308,  0x03CC,0x0003,0x038C,0x038E,0x038F,  0x03D8,0x0118,
      0x03F2,0x000A,0x03F9,0x03F3,0x03F4,0x03F5,0x03F6,0x03F7,0x03F7,0x03F9,0x03FA,0x03FA,
      /* Cyrillic */
      0x0430,0x0320,  0x0450,0x0710,  0x0460,0x0122,  0x048A,0x0136,  0x04C1,0x010E,  0x04CF,0x0001,0x04C0,  0x04D0,0x0144,
      /* Armenian */
      0x0561,0x0426,

      0x0000
   };
   static const wchar_t cvt2[] = {   /* U+1000 - U+FFFF */
      /* Phonetic Extensions */
      0x1D7D,0x0001,0x2C63,
      /* Latin Extended Additional */
      0x1E00,0x0196,  0x1EA0,0x015A,
      /* Greek Extended */
      0x1F00,0x0608,  0x1F10,0x0606,  0x1F20,0x0608,  0x1F30,0x0608,  0x1F40,0x0606,
      0x1F51,0x0007,0x1F59,0x1F52,0x1F5B,0x1F54,0x1F5D,0x1F56,0x1F5F,  0x1F60,0x0608,
      0x1F70,0x000E,0x1FBA,0x1FBB,0x1FC8,0x1FC9,0x1FCA,0x1FCB,0x1FDA,0x1FDB,0x1FF8,0x1FF9,0x1FEA,0x1FEB,0x1FFA,0x1FFB,
      0x1F80,0x0608,  0x1F90,0x0608,  0x1FA0,0x0608,  0x1FB0,0x0004,0x1FB8,0x1FB9,0x1FB2,0x1FBC,
      0x1FCC,0x0001,0x1FC3,  0x1FD0,0x0602,  0x1FE0,0x0602,  0x1FE5,0x0001,0x1FEC,  0x1FF2,0x0001,0x1FFC,
      /* Letterlike Symbols */
      0x214E,0x0001,0x2132,
      /* Number forms */
      0x2170,0x0210,  0x2184,0x0001,0x2183,
      /* Enclosed Alphanumerics */
      0x24D0,0x051A,  0x2C30,0x042F,
      /* Latin Extended-C */
      0x2C60,0x0102,  0x2C67,0x0106, 0x2C75,0x0102,
      /* Coptic */
      0x2C80,0x0164,
      /* Georgian Supplement */
      0x2D00,0x0826,
      /* Full-width */
      0xFF41,0x031A,

      0x0000
   };
   const wchar_t *p;
   wchar_t bc, nc, cmd;

   p = chr < 0x1000 ? cvt1 : cvt2;
   for (;;) {
       bc = *p++;                              /* Get block base */
       if (!bc || chr < bc) break;
       nc = *p++; cmd = nc >> 8; nc &= 0xFF;   /* Get processing command and block size */
       if (chr < bc + nc) {    /* In the block? */
           switch (cmd) {
           case 0: chr = p[chr - bc]; break;       /* Table conversion */
           case 1: chr -= (chr - bc) & 1; break;   /* Case pairs */
           case 2: chr -= 16; break;               /* Shift -16 */
           case 3: chr -= 32; break;               /* Shift -32 */
           case 4: chr -= 48; break;               /* Shift -48 */
           case 5: chr -= 26; break;               /* Shift -26 */
           case 6: chr += 8; break;                /* Shift +8 */
           case 7: chr -= 80; break;               /* Shift -80 */
           case 8: chr -= 0x1C60; break;           /* Shift -0x1C60 */
           }
           break;
       }
       if (!cmd) p += nc;
   }

   return chr;
}
#endif

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

// note, that this and next functions called under cli!
static u16t _std _ff_convert(u16t src, int to_unicode) {
   if (!cpcur) return src>=0x80?0:src;
   return convert(ctables[cpidx], src, to_unicode);
}

static u16t _std _ff_wtoupper(u16t src) {
   return towupper(src);
}

u32t _exicc cpconv_setcp(EXI_DATA, u16t cp) {
   u16t *pos;
   instance_ret(cpd,0);
   pos = memchrw(cpages, cp, CPAGES);
   if (!pos) return 0;
   mt_swlock();
   cpd->codepage = cp;
   cpd->index    = pos - cpages;
   mt_swunlock();
   return 1;
}

wchar_t _exicc cpconv_convert(EXI_DATA, wchar_t chr, int dir) {
   instance_ret(cpd,0);
   if (!cpd->codepage) return chr;
   return convert(ctables[cpd->index], chr, dir);
}

char _exicc cpconv_toupper(EXI_DATA, char chr) {
   instance_ret(cpd,chr);
   if (!cpd->codepage) return chr;
   return chr>=0x80 ? utables[cpd->index][chr-0x80] : toupper(chr);
}

char _exicc cpconv_tolower(EXI_DATA, char chr) {
   wchar_t cc;
   instance_ret(cpd,chr);
   if (!cpd->codepage) return chr;
   // long way: oem->1200->lower->oem
   cc = cpconv_convert(data, 0, chr, 1);
   if (!cc) return chr;
   cc = towlower(cc);
   cc = cpconv_convert(data, 0, cc, 0);
   return cc ? cc : chr;
}

u16t* _exicc cpconv_cplist(EXI_DATA) {
   u16t* rc = (u16t*)malloc((CPAGES+1)*2);
   memcpy(rc, &cpages, CPAGES*2);
   rc[CPAGES] = 0;
   mem_localblock(rc);
   return rc;
}

u16t _exicc cpconv_getsyscp(EXI_DATA) {
   return cpcur;
}

u8t* _exicc cpconv_uprtab(EXI_DATA, u16t cp) {
   instance_ret(cpd,0);
   if (!cp) cp = cpcur;
   if (cp) {
      u16t *pos = memchrw(cpages, cp, CPAGES);
      if (!pos) return 0;
      return utables[pos - cpages];
   }
   return 0;
}

u32t _exicc cpconv_setsyscp(EXI_DATA, u16t cp) {
   u16t *pos = memchrw(cpages, cp, CPAGES);
   if (!pos) return 0;
   mt_swlock();
   cpcur = cp;
   cpidx = pos - cpages;
   cpisys.cpnum    = cp;
   cpisys.oemupr   = utables[cpidx];
   cpisys.convert  = _ff_convert;
   cpisys.wtoupper = _ff_wtoupper;
   // system notify
   sys_notifyexec(SECB_CPCHANGE, &cpisys);
   mt_swunlock();
   return 1;
}

wchar_t _exicc cpconv_towlower(EXI_DATA, wchar_t chr) {
   return towlower(chr);
}

wchar_t _exicc cpconv_towupper(EXI_DATA, wchar_t chr) {
   return towupper(chr);
}

static void *methods_list[] = { cpconv_setcp, cpconv_convert, cpconv_toupper,
   cpconv_tolower, cpconv_cplist, cpconv_getsyscp, cpconv_setsyscp, cpconv_uprtab,
   cpconv_towlower, cpconv_towupper};

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
            if (!cpconv_setsyscp(0, 0, cp)) {
               printf("There is no code page %d\n", cp);
               return EINVAL;
            } else
               return 0;
      }
   }
   cmd_shellerr(EMSG_CLIB,EINVAL,0);
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
         sizeof(methods_list)/sizeof(void*), sizeof(cpconv_data), 0,
            cpconv_init, cpconv_done, 0);
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
      // drop code page support
      sys_notifyexec(SECB_CPCHANGE,0);
   }
   return 1;
}
