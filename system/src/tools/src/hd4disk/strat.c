#include "strat.h"
#include "ldrparam.h"
#include "doshlp.h"
#include "iofmt.h"
#include "devhlp.h"
#include "seldesc.h"

#undef  LOG_IOREQ                  // annoying log about every i/o op
#undef  LOG_ERRORS

#define UNIT_HANDLE_VAL   0x3455   // unit handle for check - as memdisk do
#define PAGESIN4GB      0x100000   // # of pages in 4Gb
#define GDT_SELS_REQ           3   // # of required GDT selectors
#define MAX_LIN_USED          16   // # of VMAlloc blocks can be used
#define MAX_IO_USED           16   // # of ioctl i/o buffers (64k)
#define UNEXPECTED_ERROR   0xBAD   // internal error code
#define SIGNMASK      0x55AA55AA

// OS/4 & QSINIT imports (can be 0!)
struct LoaderInfoData     far *ldri = 0;   // loader info struct (OS/4 & QS)
struct ExportFixupData    far *expd = 0;   // doshlp internal data (QS only)
void (far *PrintLog)(char far*,...) = 0;   // loader log simple printf (OS/4 & QS)
DHPrnFn                     BPrintF = 0;

void init(InitPacket far *pkt);
void read_args(char far *cmdline);
void zero_header(void);
void far _cdecl _loadds iohandler(PIORB pIORB);
void (far *DevHelp)(void);

extern u16t CodeSegLen, DataSegLen, FlatDS, switch_codelen, cp_sel;
extern char far switch_handler[];      // pointer to 32bit code in 3rd driver object

static PUNITINFO       extuinfo = 0;   /* ptr to updated unit info (caller must
                                          not invalidate it!) */
u32t                    dskpage = 0;   // phiscal page number with disk info
u16t                  devhandle = 0;   // ADD handle
u16t                 lock_state = 0;   // IOCM_LOCK_MEDIA flag
static int       unit_allocated = 0;   // IOCM_ALLOCATE_UNIT flag
HD4_Header far           *diskh = 0;   // disk header (copy or original page)
u32t                     diskhp = 0;   // phys addr of data above
LIN                    linearDS = 0;   // driver`s DS linear address
u32t                     swcode = 0;   // switch code phys addr in 1st Mb
u32t                  swcodelin = 0;   // switch code linear addr
u32t                     paeppd = 0;   // page dirs for PAE mode (phys addr)
u32t                    pbuf64k = 0;   // buffer for non-aligned SGList ops
u8t                      is_pae = 0;   // low or high memory is used?
u8t                   is_native = 0;   // disk found at expd->HD4Page location
u8t                     verbose = 0;   // be verbose
u8t                  ioctl_only = 0;   // do not install ADD handler (process IOCTLs only)
u8t                      strat1 = 0;   // use strat1 calls
u8t               init_complete = 0;

u16t         sels[GDT_SELS_REQ];       // GDT selector for own use
LIN         linpf[MAX_LIN_USED];
u8t    lockhdl[MAX_LIN_USED*12];       // array for lock handles
u8t    ioctlhdl[MAX_IO_USED*12];       // array for ioctl i/o lock handles
u8t         ioctlf[MAX_IO_USED] = { 0 };
u16t                    linused = 0;   // # of VMAlloc-ed blocks in linpf
LIN                    ioctl_pl = 0;   // lin addr of 16 pages for ioctl i/o
u32t                  ioctl_ppl = 0;   // phys addr of the same buffer

u32t                hook_handle = 0;
volatile u16t      nest_counter = 0;
volatile u16t           locksfn = 0;

#define BPRN_LEN              64
static char   bprnbuf[BPRN_LEN];       // screen message buffer
static u16t             bprnpos = 0;   // message buffer position

extern volatile u32t  pIORBHead;
extern volatile u8t     dis_nmi;
void notify_hook(void);
void _stdcall put_IORB(IORBH far *);

u16t notify_inc(void);
#pragma aux notify_inc =        \
   "mov     ax,1"               \
   "lock  xadd nest_counter,ax" \
   value [ax];

u16t notify_dec(void);
#pragma aux notify_dec =        \
   "mov     ax,-1"              \
   "lock  xadd nest_counter,ax" \
   value [ax];

u16t get_pagebuf(u16t index);
#pragma aux get_pagebuf =       \
   "add     di,offset ioctlf"   \
   "xor     ax,ax"              \
   "mov     cl,1"               \
   "lock  cmpxchg [di],cl"      \
   value [ax]                   \
   parm [di]                    \
   modify exact [cx di];

u16t ioctl_lock(u16t sfn);
#pragma aux ioctl_lock =        \
   "xor     ax,ax"              \
   "lock  cmpxchg locksfn,cx"   \
   value [ax] parm [cx];

// adapter info  (ready to use)
static ADAPTERINFO ainfo = { "ram disk", 0, 1, AI_DEVBUS_OTHER|AI_DEVBUS_32BIT,
   AI_IOACCESS_MEMORY_MAP, AI_HOSTBUS_OTHER|AI_BUSWIDTH_64BIT, 0, 0, AF_16M,
   /* !!!MaxHWSGList!!! */ 16, 0,
   {
      0, 0, UF_NOSCSI_SUPT, 0, UNIT_HANDLE_VAL, 0, UIB_TYPE_DISK, 16, 0, 0
   }};

static MSGTABLE msgtab = { MSG_REPLACEMENT_STRING, 1, 0 };

// allocate x contig pages, lock it and optionally map to selector.
static u16t alloc_contig(u16t pages, u16t sel, u32t near *physaddr, LIN near *lin) {
   static PAGELIST  pla[16];         // up to 16 pages in 64k
   u32t      resbuf;
   LIN      linaddr;
   u32t        size = (u32t)pages<<PAGESHIFT, rc;
   u32t     rcpages = 0;

   if (linused==MAX_LIN_USED) return UNEXPECTED_ERROR;
   // page-aligned allocate
   rc = DevHelp_VMAlloc(VMDHA_RESERVE, size, 0, &linaddr, &resbuf);
   if (rc) return rc;
   linpf[linused++] = linaddr;
   // set as swappable else VMLock will return err
   rc = DevHelp_VMSetMem(linaddr, size, VMDHS_SWAP);
   if (rc) return rc;
   // lock to contig physical space (will fail if size>64k)
   rc = DevHelp_VMLock(VMDHL_CONTIGUOUS|VMDHL_WRITE|VMDHL_LONG, linaddr, size,
      linearDS + (u16t)&pla, linearDS + (u16t)&lockhdl[(linused-1)*12], &rcpages);
   if (rc) return rc;
   if (!rcpages) return UNEXPECTED_ERROR;
#ifdef DEBUG
   log_print("lin:%lx %x -> %lx\n",linaddr, size, pla[0].PhysAddr);
#endif
   if (lin) *lin = linaddr;
   // phys mem is contiguous, get start address only
   if (physaddr) *physaddr = pla[0].PhysAddr;
   // map selector if it was asked
   if (sel) rc = DevHelp_LinToGDTSelector(sel, linaddr, size);
   // error code
   return rc;
}

static void far __cdecl bprn_charout(char ch) {
   if (bprnpos>=BPRN_LEN-1) return;
   bprnbuf[bprnpos++] = ch;
   bprnbuf[bprnpos]   = 0;
}

static void vioprint(char *str) {
   if (verbose) {
      msgtab.MsgStrings[0] = (char far *)str;
      DevHelp_Save_Message(&msgtab);
   }
   if (PrintLog) {
      (*PrintLog)("HD4D: ");
      (*PrintLog)(str);
   }
}

void _loadds _stdcall strategy(ReqPacket far *pkt) {
   switch (pkt->Command) {
      case CMDInitBase:
         init((InitPacket far*)pkt);
         break;
      case CMDShutdown:
         // zero disk signature on shutdown
         if (((ShutdownPacket far*)pkt)->Func==1) {
            //zero_header();
            debug_str("shutdown end\n");
         }
         pkt->Status   = STDON;
         break;
      case CMDInitComplete:
         init_complete = 1;
         pkt->Status   = STDON;
         break;
      case CMDOpen:
         pkt->Status = STDON;
         break;
      case CMDClose: {
         OpenClosePacket far *ocpkt = (OpenClosePacket far*)pkt;
         // unlock "exclusive" ioctl mode
         if (ocpkt->sfn==locksfn) locksfn = 0;
         pkt->Status = STDON;
         break;
      }
      case CMDGenIOCTL: {
         IOCTLPacket far *iopkt = (IOCTLPacket far*)pkt;
         u8t func = iopkt->Function;

         if (iopkt->Category==IOCTL_CAT && func>=IOCTL_F_FIRST && func<=IOCTL_F_LAST) {
            ioctl(iopkt);
            break;
         }
      }
      default:
         pkt->Status   = STDON | ST_BADCMD;
   }
#ifdef LOG_IOREQ
   log_print("strategy(%x), unit %d->%x\n", pkt->Command, pkt->Unit, pkt->Status);
#endif
}

void init(InitPacket far *pkt) {
   InitArgs far *iap = (InitArgs far *)pkt->Pointer_2;
   u16t      ipktsel = SELECTOROF(iap), rc;
   u32t      ldrsign;
   // make pointer to DOSHLP code (constant value for all OS/2 versions)
   u32t far  *doshlp = MAKEP (DOSHLP_CODESEL,0);
   // devhelp
   DevHelp = pkt->Pointer_1;
   ldrsign = *doshlp^SIGNMASK;
   // check loader signature at DOSHLP:0 seg
   if (ldrsign==(LDRINFO_MAGIC^SIGNMASK) || ldrsign==(OS4MAGIC^SIGNMASK)) {
      ldri = (struct LoaderInfoData far *)++doshlp;
      // check OS/4 loader info struct size
      if (ldri->StructSize>=FIELDOFFSET(struct LoaderInfoData,ConfigExt)) {
         SELECTOROF(PrintLog) = DOSHLP_CODESEL;
         OFFSETOF  (PrintLog) = ldri->PrintFOffset;
         // is this QSINIT/AOSLDR?
         expd = (struct ExportFixupData far*)((u8t far*)ldri + ldri->StructSize);
         /* there is no signature in QS prior to hd4disk, but we use it only
            for query our`s ram info, so no difference */
         if (expd->InfoSign==EXPDATA_SIGN) {
            u16t bfofs = get_bprint(expd);
            if (bfofs) {
               SELECTOROF(BPrintF) = DOSHLP_CODESEL;
               OFFSETOF  (BPrintF) = bfofs;
            }
            dskpage   = expd->HD4Page;
            if (dskpage) is_native = 1;
         }
      }
   }
   /* we can be loaded by IBM OS2LDR, OS/4 or old QSINIT, but from
      "new" QSINIT menu (create ram disk->load alt loader or
      create ram disk->load boot sector of another partition).
      So, check first page above 4Gb for our`s disk info to catch
      this case. */
   if (!dskpage) dskpage = PAGESIN4GB;

   log_print("check at %lx000\n",dskpage);

   // get own linear DS (need for VMLock)
   linearDS = DevHelp_GetDSLin();

   do {
      for (rc=0; rc<GDT_SELS_REQ; rc++) sels[rc] = 0;
      // allocate context hook
      rc = DevHelp_AllocateCtxHook(notify_hook, &hook_handle);
      if (rc) break;
      // allocate GDT selectors and try to get access to our`s disk header
      rc = DevHelp_AllocGDTSelector(sels, GDT_SELS_REQ);
      if (rc) break;
      // allocate buffer for code in 1st Mb
      rc = DevHelp_AllocPhys(PAGESIZE, MEMTYPE_BELOW_1M, &swcode);
      if (rc) break;
      rc = DevHelp_PhysToGDTSel(swcode, PAGESIZE, sels[1], GDTSEL_R0DATA);
      if (rc) break; else {
         SELDESCINFO di;
         rc = DevHelp_GetDescInfo(sels[1], &di);
         swcodelin = di.BaseAddr;
      }
      if (rc) break;
#ifdef DEBUG
      log_print("phys code %lx, sel %x\n", swcode, sels[1]);
      log_print("swh:%lx\n",&switch_handler);
#endif
      // copy code to 1Mb buffer
      memcpy(MAKEP(sels[1],0), &switch_handler, switch_codelen);
      /* update selector to be 32bit code by direct GDT editing.
        (to leave this page writable!) */
      selsetup(sels[1], swcodelin, PAGESIZE, D_CODE0, D_DBIG);
      // export selectors for asm code
      cp_sel = sels[1];
      is_pae = dskpage>=PAGESIN4GB;
      /* allocate 32k contig page aligned space:
         * 4k  - plain 32bit page dir for mode switching
         * 8k  - 2 page tables for mode switching
         * 16k - PAE page dirs
         * 4k  - PAE page table for 1st 2Mbs */
      rc = alloc_contig(is_pae?8:3, sels[2], &paeppd, 0);
      if (rc) break;
      // allocate 64k buffer
      rc = alloc_contig(16, 0, &pbuf64k, 0);
      if (rc) break;
      // buffers for ioctl i/o
      rc = alloc_contig(16, 0, &ioctl_ppl, &ioctl_pl);
      if (rc) break;
      // make switch page tables
      linmake(sels[2]);
      // make switch code gdt
      gdtmake(swcodelin, PAGESIZE-1);
      // make PAE page tables
      if (is_pae) paemake(sels[2]);
      // allocate one page and copy disk header to it
      rc = alloc_contig(1, sels[0], &diskhp, 0);
      if (rc) break;
      // read disk header
      rc = paecopy(diskhp, dskpage, 4, 0);
      if (rc) {
         log_print("no PAE (%d)\n",rc);
         rc = 1;
         break;
      }
      // far pointer to copy of disk header
      diskh = MAKEP(sels[0],0);
#ifdef DEBUG
      log_print("hdr copy: %lx (%lx)\n", diskhp, diskh);
#endif
      // read command line
      read_args((char far *)MAKEP(ipktsel,iap->Cmd_Line_Ofs));

      if (diskh->h4_sign==EXPDATA_SIGN && diskh->h4_version==1) {
         if (!is_native) {
            HD4_TabEntry far *tb = (HD4_TabEntry far *)(diskh+1);
            u16t ii;
            for (ii=0; ii<diskh->h4_tabsize; ii++)
               if (tb[ii].hde_1stpage<PAGESIN4GB) {
                  info_str("System loaded not by QSINIT who made this disk.\n");
                  info_str("Disk found, but contains low memory pages and cannot be used.\n");
                  rc = 1;
                  break;
               }
         }
         if (!rc) {
            /* add one page for the header, this should fix the messages
               like 511Mb and 1023Mb ;) */
            u32t sizemb = diskh->h4_pages+1>>(20-PAGESHIFT);
            // some logging and printing
            log_print("%ld Mb disk, %lx sectors\n", sizemb, diskh->h4_pages<<3);
            if (ioctl_only) log_str("IOCTL-only mode on\n"); else
            if (strat1) log_str("STRAT1 mode on\n");

            if (verbose) {
               if (BPrintF) {
                  bprnpos = 0;
                  (*BPrintF)(bprn_charout, "%ld Mb PAE ram %s installed.\n",
                     sizemb, (char far*)(ioctl_only?"storage":"disk"));
                  vioprint(bprnbuf);
               } else
                  // loader has no print with charout
                  vioprint(ioctl_only ? "PAE ram storage here.\n" :
                                        "PAE ram disk here.\n");
            }
            // inform switch code about disk location
            setdisk(dskpage, diskhp, pbuf64k);
         }
      } else {
         info_str("No ram disk.\n");
         rc = 1;
      }
   } while (0);
   // status
   if (rc) {
      pkt->Pointer_1 = 0;
      pkt->Status    = STDON | STERR | ERROR_I24_QUIET_INIT_FAIL;
      // print error
      if (rc>1) {
         log_print("devhlp err %x\n",rc);
         if (verbose) vioprint("HD4DISK init error.\n");
      }
      // free allocated selectors and memory
      for (rc=0; rc<GDT_SELS_REQ; rc++) DevHelp_FreeGDTSelector(sels[rc]);
      for (rc=0; rc<linused; rc++) DevHelp_VMFree(linpf[rc]);
      if (hook_handle) DevHelp_FreeCtxHook(hook_handle);
      if (swcode) DevHelp_FreePhys(swcode);
      // signal "we are here" in debug version
      debug_str("bye! may be next time!\n");
   } else {
      // register ADD
      if (!ioctl_only)
         DevHelp_RegisterDeviceClass(ainfo.AdapterName, iohandler, 0, 1, &devhandle);

      pkt->Pointer_1 = MAKEP(DataSegLen,CodeSegLen);
      pkt->Status    = STDON;
   }
   pkt->Data_1    = 0;
   pkt->Pointer_2 = 0;
}

static char toupper(char cc) {
   return ((cc)>0x60&&(cc)<=0x7A?(cc)-0x20:(cc));
}

void read_args(char far *cmdline) {
   char ch = *cmdline++;
   // read cmdline
   while (ch) {
      if (ch!=' ' && ch!='\t') {
         if (ch=='/') {
            ch = toupper(*cmdline++);
            if (!ch) break;

            switch (ch) {
               case 'V': // be verbose
                  verbose = 1;
                  break;
               case 'N': // disable NMIs
                  dis_nmi = 1;
                  break;
               case 'I': // ioctl i/o only
                  ioctl_only = 1;
                  break;
               case '1': // switch to STRAT1
                  strat1 = 1;
                  ainfo.MaxHWSGList = 1;
                  break;
               case 'H': // hide it (for GPT.FLT testing basically)
                  ainfo.UnitInfo[0].UnitFlags |= UF_NODASD_SUPT;
                  break;
            }
         }
      }
      ch = *cmdline++;
   }
}

/// common ioctl process
void ioctl(IOCTLPacket far *iopkt) {
   static u16t size[IOCTL_F_LAST - IOCTL_F_FIRST + 1] = { 4, 4, sizeof(RW_PKT),
      sizeof(RW_PKT), sizeof(GEO_PKT), sizeof(INFO_PKT) };

   u32t lflags = 0;
   u8t    func = iopkt->Function;
   u16t  va_rc = DevHelp_VerifyAccess(SELECTOROF(iopkt->DataPacket),
      size[func-IOCTL_F_FIRST], OFFSETOF(iopkt->DataPacket), VERIFY_READWRITE);

   if (va_rc) {
#ifdef LOG_IOREQ
      log_print("parm %lx,%d data %lx,%d\n", iopkt->ParmPacket, iopkt->ParmLen,
         iopkt->DataPacket, iopkt->DataLen);
      log_print("data packet %lx func:%d size:%d err:%x\n", iopkt->DataPacket,
         func, size[func-IOCTL_F_FIRST], va_rc);
#endif
      iopkt->Status = STDON | STERR | ERROR_I24_INVALID_PARAMETER;
      return;
   }

   switch (func) {
      case IOCTL_F_CHECK: {
         u32t far *pv = (u32t far *)iopkt->DataPacket;
         // if driver already locked - return wrong version
         if (*pv!=FFFF) *pv = diskh->h4_version; else
            *pv = ioctl_lock(iopkt->sfn) ? FFFF : diskh->h4_version;
         iopkt->Status = STDON;
         return;
      }
      case IOCTL_F_SIZE :
         *((u32t far *)iopkt->DataPacket) = diskh->h4_pages << 3;
         iopkt->Status = STDON;
         return;

      case IOCTL_F_WRITE:
         if (locksfn && locksfn!=iopkt->sfn) {
            RW_PKT far *rwp = (RW_PKT far *)iopkt->DataPacket;
            rwp->count      = 0;
            iopkt->Status   = STDON | STERR | ERROR_I24_WRITE_PROTECT;
            return;
         }
         lflags |= VMDHL_WRITE;
      case IOCTL_F_READ : {
         RW_PKT far *rwp = (RW_PKT far *)iopkt->DataPacket;
         u16t        idx = 0, bofs;
         u32t      count = rwp->count, pages, ccnt;
         LIN    lockaddr;

         if (!count || !rwp->lindata) {
            iopkt->Status = STDON | STERR | ERROR_I24_INVALID_PARAMETER;
            return;
         }
         // get free buffer for pagelist
         while (1) {
            if (!get_pagebuf(idx)) break;

            if (++idx==MAX_IO_USED) {
               // all 16 IOCTL pagelist buffers used?
               DevHelp_Yield();
               idx = 0;
            }
         }
         lockaddr = linearDS + (u16t)&ioctlhdl[idx*12];
         bofs     = idx<<PAGESHIFT;

         while (1) {
            u16t iorc;
            ccnt = count>511*8 ? 511*8 : count;

            // get page list for 2Mb only (511 entries to fit in one page buffer)
            if (DevHelp_VMLock(lflags, rwp->lindata, ccnt<<9,  ioctl_pl + bofs,
               lockaddr, &pages)) break;
            // call i/o
            iorc = paeio(ioctl_ppl + bofs, pages, rwp->sector, ccnt, lflags?1:0);
            // unlock pages
            DevHelp_VMUnLock(lockaddr);

            if (iorc) break;

            count -= ccnt;
            if (!count) break;
            // update pos
            rwp->lindata += ccnt<<9;
            rwp->sector  += ccnt;
         }
         rwp->count -= count;
         if (!count) iopkt->Status = STDON; else
            iopkt->Status = STDON | STERR | ERROR_I24_SECTOR_NOT_FOUND;
         // buffer is free
         ioctlf[idx] = 0;
         return;
      }

      case IOCTL_F_CHS  : {
         GEO_PKT far *gpk = (GEO_PKT far *)iopkt->DataPacket;
         gpk->cyls     = diskh->h4_cyls;
         gpk->heads    = diskh->h4_heads;
         gpk->spt      = diskh->h4_spt;
         gpk->active   = ioctl_only ? 0 : 1;

         iopkt->Status = STDON;
         return;
      }

      case IOCTL_F_INFO : {
         INFO_PKT far *ipkt = (INFO_PKT far *)iopkt->DataPacket;
         u16t cpe = ipkt->entries;
         // check access rights ;)
         if (locksfn && locksfn!=iopkt->sfn) {
            ipkt->entries = 0;
            iopkt->Status = STDON | STERR | ERROR_I24_WRITE_PROTECT;
            return;
         }
         if (cpe>diskh->h4_tabsize) cpe = diskh->h4_tabsize;
         // R3 userspace pointer must be in ipkt->loc, so make a simple conversion
         if (ipkt->loc && cpe) {
            HD4_TabEntry far *tab = (HD4_TabEntry far *)FLATTOP(ipkt->loc);
            u16t         copysize = cpe * sizeof(HD4_TabEntry);

            va_rc = DevHelp_VerifyAccess(SELECTOROF(tab), copysize, OFFSETOF(tab),
               VERIFY_READWRITE);
            if (va_rc) {
               ipkt->entries = 0;
               iopkt->Status = STDON | STERR | ERROR_I24_INVALID_PARAMETER;
               return;
            }
            memcpy(tab, diskh+1, copysize);
         }
         ipkt->entries = diskh->h4_tabsize;
         iopkt->Status = STDON;
         return;
      }
   }
   iopkt->Status = STDON | ST_BADCMD;
}

/* ADD command handler.
   Functionality just made the same as in memdisk. */
void far _loadds _cdecl iohandler(PIORB pIORB) {
   do {
      PIORB   next;
      u16t   ccode = pIORB->CommandCode,
             rcont = pIORB->RequestControl,
             error = 0;

      // wrong unit handle?
      if (ccode!=IOCC_CONFIGURATION && pIORB->UnitHandle!=UNIT_HANDLE_VAL) {
         error  = IOERR_CMD_SYNTAX;
      } else {
         u16t scode = pIORB->CommandModifier;
#ifdef LOG_IOREQ
         log_print("iohandler %x %x\n", ccode, scode);
#endif
         switch (ccode) {
            case IOCC_CONFIGURATION   :
               if (scode==IOCM_GET_DEVICE_TABLE) {
                  PIORB_CONFIGURATION cpIO = (PIORB_CONFIGURATION)pIORB;
                  PDEVICETABLE          dt = cpIO->pDeviceTable;
                  PADAPTERINFO        aptr = (PADAPTERINFO)(dt+1);

                  dt->ADDLevelMajor = ADD_LEVEL_MAJOR;
                  dt->ADDLevelMinor = ADD_LEVEL_MINOR;
                  dt->ADDHandle     = devhandle;
                  dt->TotalAdapters = 1;
                  dt->pAdapter[0]   = (ADAPTERINFO near *)aptr;
                  // copy adapter info
                  memcpy(aptr, &ainfo, sizeof(ADAPTERINFO));
                  // copy alt info from IOCM_CHANGE_UNITINFO
                  if (extuinfo) memcpy(&aptr->UnitInfo, extuinfo, sizeof(UNITINFO));
               } else
               if (scode!=IOCM_COMPLETE_INIT) error = IOERR_CMD_NOT_SUPPORTED;
               break;

            case IOCC_UNIT_CONTROL    :
               if (scode==IOCM_ALLOCATE_UNIT) {
                  if (!unit_allocated) unit_allocated = 1;
                     else error = IOERR_UNIT_ALLOCATED;
               } else
               if (scode==IOCM_DEALLOCATE_UNIT) {
                  if (unit_allocated) unit_allocated = 0;
                     else error = IOERR_UNIT_NOT_ALLOCATED;
               } else
               if (scode==IOCM_CHANGE_UNITINFO) {
                  PIORB_UNIT_CONTROL cpUI = (PIORB_UNIT_CONTROL)pIORB;

                  if (cpUI->UnitInfoLen==sizeof(UNITINFO)) extuinfo = cpUI->pUnitInfo;
                     else error = IOERR_CMD_SYNTAX;
               } else {
                  error = IOERR_CMD_NOT_SUPPORTED;
               }
               break;

            case IOCC_GEOMETRY        :
               if (scode==IOCM_GET_MEDIA_GEOMETRY || scode==IOCM_GET_DEVICE_GEOMETRY) {
                  PIORB_GEOMETRY cpDG = (PIORB_GEOMETRY)pIORB;
                  PGEOMETRY      pgeo = cpDG->pGeometry;

                  pgeo->TotalSectors    = diskh->h4_pages<<3; // pages*8
                  pgeo->BytesPerSector  = 512;
                  pgeo->Reserved        = 0;
                  pgeo->NumHeads        = diskh->h4_heads;
                  pgeo->TotalCylinders  = diskh->h4_cyls;
                  pgeo->SectorsPerTrack = diskh->h4_spt;
               } else
                  error = IOERR_CMD_NOT_SUPPORTED;
               break;

            case IOCC_EXECUTE_IO      :
               if (scode && scode<=IOCM_WRITE_VERIFY && scode!=IOCM_READ_PREFETCH) {
                  PIORB_EXECUTEIO cpIO = (PIORB_EXECUTEIO)pIORB;

                  if (rcont & IORB_CHS_ADDRESSING) {
                     error = IOERR_ADAPTER_REQ_NOT_SUPPORTED;
                     debug_str("chs!\n");
                  } else
                  if (cpIO->BlockSize==512) {
                     cpIO->BlocksXferred = 0;

                     if (cpIO->cSGList && cpIO->ppSGList && cpIO->BlockCount) {
                        u16t iorc;
#ifdef LOG_IOREQ
                        log_print("iohandler %lx %d\n", cpIO->RBA, cpIO->BlockCount);
#endif
                        iorc = paeio(cpIO->ppSGList, cpIO->cSGList, cpIO->RBA,
                           cpIO->BlockCount, scode>IOCM_READ_PREFETCH);

                        if (iorc) error = iorc; else
                           cpIO->BlocksXferred = cpIO->BlockCount;
                     } else
                     if (cpIO->cSGList) error = IOERR_CMD_SGLIST_BAD;
                  } else
                     error = IOERR_ADAPTER_REQ_NOT_SUPPORTED;
#ifdef LOG_ERRORS
                  if (error) {
                     u16t ii;
                     PSCATGATENTRY pt = cpIO->pSGList;
                     log_print("iohandler %lx %d -> rc %d\n", cpIO->RBA, cpIO->BlockCount, error);

                     for (ii=0; ii<cpIO->cSGList; ii++) {
                        log_print("sge: %d. %lx %lx\n", ii, pt[ii].ppXferBuf, pt[ii].XferBufLen);
                     }
                  }
#endif
               } else
                  error = IOERR_CMD_NOT_SUPPORTED;
               break;

            case IOCC_UNIT_STATUS     : {
               PIORB_UNIT_STATUS cpUS = (PIORB_UNIT_STATUS)pIORB;

               if (scode==IOCM_GET_UNIT_STATUS) cpUS->UnitStatus = US_READY;
                  else
               if (scode==IOCM_GET_LOCK_STATUS) cpUS->UnitStatus = lock_state;
                  else error = IOERR_CMD_NOT_SUPPORTED;
               break;
            }
            case IOCC_DEVICE_CONTROL  :
               if (scode==IOCM_LOCK_MEDIA) lock_state |= US_LOCKED;
                  else
               if (scode==IOCM_UNLOCK_MEDIA) lock_state &= ~US_LOCKED;
                  else error = scode!=IOCM_EJECT_MEDIA ? IOERR_CMD_NOT_SUPPORTED :
                     IOERR_DEVICE_REQ_NOT_SUPPORTED;
               break;

            default:
               error  = IOERR_CMD_NOT_SUPPORTED;
               break;
         }
      }
      pIORB->ErrorCode = error;
      pIORB->Status    = (error?IORB_ERROR:0)|IORB_DONE;

      next = rcont & IORB_CHAIN ? pIORB->pNxtIORB : 0;
#ifdef LOG_IOREQ
      log_print("iohandler done status=%x error=%x next=%lx\n", pIORB->Status, error, next);
#endif
      // call to completion callback?
      if (rcont & IORB_ASYNC_POST) {
         int direct_call = strat1 || !init_complete || ccode!=IOCC_EXECUTE_IO;
         /* a short story:
            * if we set MaxHWSGList in adapter info to >=16 - IORB_NotifyCall
              became recursive and reach stack limit!
              It designed to be async-called from interrupt, but we call it
              on caller stack. DASD just FROM IORB_NotifyCall call us again
              and again - until out of stack.

              UNITINFO.QueuingCount = 16 (max value) - can help on FAT only,
              HPFS reboots on any activity.

            * if we set MaxHWSGList to 1 - driver forced to use Strat1 calls.
              This is FINE and FAST and STABLE, but this stupid FAT32.IFS does
              not check for zero in pvpfsi->vpi_pDCS in FS_MOUNT - and goes to
              trap D. And JFS unable to operate with disk too.

            * if we Arm context hook on every call - we became blocked on
              init (DASD calls), because hook called in task-time only.

            * if we allow at least 2-3 recursive calls here - JFS traps system
              on huge load.

            So, Call it directly until the end of basedev loading, then use
            Arm hook. This is the only method, which makes happy all: FAT,
            FAT32, HPFS, HPFS386 and JFS.

            BUT - if any basedev will try to call us directly from own
            IOCM_COMPLETE_INIT - we will stop system here, I think ;)
         */
//         if (notify_inc()<1) direct_call = 1;
         if (!direct_call) put_IORB(pIORB); else IORB_NotifyCall(pIORB);
//         notify_dec();
      }
      // chained request?
      pIORB = next;
   } while (pIORB);

   if (pIORBHead) DevHelp_ArmCtxHook(0, hook_handle);
}
