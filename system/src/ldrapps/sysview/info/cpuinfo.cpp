#include "diskedit.h"
#include "string.h"
#include "stdlib.h"
#include "stdio.h"
#include "cpudef.h"

#define Uses_MsgBox
#define Uses_TView
#define Uses_TDialog
#define Uses_TScreen
#define Uses_TCollection
#define Uses_TScrollBar
#include "tv.h"
#include "../../hc/qsshell.h"

static TListBox     *CpuInfoList = 0;
static TColoredText   *CpuIdText = 0,
                   *CpuIdMemText = 0;

#define LARGEBOX_INC    3

TDialog* makeCpuInfoDlg(void) {
   TView *control;
   int largeBox = TScreen::screenHeight>=32 ? LARGEBOX_INC : 0;

   TDialog* dlg = new TDialog(TRect(1, 1, 79, 21+largeBox), "CPU Information");
   if (!dlg) return 0;

   dlg->options |= ofCenterX | ofCenterY;
   dlg->helpCtx  = hcCpuInfo;

   control = new TScrollBar(TRect(75, 4, 76, 19+largeBox));
   dlg->insert(control);

   CpuInfoList = new TListBox(TRect(2, 4, 75, 19+largeBox), 1, (TScrollBar*)control);
   dlg->insert(CpuInfoList);

   control = new TButton(TRect(64, 2, 73, 4), "~C~lose", cmOK, bfDefault);
   dlg->insert(control);

   control = new TStaticText(TRect(3, 2, 11, 3), "CPU ID:");
   dlg->insert(control);

   CpuIdText = new TColoredText(TRect(11, 2, 25, 3), "", 0x79);
   dlg->insert(CpuIdText);

   control = new TStaticText(TRect(30, 2, 45, 3), "System memory:");
   dlg->insert(control);

   CpuIdMemText = new TColoredText(TRect(45, 2, 53, 3), "", 0x79);
   dlg->insert(CpuIdMemText);

   dlg->selectNext(False);
   return dlg;
}

#define CPUID_CNT    9
#define CPUV_INTEL   0
#define CPUV_AMD     2

static const char *VendorIDs[CPUID_CNT]={"GenuineIntel","UMC UMC UMC ","AuthenticAMD",
  "CyrixInstead","NexGenDriven","CentaurHauls","RiseRiseRise","GenuineTMx86",
  "Geode by NSC"};

static const char *VendorStr[CPUID_CNT]={"Intel","UMC","AMD","Cyrix","NexGen",
  "Centaur","Rise","Transmeta","NSC"};


static void push_avail(TCollection *cl, int width, char **buf, const char *str) {
   int ln = strlen(*buf),
      cln = strlen(str);
   if (ln+cln>=width-1) {
      cl->insert(*buf);
      *buf = new char[width+1];
      strcpy(*buf, "           : ");
   }
   strcat(*buf,str);
}

#define PUSH_AVAIL(x) push_avail(dl,list_wdt,&str,x)
#define PUSH_EMPTY()  str = new char[2]; *str = 0; dl->insert(str);

void TSysApp::ExecCpuInfoDlg() {
   u32t data[4];
   TCollection *dl = new TCollection(0,10);
   TDialog    *dlg = makeCpuInfoDlg();

         
   char idstr[32], *str;
   int  ii, vendorNum = -1;

   if (!opts_cpuid(0,data)) {
       replace_coltxt(&CpuIdText, "486");
   } else {
       *((u32t*)idstr+0)=data[1]; *((u32t*)idstr+1)=data[3]; 
       *((u32t*)idstr+2)=data[2]; idstr[12]=0;

       str = new char[48];
       strcpy(str, "Vendor     : ");
       
       for (ii=0;ii<CPUID_CNT;ii++)
          if (strcmp(VendorIDs[ii],idstr)==0) { 
             strcat(str,VendorStr[vendorNum=ii]); 
             break; 
          }
       if (ii>=CPUID_CNT) strcat(str,idstr);
       dl->insert(str);

       opts_cpuid(1,data);
       u32t fi[5];
       fi[0] = data[2], fi[1] = data[3], fi[2] = 0, fi[3] = 0, fi[4] = 0;

       sprintf(idstr,"%d.%d.%d",data[0]>>8&0xF,data[0]>>4&0xF,data[0]&0xF);
       replace_coltxt(&CpuIdText, idstr);
       u32t    cores = data[1]>>16&0xF,
              apicid = data[1]>>24;

       opts_cpuid(0x80000000,data);
       u32t maxlevel = data[0];

       if (maxlevel>0x80000000) {
          if (maxlevel>=0x80000001) {
             opts_cpuid(0x80000001,data);
             fi[2] = data[2];
             fi[3] = data[3];
          }

          if (maxlevel>=0x80000004) {
             str = new char[96];
             strcpy(str, "Processor  : ");
             char *cp = str+strlen(str);
             *cp = 0;
             
             u32t level=0x80000002;
             while (level<0x80000005) {
               opts_cpuid(level,data);
               memcpy(cp+(level++-0x80000002)*16,data,16);
             }
             cp[48]=0;
             for (ii=0;ii<48;ii++)
                if (cp[ii]==0) cp[ii]=' ';
             for (ii=48;ii>0;)
                if (cp[--ii]!=' ') break; else cp[ii]=0;
             if (cp[0]==' ') {
                char *ps=cp;
                while (*ps==' ') ps++;
                ii=ps-cp;
                if (ii==48) *cp=0; else
                   memmove(cp,ps,48-ii+1);
             }
             dl->insert(str);

             if (maxlevel>=0x80000007 && vendorNum==CPUV_AMD) {
                opts_cpuid(0x80000007,data);
                fi[4] = data[3];
             }
          }
       }
       if (cores>1) {
          str = new char[80];
          sprintf(str, "CPU cores  : %u (unit id %u)", cores, apicid);
          dl->insert(str);
       }
       PUSH_EMPTY();

       TRect rr(CpuInfoList->getBounds());
       int list_wdt = rr.b.x-rr.a.x;

       //fi1 = fi2 = fi4 = 0xFFFFFFFF;

       str = new char[list_wdt+1];
       strcpy(str, "Available  : ");
       if (fi[1-1]&CPUID_FI1_SSE42) PUSH_AVAIL("SSE4.2 "); else
       if (fi[1-1]&CPUID_FI1_SSE41) PUSH_AVAIL("SSE4.1 "); else
       if (fi[1-1]&CPUID_FI1_SSSE3) PUSH_AVAIL("SSSE3 "); else
       if (fi[1-1]&CPUID_FI1_SSE3) PUSH_AVAIL("SSE3 "); else
       if (fi[2-1]&CPUID_FI2_SSE) PUSH_AVAIL("SSE "); else
       if (fi[2-1]&CPUID_FI2_SSE2) PUSH_AVAIL("SSE2 ");
       if (fi[4-1]&CPUID_FI4_3DNOWEXT) PUSH_AVAIL("3DNow!+ "); else
       if (fi[4-1]&CPUID_FI4_3DNOW) PUSH_AVAIL("3DNow! ");
       if (fi[5-1]&CPUID_FI5_PSTATE) PUSH_AVAIL("P-States ");

       static u32t batch1[] = { 4,CPUID_FI4_IA64, 2,CPUID_FI2_MMX,
          3,CPUID_FI2_CMPXCHG8B, 1,CPUID_FI1_CMPXCHG16B, 2,CPUID_FI2_FXSR,
          1,CPUID_FI1_MOVBE, 1,CPUID_FI1_POPCNT, 1,CPUID_FI1_MONITOR,
          4,CPUID_FI4_SYSCALL, 2,CPUID_FI2_TSC,  4,CPUID_FI4_RDTSCP,
          2,CPUID_FI2_CLFSH, 2,CPUID_FI2_CMOV, 3,CPUID_FI3_LAHF64,
          2,CPUID_FI2_SEP, 1,CPUID_FI1_PCLMULQDQ, 1,CPUID_FI1_RDRAND, 
          1,CPUID_FI1_AVX, 1,CPUID_FI1_FMA, 1,CPUID_FI1_AESNI,
          1,CPUID_FI1_F16C, 0,0};
       static const char *bstr1[] = { "64bit ", "MMX ", "CMPXC8 ", "CMPXC16 ",
          "FXSR ", "MOVBE ", "POPCNT ", "MON ", "SYSC ", "RDTSC ", "RDTSCP ",
          "CLFLUSH ", "CMOV ", "LAHF ", "SYSENT ", "PCLMULQDQ ", "RDRAND ",
          "AVX ", "FMA ", "AESNI ", "F16C "};

       for (ii=0; batch1[ii*2]; ii++)
          if (fi[batch1[ii*2]-1]&batch1[ii*2+1]) PUSH_AVAIL(bstr1[ii]);

       if (fi[1-1]&CPUID_FI1_XSAVE) 
          PUSH_AVAIL(fi[1-1]&CPUID_FI1_OSXSAVE?"XSAVE- ":"XSAVE+ ");
       dl->insert(str);

       PUSH_EMPTY();

       str = new char[list_wdt+1];
       strcpy(str, "Features   : ");

       static u32t batch2[] = { 2,CPUID_FI2_HTT, 2,CPUID_FI2_VME, 
         2,CPUID_FI2_FPU, 1,CPUID_FI1_VMX, 2,CPUID_FI2_PAE, 2,CPUID_FI2_PSE,
         2,CPUID_FI2_PSE36, 4,CPUID_FI4_EXDBIT, 2,CPUID_FI2_PAT, 
         2,CPUID_FI2_PSN, 2,CPUID_FI2_PGE, 2,CPUID_FI2_MSR, 2,CPUID_FI2_ACPI, 
         1,CPUID_FI1_TM2, 2,CPUID_FI2_TM, 2,CPUID_FI2_APIC, 1,CPUID_FI1_x2APIC,
         2,CPUID_FI2_MTRR, 1,CPUID_FI1_EST, 2,CPUID_FI2_MCA, 2,CPUID_FI2_MCE,
         1,CPUID_FI1_DCA, 2,CPUID_FI2_DS, 1,CPUID_FI1_DTES64, 1,CPUID_FI1_DSCPL,
         2,CPUID_FI2_DE, 1,CPUID_FI1_PDCM, 2,CPUID_FI2_PBE, 1,CPUID_FI1_SMX,
         1,CPUID_FI1_CNXTID, 1,CPUID_FI1_xTPRUpdate, 2,CPUID_FI2_SSNOOP, 
         4,CPUID_FI4_1GBPAGES, 1,CPUID_FI1_TSC_D, 1,CPUID_FI1_PCID, 0,0};
       
       static const char *bstr2[] = { "HT ", "VME ", "FPU ", "VMX ", "PAE ", 
          "PSE ", "PSE36 ", "EXD ", "PAT ", "PSN ", "PGE ", "MSR ", "TM ", 
          "TM2 ", "TTC ", "APIC ", "x2APIC ", "MTRR ", "EST ", "MCA ", "MCE ", 
          "DCA ", "DS ", "DS64 ", "DSCPL ", "DE ", "PDCM ", "PBE ", "SMX ", 
          "CNXTID ", "xTPRUp ", "SSNOOP ", "1GbPAGES ", "TSC-D ", "PCID " };

       for (ii=0; batch2[ii*2]; ii++)
          if (fi[batch2[ii*2]-1]&batch2[ii*2+1]) PUSH_AVAIL(bstr2[ii]);
       dl->insert(str);

       u32t tempr = opts_getcputemp();
       if (tempr) {
          PUSH_EMPTY();
          str = new char[list_wdt+1];
          strcpy(str, "Temperature: ");
          char *cp = str + strlen(str);
          sprintf(cp, "%døC",tempr);
          dl->insert(str);
       }

       u32t flags, state, addrbits, 
            mtrregs = opts_mtrrquery(&flags,&state,&addrbits);

       static const char *mtstr[] = { "Uncacheable", "Write Combining", "", "",
          "Write-through", "Write-protected", "Writeback", ""};

       if (mtrregs) {
          PUSH_EMPTY();
          str = new char[list_wdt+1];
          strcpy(str, "Addr.width : ");
          char *cp = str + strlen(str);
          sprintf(cp, "%d bits",addrbits);
          dl->insert(str);

          str = new char[list_wdt+1];
          sprintf(str, "MTRR       : %d regs",mtrregs);
          if (flags&1) strcat(str, " + fixed");
          if (flags&4) strcat(str, " + SMRR");
          if (flags&2) strcat(str, ", WC supported");
          dl->insert(str);

          str = new char[list_wdt+1];
          strcpy(str, "MTRR state :");
          if (state&0x0800) {
             strcat(str, " enabled");
             if (state&0x0400) strcat(str, " + fixed");
          }
          if ((state&0x007F)<7) {
             cp = str + strlen(str);
             if (cp[-1]!=':') *cp++ = ',';
             sprintf(cp, " default mode is %s", mtstr[state&0x007F]);
          }
          dl->insert(str);
       }
       u32t acpi = opts_acpiroot();
       if (acpi) {
          PUSH_EMPTY();
          str = new char[64];
          sprintf(str, "ACPI root  : %08X", acpi);
          dl->insert(str);
       }
   }
   u32t memsize = opts_getpcmem();
   sprintf(idstr,"%d Mb",memsize);
   replace_coltxt(&CpuIdMemText, idstr);

   CpuInfoList->newList(dl);
   execView(dlg);
   destroy(dlg);
   destroy(dl);
}

