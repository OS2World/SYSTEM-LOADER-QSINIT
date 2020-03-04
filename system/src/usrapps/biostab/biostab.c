//
// BIOS tables dump.
// ------------------------------------------------------------
// biosdecode from dmidecode was used as a reference
//
#include <qsutil.h>
#include <qsshell.h>
#include <stdlib.h>
#include "qssys.h"

#define _W(x) (u16t)(*(u16t*)(x))
#define _D(x) (u32t)(*(u32t*)(x))
#define _Q(x) (*(u64t*)(x))
#define prn    cmd_printf

static int pciv = 0;

typedef struct {
   char     *sign;
   u32t       ofs;
   // return detected size or 0
   u32t   (*check)(u8t *ptr);
} TableDef;

int sum(u8t *ptr, u32t len) {
   u32t  ii = 0;
   u8t  sum = 0;
   while (ii<len) sum += ptr[ii++];
   return sum==0;
}

u32t is_smbios3(u8t *ptr) {
   u32t len = ptr[6];
   if (len<0x18 || !sum(ptr,len)) return 0;

   prn("SMBIOS %u.%u.%u\n", ptr[7], ptr[8], ptr[9]);
   prn("\tStructure Table Maximum Length: %u bytes\n", _D(ptr+12));
   prn("\tStructure Table 64-bit Address: 0x%010LX%08X\n", _Q(ptr+16));

   return len;
}

u32t is_smbios(u8t *ptr) {
   u32t len = ptr[5]==0x1E ? 0x1F : ptr[5];
   if (len<0x1F || !sum(ptr,ptr[5]) || memcmp("_DMI_", ptr+0x10, 5) ||
      !sum(ptr+0x10, 0x0F)) return 0;

   prn("SMBIOS %u.%u\n", ptr[6], ptr[7]);
   prn("\tStructure Table Length: %u bytes\n", _W(ptr+0x16));
   prn("\tStructure Table Address: 0x%08X\n", _D(ptr+0x18));
   prn("\tNumber Of Structures: %u\n", _W(ptr+0x1C));
   prn("\tMaximum Structure Size: %u bytes\n", _W(ptr+8));

   return len;
}

u32t is_dmi(u8t *ptr) {
   u32t len = 15;
   if (!sum(ptr,len)) return 0;

   prn("Legacy DMI %u.%u\n", ptr[0x0E]>>4, ptr[0x0E]&0x0F);
   prn("\tStructure Table Length: %u bytes\n", _W(ptr+6));
   prn("\tStructure Table Address: 0x%08X\n", _D(ptr+8));
   prn("\tNumber Of Structures: %u\n", _W(ptr+12));

   return len;
}

u32t is_sysid(u8t *ptr) {
   u32t len = _W(ptr+8);
   if (len<0x11 || !sum(ptr,len)) return 0;

   prn("SYSID\n");
   prn("\tRevision: %u\n", ptr[0x10]);
   prn("\tStructure Table Address: 0x%08X\n", _D(ptr+10));
   prn("\tNumber Of Structures: %u\n", _W(ptr+14));

   return len;
}

u32t is_pnp(u8t *ptr) {
   static const char *ntype[] = { "Not Supported", "Polling", "Asynchronous",
                                  "Unknown"};
   u32t len = ptr[5];
   if (len<0x21 || !sum(ptr,len)) return 0;

   prn("PNP BIOS %u.%u\n", ptr[4]>>4, ptr[4]&0x0F);
   prn("\tEvent Notification: %s\n", ntype[_W(ptr+6)&3]);

   if ((_W(ptr+0x06)&3)==1)
      prn("\tEvent Notification Flag Address: 0x%08X\n", _D(ptr+9));
   prn("\tReal Mode 16-bit Code Address: %04X:%04X\n", _W(ptr+15), _W(ptr+13));
   prn("\tReal Mode 16-bit Data Address: %04X:0000\n", _W(ptr+0x1B));
   prn("\t16-bit Protected Mode Code Address: %08X\n",
      _D(ptr + 0x13) + _W(ptr + 0x11));
   prn("\t16-bit Protected Mode Data Address: %08X\n", _D(ptr+0x1D));
   if (_D(ptr+0x17))
      prn("\tOEM Device Identifier: %c%c%c%02X%02X\n",
         0x40+((ptr[0x17]>>2)&0x1F), 0x40+((ptr[0x17]&3)<<3)+((ptr[0x18]>>5)&7),
            0x40+(ptr[0x18]&0x1F), ptr[0x19], ptr[0x20]);

   return len;
}

u32t is_acpi(u8t *ptr) {
   u32t len = ptr[15]==2 ? 36 : 20;
   if (!sum(ptr,20)) return 0;

   prn("ACPI%s\n", ptr[15]==0?" 1.0":(ptr[15]==2?" 2.0":""));
   prn("\tOEM: %c%c%c%c%c%c\n", ptr[9], ptr[10], ptr[11], ptr[12], ptr[13], ptr[14]);
   prn("\tRSD Table 32-bit Address: 0x%08X\n", _D(ptr+16));

   if (len>=36) {
      u32t len2 = _D(ptr+20);
      if (len2>len || !sum(ptr,len2)) return 0;
      if (len2>=32)
         prn("\tXSD Table 64-bit Address: 0x%010LX\n", _Q(ptr+24));
   }
   return len;
}

u32t is_bios32(u8t *ptr) {
   u32t len = (u32t)ptr[9]<<4;
   if (len<10 || !sum(ptr,len)) return 0;

   prn("BIOS32 Service Directory\n");
   prn("\tRevision: %u\n", ptr[8]);
   prn("\tCalling Interface Address: 0x%08X\n", _D(ptr+4));

   return len;
}

static void pci_irqs(u16t code) {
   if (!code) prn(" None"); else {
      u32t ii;
      for (ii=0; ii<16; ii++)
        if (code&(1<<ii)) prn(" %u", ii);
   }
}

static void pci_link(char letter, const u8t *ptr) {
   if (!ptr[0]) return;
   prn("\t\tINT%c#: Link 0x%02X, IRQ Bitmap", letter, ptr[0]);
   pci_irqs(_W(ptr+1));
   prn("\n");
}

u32t is_pci(u8t *ptr) {
   int   ii, n, sumok;
   u32t len = _W(ptr+6);
   if (len<32) return 0;
   sumok = sum(ptr,len);
   // print error instead of failure
   prn("PCI Interrupt Routing %u.%u %s\n", ptr[5], ptr[4], sumok?"":
       "(table sum mismatch)");
   prn("\tRouter Device: %02X:%02X.%X\n", ptr[8], ptr[9]>>3, ptr[9]&7);
   prn("\tExclusive IRQs:");
   pci_irqs(_W(ptr+10));
   prn("\n");
   if (_D(ptr+12))
      prn("\tCompatible Router: %04X:%04X\n", _W(ptr+12), _W(ptr+14));
   if (_D(ptr+16))
      prn("\tMiniport Data: 0x%08X\n", _D(ptr+16));

   n = len-32 >> 4;
   for (ii=1, ptr+=32; ii<=n; ii++, ptr+=16) {
      prn("\tDevice: %02X:%02X,", ptr[0], ptr[1]>>3);
      if (!ptr[14]) prn(" on-board\n"); else prn(" slot %u\n", ptr[14]);
      if (pciv) {
         pci_link('A', ptr+ 2);
         pci_link('B', ptr+ 5);
         pci_link('C', ptr+ 8);
         pci_link('D', ptr+11);
      }
   }
   return len;
}

u32t is_mp(u8t *ptr) {
   u32t len = (u32t)ptr[8]<<4;
   if (!sum(ptr,len)) return 0;

   prn("Intel Multiprocessor Configuration\n");
   prn("\tSpecification Revision: %s\n", ptr[9]==1 ? "1.1" :
      (ptr[9]==4?"1.4":"Invalid"));
   if (ptr[11]) prn("\tDefault Configuration: #%d\n", ptr[11]);
      else prn("\tConfiguration Table Address: 0x%08X\n", _D(ptr+4));
   prn("\tMode: %s\n", ptr[12]&1<<7 ? "IMCR and PIC" : "Virtual Wire");

   return len;
}

static TableDef tbl[] = {
   {"\x8RSD PTR ", 0      , is_acpi},    {"\4_SM_" , 0x10000, is_smbios},
   {"\5_SM3_"    , 0x10000, is_smbios3}, {"\5_DMI_", 0x10000, is_dmi},
   {"\4$PnP"     , 0x10000, is_pnp},     {"\4_32_" , 0      , is_bios32},
   {"\4$PIR"     , 0x10000, is_pci},     {"\4_MP_" , 0      , is_mp},
   {"\7_SYSID_"  , 0      , is_sysid},   {0,0,0}
};

int main(int argc, char *argv[]) {
   u32t    ii, ofs;
   int  qsapi = 0;
   char  *src = 0;
   u32t  opts = PRNSEQ_INIT;

   for (ii=1; ii<argc; ii++) {
      if (argv[ii][0]=='/') argv[ii][0] = '-';
      if (strnicmp(argv[ii], "-f=", 3)==0) src = argv[ii]+3; else
      if (stricmp(argv[ii], "-np")==0) opts |= PRNSEQ_INITNOPAUSE; else
      if (stricmp(argv[ii], "-e")==0) qsapi = 1; else
      if (stricmp(argv[ii], "-pv")==0) pciv = 1; else
      if (stricmp(argv[ii], "-log")==0) opts |= PRNSEQ_LOGCOPY; else {
         printf("biostab:\n"
                "  -log     copy output to the log\n"
                "  -np      disable end of screen \"pause\"\n"
                "  -e       use QSINIT API instead of own BIOS search\n"
                "  -pv      show PCI IRQ routing table\n"
                "  -f=path  use E0000..FFFFF dump file (128kb) or F0000..FFFFF (64kb)\n");
         return 1;
      }
   }
   if (qsapi && src) qsapi = 0;

   cmd_printseq(0, opts, 0);

   if (qsapi) {
      for (ii=SYSTAB_ACPI; ii<=SYSTAB_SYSID; ii++) {
         bios_table_ptr bt;

         if (sys_gettable(ii,&bt)==0) {
            prn("[%08X] %s", bt.addr, bt.status&1?"(wrong table) ":"");
            if (!tbl[ii].check((u8t*)bt.addr)) prn("\n");
         }
      }
   } else {
      u8t *mem = (u8t*)malloc(_128KB);
      if (src) {
         u32t size;
         void  *fd = freadfull(src, &size);
         if (!fd) {
            printf("no file \"%s\" found!\n", src);
            return 2;
         } else
         if (size!=_64KB && size!=_128KB) {
            printf("file size should be exact 64kb or 128kb!\n");
            return 2;
         } else
         if (size==_128KB) memcpy(mem, fd, _128KB); else {
            memset(mem, 0, _64KB);
            memcpy(mem+_64KB, fd, _64KB);
         }
         hlp_memfree(fd);
      } else
         memcpy(mem, (void*)hlp_segtoflat(0xE000), _128KB);
      
      for (ofs=0; ofs<=0x1FFF0; ofs+=16) {
         u8t *ptr = mem+ofs;
      
         for (ii=0; tbl[ii].sign; ii++)
            if (ofs>=tbl[ii].ofs && tbl[ii].sign[1]==*ptr)
               if (memcmp(ptr, tbl[ii].sign+1, tbl[ii].sign[0])==0) {
                  u32t len;
                  prn("[%05X] ", 0xE0000+ofs);
                  len = tbl[ii].check(ptr);
                  // seek forward
                  if (len) ofs += len-1>>4<<4; else prn("wrong table\n");
                  break;
               }
      }
      if (mem) free(mem);
   }

   return 0;
}
