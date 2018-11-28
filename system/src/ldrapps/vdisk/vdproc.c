#include "qsbase.h"
#include "doshlp.h"
#include "stdlib.h"
#include "qcl/qslist.h"
#include "vdproc.h"
#include "qcl/sys/qsedinfo.h"
#include "qsint.h"
#include "dskinfo.h"

#define MAP_SIZE                  (_8MB)
#define PAGE_SIZE                 (_2MB)
#define PAGESIN4GB            (0x100000)   // # of pages in 4Gb
// read size, which SHOULD fit into mapped MAP_SIZE area, aligned to PAGE_SIZE
#define READ_IN_TIME              ((MAP_SIZE-PAGE_SIZE)/512 - 8)

static HD4_Header   *cdh = 0;   ///< active disk header
static u64t      cdhphys = 0;   ///< disk header physical address
static u32t         chdd = 0;   ///< QS disk handle
static u32t      cdhsize = 0;   ///< disk size (in sectors)
static void    *reserved = 0;   ///< reserved space in qsinit memory
static HD4_TabEntry *cde = 0;
static u32t     cdecount = 0;
static char     *maparea = 0;
static u64t     mapstart = 0;
static u64t    min4Gaddr = 0;   ///< lowest high memory address
static u32t    min4Gsize = 0;   ///< size of lowest high memory block (in pages)
static u32t     endofram = 0;
static u32t  classid_ext = 0;   ///< external info "qs_extdisk" compatible class
static qs_extdisk  eiptr = 0;
u32t          classid_vd = 0;   ///< qs_vdisk class id

static u32t io(u32t disk, u64t sector, u32t count, void *data, int write) {
   u32t ii, svcount;
   if (!cdh) return 0;
   // overflow check
   if (sector>=(u64t)cdhsize) return 0;
   if (sector+count>(u64t)cdhsize) count = cdhsize-sector;

   if (!endofram) endofram = sys_endofram();

   ii = 0;
   svcount = count;

   while (count && ii<cdecount) {
      if (cde[ii].hde_sectors>sector) {
         u32t toread = cde[ii].hde_sectors - sector, cdeofs = sector;
         if (toread>count) toread = count;
         // loop by 4Mb temp mapping
         while (toread) {
            u32t  cnt = toread>READ_IN_TIME ? READ_IN_TIME : toread,
                bytes = cnt * 512,
                  ofs = (cdeofs & 7) * 512;
            u64t addr = (u64t)cde[ii].hde_1stpage + (cdeofs>>3) << PAGESHIFT,
                eaddr = addr + bytes + ofs;

            char *ptr =  0;

            /* do not unmap areas below end of ram at all (because it was not
               mapped actually), for any other (must be only above 4Gb areas)
               use 8Mb map space */
            if (eaddr<=(u64t)endofram)
               ptr = (char*)pag_physmap(addr, bytes + ofs, 0);
            else {
               // unmap not matched 8Mb area
               if (maparea)
                  if (mapstart<=addr && eaddr>mapstart && eaddr<=mapstart+MAP_SIZE) ; else {
                     pag_physunmap(maparea);
                     maparea = 0;
                  }
               /* map 8Mb, aligned down to big page phys addr to allow page allocator
                  make 4 big pages for us (much faster) */
               if (!maparea) {
                  mapstart = addr & ~(u64t)(PAGE_SIZE-1);
                  maparea = pag_physmap(mapstart, MAP_SIZE, 0);
               }
               if (maparea) ptr = maparea + (addr - mapstart);
            }

            if (ptr)
               if (write) memcpy(ptr + ofs, data, bytes);
                  else memcpy(data, ptr + ofs, bytes);

            if (!ptr) return svcount - count;

            data    = (char*)data + bytes;
            toread -= cnt;
            count  -= cnt;
            cdeofs += cnt;
         }
         sector = 0;
      } else
         sector-=cde[ii].hde_sectors;
      ii++;
   }
   return svcount;
}

static u32t _std _read(u32t disk, u64t sector, u32t count, void *data) {
   return io(disk, sector, count, data, 0);
}

static u32t _std _write(u32t disk, u64t sector, u32t count, void *data) {
   return io(disk, sector, count, data, 1);
}

static void fill_header(u32t pages) {
   if (cdh) {
      cdhsize = pages<<PAGESHIFT-9;

      memset(cdh, 0, PAGESIZE);
      cdh->h4_sign     = EXPDATA_SIGN;
      cdh->h4_version  = 1;

      cdh->h4_cyls     = 1024;
      cdh->h4_spt      = 17;
      cdh->h4_heads    = 31;

      while (cdh->h4_cyls*cdh->h4_spt*cdh->h4_heads < cdhsize) {
         if (cdh->h4_spt<63) {
            cdh->h4_spt   = (cdh->h4_spt   + 1 & 0xF0) * 2 - 1; continue;
         }
         if (cdh->h4_heads<255) {
            cdh->h4_heads = (cdh->h4_heads + 1 & 0xF0) * 2 - 1; continue;
         }
         break;
      }
      cdh->h4_cyls     = cdhsize / (cdh->h4_spt*cdh->h4_heads);

      cdh->h4_pages    = pages;
      cdh->h4_tabofs   = sizeof(HD4_Header);
      cdh->h4_tabsize  = 1;

      cde      = (HD4_TabEntry*)(cdh+1);
      cdecount = 0;
   }
}

static void wipe_fat32bs(u8t vol) {
   disk_volume_data vi;
   if (hlp_volinfo(vol, &vi)==FST_FAT32) dsk_emptysector(vi.Disk, vi.StartSector, 1);
}

static void set_lvmname(u32t index, str_list* vol_names, u32t *pl_rc) {
   if (vol_names)
      if (vol_names->count>index && vol_names->item[index][0])
         *pl_rc = lvm_setname(chdd, index, LVMN_PARTITION|LVMN_VOLUME,
            vol_names->item[index]);
}

static u32t make_partition(u32t index, u32t flags, char letter, u32t *pl_rc) {
   u32t rc = 0;
   // and format at last
   if ((flags&VFDF_NOFMT)==0) {
      u8t    vol = 0;
      u32t usize = 0;
      // mount & format
      rc = vol_mount(&vol, chdd, index);
      // "always-FAT32"
      if (!rc && (flags&VFDF_FAT32)!=0) {
         disk_volume_data vi;
         u32t          csize = _64KB;
         hlp_volinfo(vol, &vi);
         // making too big for FAT16 number of clusters
         if (vi.TotalSectors) {
            while (vi.TotalSectors * (u64t)vi.SectorSize / csize <= _64KB)
               if (csize > vi.SectorSize) csize>>=1; else break;
            if (vi.TotalSectors * (u64t)vi.SectorSize / csize > _64KB)
               usize = csize;
         }
      }
      if (!rc) rc = vol_formatfs(vol, flags&VFDF_HPFS?"HPFS":"FAT",
          DFMT_ONEFAT|DFMT_QUICK, usize, 0);
      // un-format FAT32 formatted partition
      if (!rc && (flags&VFDF_NOFAT32)!=0) wipe_fat32bs(vol);

   }
   // set LVM drive letter
   if (*pl_rc==0 && letter && (isalpha(letter) || letter=='*'))
      *pl_rc = lvm_assignletter(chdd, index, toupper(letter), 1);
   return rc;
}

/** read/update PC memory list.
    returns list qwords in form:
       start_page<<32 | num_of_pages | (0x80000000 if memory is used by QSINIT) */
static dq_list get_memory_list(u32t *lopages, u32t *hipages) {
   pcmem_entry  *me, *pe;
   dq_list      mem;
   mem      = NEW(dq_list);
   me       = sys_getpcmem(0);
   *lopages = 0;
   *hipages = 0;
   if (me) {
      pe = me;
      while (pe->pages) {
         u32t owner = pe->flags&PCMEM_TYPEMASK;
         if (pe->start>=_4GBLL && pe->pages>=_4MB>>PAGESHIFT && owner==PCMEM_FREE) {
            // address fit to 44 bits?
            if (pe->start>>PAGESHIFT<=(u64t)FFFF) {
               mem->add((pe->start>>PAGESHIFT)<<32|pe->pages);
               *hipages += pe->pages;
               // find lowest page
               if (min4Gaddr && pe->start<min4Gaddr || !min4Gaddr) {
                  min4Gaddr = pe->start;
                  min4Gsize = pe->pages;
               }
            }
         }
         if (pe->start<_4GBLL && pe->pages>=_1MB>>PAGESHIFT && (owner==PCMEM_FREE
            || owner==PCMEM_QSINIT))
         {
            u32t pages = pe->pages;
            *lopages  += pages;
            // mark qsinit pages
            if (owner==PCMEM_QSINIT) pages|=0x80000000;
            mem->add((pe->start>>PAGESHIFT)<<32|pages);
         }
         pe++;
      }
      free(me);
   }
   return mem;
}

static qserr check_pae(void) {
   qserr rc = 0;
   if (!sys_pagemode()) {
      rc = pag_enable();
      log_printf("set PAE (%d)\n", rc);
      if (rc) return rc;
   }
   return sys_pagemode()?0:E_SYS_UNSUPPORTED;
}

/** check for disk presence at the start of 4th Gb.
    @param [out] pdh     returning disk header (on success, mapped)
    @retval 0                  success, returns mapped disk header (must be released in caller)
    @retval E_SYS_NONINITOBJ   no disk
    @retval E_SYS_NOMEM        no memory above 4Gb
    @retval E_SYS_UNSUPPORTED  no PAE
    @retval E_SYS_BADFMT       disk present, but uses low pages  */
static qserr vdisk_exists(HD4_Header **pdh) {
   u32t  rc;
   if (!min4Gaddr) return E_SYS_NOMEM;
   // unable to set mode?
   rc = check_pae();
   if (!rc) {
      HD4_Header *dh = (HD4_Header*)pag_physmap(min4Gaddr, PAGESIZE, 0);
      if (dh) {
         if (dh->h4_sign==EXPDATA_SIGN && dh->h4_version==1) {
            HD4_TabEntry *tab = (HD4_TabEntry*)(dh+1);
            u32t ii;
            // enum page ranges
            for (ii=0; ii<dh->h4_tabsize; ii++)
               if (tab[ii].hde_1stpage<PAGESIN4GB) {
                  log_printf("Disk found, but uses low memory!\n");
                  rc = E_SYS_BADFMT;
                  break;
               }
            if (!rc && pdh) *pdh = dh;
         } else
            rc = E_SYS_NONINITOBJ;
         // release it if nobody receives out ptr
         if (rc || !pdh) pag_physunmap(dh);
      }
   }
   return rc;
}


qserr _exicc vdisk_init(EXI_DATA, u32t minsize, u32t maxsize, u32t flags,
                       u32t divpos, char letter1, char letter2, u32t *disk)
{
   qserr         rc;
   u32t          ii, 
         persist_ok = 0;
   dq_list      mem;
   u32t     lopages, hipages;
   // disk was created?
   if (cdh) return E_SYS_INITED;
   // invalid flags
   if ((flags&(VFDF_NOHIGH|VFDF_NOLOW))==(VFDF_NOHIGH|VFDF_NOLOW) ||
       (flags&(VFDF_EMPTY|VFDF_SPLIT))==(VFDF_EMPTY|VFDF_SPLIT) ||
       (flags&(VFDF_FAT32|VFDF_NOFAT32))==(VFDF_FAT32|VFDF_NOFAT32) ||
       (flags&(VFDF_FAT32|VFDF_HPFS))==(VFDF_FAT32|VFDF_HPFS) ||
       (flags&VFDF_EXACTSIZE)!=0 && !minsize ||
       (flags&VFDF_SPLIT)!=0 && !divpos ||
       (flags&VFDF_EMPTY)!=0 && divpos ||
       (flags&VFDF_PERCENT)!=0 && divpos>=100) return E_SYS_INVPARM;
   // allow NOFAT32 with HPFS but clears it to not confuse later code
   if (flags&VFDF_HPFS) flags&=~VFDF_NOFAT32;

   mem = get_memory_list(&lopages, &hipages);

   if (flags&VFDF_NOHIGH) hipages = 0;
   if (flags&VFDF_NOLOW)  lopages = 0;
   // limit usage memory above 4Gb
   if (hipages && maxsize) {
      maxsize <<= 20 - PAGESHIFT;
      if (hipages>maxsize) hipages = maxsize;
   }
   minsize <<= 20 - PAGESHIFT;
   // adjust size before persistence check
   if ((flags&VFDF_EXACTSIZE) && hipages>minsize) hipages = minsize;

   if ((flags&VFDF_PERSIST) && hipages) {
      HD4_Header *edh;
      // fill it back from the header, but only on size match!
      if (vdisk_exists(&edh)==0)
         if (edh->h4_pages+1==hipages && hipages>=minsize) {
            cdh        = edh;
            cdhphys    = min4Gaddr;
            cdhsize    = edh->h4_pages<<PAGESHIFT-9;
            cde        = (HD4_TabEntry*)(edh+1);
            cdecount   = edh->h4_tabsize;
            persist_ok = 1;
            rc = 0;
         }
   }
   if (!cdh)
   do {
      rc = 0;
      // minimal size specified, check memory below 4Gb
      if (minsize) {
         if (hipages<minsize) {
            if (hipages+lopages<minsize) {
               log_printf("min %d pages, but avail: lo %d, high %d\n",
                  minsize, lopages, hipages);
               rc = E_SYS_NOMEM;
               break;
            }
            lopages = minsize - hipages;
         } else {
            lopages = 0;
            if (flags&VFDF_EXACTSIZE) hipages = minsize;
         }
      } else lopages = 0;

      if (hipages+lopages==0) {
         log_printf("no high memory?\n");
         rc = E_SYS_NOMEM;
         break;
      }
      // check split border
      if (divpos) {
         if ((flags&VFDF_PERCENT)==0) divpos<<=20-PAGESHIFT; else
            divpos = (u64t)(hipages+lopages) * divpos / 100;
         if (divpos>=hipages+lopages) divpos = 0;
         if (divpos) log_printf("split on %d Mb\n", divpos>>20-PAGESHIFT);
      }
      // turn on PAE paging - if high pages used
      if (hipages) {
         rc = check_pae();
         if (rc) break;

         // walk over high memory
         for (ii=0; ii<mem->count(); ii++) {
            u64t  addr = mem->value(ii);
            u32t pages = addr;
            addr = addr>>32<<PAGESHIFT;
            if (addr>=_4GBLL && addr<_2TBLL) {
               u32t res;
               // address >> 9 must fit to dword (i.e. max 2Tb of memory ;))
               if (addr+((u64t)pages<<PAGESHIFT)>_2TBLL)
                  pages = _2TBLL-addr >> PAGESHIFT;

               if (pages>hipages) pages = hipages;
               // mark memory as used by ram disk
               res = sys_markmem(addr, pages, PCMEM_RAMDISK);
               if (res)
                  log_printf("warning! mark failed, addr %LX - %d\n", addr, res);
               // map header on first block
               if (!cdh) {
                  cdh     = (HD4_Header*)pag_physmap(addr, PAGESIZE, 0);
                  cdhphys = addr;
                  if (!cdh) {
                     log_printf("Header map err (hi) - %LX\n", addr);
                     rc = E_SYS_SOFTFAULT;
                     break;
                  }
                  fill_header(--hipages + lopages);
                  addr += PAGESIZE;
                  pages--;
               }
               // add block to list in header
               cde[cdecount].hde_1stpage = (u32t)(addr>>PAGESHIFT);
               cde[cdecount].hde_sectors = pages<<3;
               hipages -= pages;
               cdecount++;
            }
            if (!hipages) break;
         }
         if (rc) break;
         // some high pages still unprocessed? strange...
         if (hipages) {
            log_printf("hipages rem: %d\n", hipages);
            rc = E_SYS_NOMEM;
            break;
         }
      }
      /* walk over low memory - in reverse order to make QSINIT block
         the last in process */
      if (lopages) {
         ii = mem->max();
         do {
            u64t  addr = mem->value(ii);
            int   used = addr& 0x80000000? 1 : 0; // qsinit block, need to reserve it!
            u32t pages = addr&~0x80000000;
            addr = addr>>32<<PAGESHIFT;
            if (addr<_4GBLL) {
               u32t res, reason;

               if (used) {
                  if (pages<=lopages) {
                     log_printf("QS block - %d, but need %d\n", pages, lopages);
                     break;
                  }
                  addr += pages - lopages << PAGESHIFT;
                  reserved = hlp_memreserve(addr, lopages<<PAGESHIFT, &reason);
                  if (!reserved) {
                     log_printf("Failed to reserve: %08X, %u pages, err %u\n",
                        (u32t)addr, lopages, reason);
                     break;
                  }
                  pages = lopages;
               } else
               if (pages>lopages) { // alloc from the end of block
                  addr += pages - lopages << PAGESHIFT;
                  pages = lopages;
               }
               /* mark memory as used by ram disk.
                  Here we hide memory from OS/2. For QSINIT block use PCMEM_QSINIT
                  owner - because sys_markmem will deny any other */
               res = sys_markmem(addr, pages, (used?PCMEM_QSINIT:PCMEM_RAMDISK)|PCMEM_HIDE);
               if (res)
                  log_printf("warning! mark failed, addr %LX - %d\n", addr, res);
               // map header on first block
               if (!cdh) {
                  // low addr must be mapped in any case (else we hang! :)
                  cdh     = (HD4_Header*)pag_physmap(addr, PAGESIZE, 0);
                  cdhphys = addr;
                  if (!cdh) {
                     log_printf("Header map err (lo) - %08X\n", (u32t)addr);
                     rc = E_SYS_SOFTFAULT;
                     break;
                  }
                  fill_header(--lopages);
                  addr += PAGESIZE;
                  pages--;
               }
               // add block to list in header
               cde[cdecount].hde_1stpage = (u32t)(addr>>PAGESHIFT);
               cde[cdecount].hde_sectors = pages<<3;
               lopages -= pages;
               cdecount++;
            }
            if (!lopages) break;
         } while (ii--);

         if (rc) break;
         // some low pages still unprocessed? strange...
         if (lopages) {
            log_printf("lopages rem: %d\n", lopages);
            rc = E_SYS_NOMEM;
            break;
         }
      }
   } while (0);

   DELETE(mem);

   if (!rc) {
      s32t           drvnum;
      struct qs_diskinfo dp;
      memset(&dp, 0, sizeof(dp));
      dp.qd_sectors     = cdhsize;
      dp.qd_cyls        = cdh->h4_cyls;
      dp.qd_heads       = cdh->h4_heads;
      dp.qd_spt         = cdh->h4_spt;
      dp.qd_extread     = _read;
      dp.qd_extwrite    = _write;
      dp.qd_sectorsize  = 512;
      dp.qd_sectorshift = 9;
      // if class is ok - create instance for our`s disk
      if (classid_ext) {
         eiptr = (qs_extdisk)exi_createid(classid_ext, EXIF_SHARED);
         dp.qd_extptr = eiptr;
      }
      // install new "hdd"
      drvnum = hlp_diskadd(&dp, HDDA_BIOSNUM);
      if (drvnum<0) {
         log_printf("hlp_diskadd err!\n");
         rc = E_DSK_MOUNTERR;
      } else {
         u32t  d_rc = 0, l_rc = 0;
         chdd = drvnum;
         // # of disk slices
         cdh->h4_tabsize = cdecount;

         log_it(3, "vdisk ranges: %d, bios fake num %02X\n", cdecount,
            hlp_diskbios(chdd,1));
         for (ii=0; ii<cdecount; ii++)
            log_it(3, "%2d. %010LX - %d pages\n", ii+1,
               (u64t)cde[ii].hde_1stpage<<PAGESHIFT, cde[ii].hde_sectors>>3);

         if (!persist_ok) {
            str_list *vol_names = 0;
            // allows custom LVM names in form {RAM/V}DISKNAME = pt1 name, pt2 name
            char       *vdnames = getenv("RAMDISKNAME");
            if (!vdnames) vdnames = getenv("VDISKNAME");
            // custom LVM volume names
            if (vdnames) vol_names = str_split(vdnames, ",");

            // wipe mbr sector
            dsk_newmbr(chdd, DSKBR_CLEARALL);
            // wipe LVM sector (to prevent of using previous disk data after "ramdisk delete")
            dsk_emptysector(chdd, cdh->h4_spt-1, 1);
            // init mbr & first lvm dlat
            d_rc = dsk_ptinit(chdd, 1);
            // create partition(s) and format it
            if (!d_rc) {
               // set disk LVM name
               l_rc = lvm_setname(chdd, 0, LVMN_DISK, "PAE_RAM_DISK");

               if ((flags&VFDF_EMPTY)==0) {
                  u64t  start, size;
                  // div pos in megabytes
                  divpos >>= 20-PAGESHIFT;
                  // create single partition
                  d_rc = dsk_ptalign(chdd, 0, divpos?divpos:100, DPAL_CHSSTART|DPAL_CHSEND|
                     (divpos?0:DPAL_PERCENT), &start, &size);
                  if (!d_rc) d_rc = dsk_ptcreate(chdd, start, size, DFBA_PRIMARY, 12);
                  // and make it active
                  if (!d_rc) d_rc = dsk_setactive(chdd, 0);
                  // format partition & assign LVM drive letter
                  if (!d_rc) d_rc = make_partition(0, flags, letter1, &l_rc);
                  // set custom (non-empty) volume name
                  if (vol_names) set_lvmname(0, vol_names, &l_rc);
                  /* second partition processing */
                  if (divpos) {
                     d_rc = dsk_ptalign(chdd, 0, 100, DPAL_CHSSTART|DPAL_CHSEND|DPAL_PERCENT,
                        &start, &size);
                     if (!d_rc) d_rc = dsk_ptcreate(chdd, start, size, DFBA_PRIMARY, 12);
                     // format partition & assign LVM drive letter
                     if (!d_rc) d_rc = make_partition(1, flags, letter2, &l_rc);
                     // set custom (non-empty) volume name
                     if (vol_names) set_lvmname(1, vol_names, &l_rc);
                  }
               }
            }
            log_printf("ram disk init: dmgr rc = %X, lvm rc = %X\n", d_rc, l_rc);
            if (vol_names) free(vol_names);
         } else {
            log_printf("ram disk init: persistent!\n");
            /* mount partition(s) to 1st available drive letter, but
               ignore result. This should help for ones, who assume it as C:
               in any case */
            if ((flags&VFDF_EMPTY)==0) {
               u8t vol = 0;
               vol_mount(&vol, chdd, 0);
               if (divpos) { vol = 0; vol_mount(&vol, chdd, 1); }
            }
         }

         if (disk) *disk = chdd;
         /* save header page number to storage key for "bootos2" to prevent
            static linking of this DLL to it (vdisk.dll uses partmgr, i.e.
            in case of static linking we will load both VDISK & PARTMGR on
            every boot) */
         {
            char dname[16];
            sto_savedword(STOKEY_VDPAGE, cdhphys>>PAGESHIFT);

            dsk_disktostr(chdd, dname);
            sto_save(STOKEY_VDNAME, dname, strlen(dname+1), 1);
         }
      }
   }
   if (rc) vdisk_free(0,0);
   return persist_ok && !rc ? E_SYS_EXIST : rc;
}

u32t _exicc vdisk_clean(EXI_DATA) {
   if (chdd) return E_SYS_INITED; else {
      qserr rc;
      // just makes list at least once, to setup min4Gsize
      if (!min4Gsize) {
         u32t  nlo, nhi;
         dq_list   meml = get_memory_list(&nlo, &nhi);
         DELETE(meml);
      }
      rc = vdisk_exists(0);

      if (!rc || rc==E_SYS_BADFMT) {
         u32t clean_size = min4Gsize<PAGE_SIZE>>PAGESHIFT ? min4Gsize : PAGE_SIZE;
         void *clean_ptr = pag_physmap(min4Gaddr, clean_size, 0);

         if (clean_ptr) {
            log_printf("ram disk clean: %LX %u\n", min4Gaddr, clean_size);
            memset(clean_ptr, 0, clean_size);
            pag_physunmap(clean_ptr);
            return 0;
         } else
            return E_SYS_SOFTFAULT;
      }
      return rc;
   }
}


u32t _exicc vdisk_free(EXI_DATA) {
   u32t     rc = 1;
   u32t     ii = 0;
   pcmem_entry *me;
   // remove installed "HDD"
   if (chdd) {
      /* wipe MBR and DLAT sectors else DMGR code can catch it in next
         disk with other size */
      dsk_newmbr(chdd,DSKBR_CLEARALL|DSKBR_LVMINFO);
      // unmount all volumes
      hlp_unmountall(chdd);
      // remove disk
      if (!hlp_diskremove(chdd)) rc = 0;
      chdd = 0;
      // delete disk name from storage
      sto_del(STOKEY_VDNAME);
   }
   // delete lost external info class instance
   if (eiptr) { DELETE(eiptr); eiptr = 0; }
   /* walk over system memory even if no disk present - to find blocks,
      was lost by broken sys_vdiskinit() call */
   me = sys_getpcmem(0);

   while (me[ii].pages) {
      if ((me[ii].flags&PCMEM_TYPEMASK)==PCMEM_RAMDISK)
         sys_unmarkmem(me[ii].start, me[ii].pages);
      else
      if (me[ii].flags==(PCMEM_QSINIT|PCMEM_HIDE))
         sys_markmem(me[ii].start, me[ii].pages, PCMEM_QSINIT);
      ii++;
   }
   free(me);
   // delete storage key with disk header phys page number
   sto_del(STOKEY_VDPAGE);
   // return reserved block to memory manager
   if (reserved) { hlp_memfree(reserved); reserved = 0; }
   // there is no disk header, return 0
   if (!cdh) return 0;
   // unmap high memory
   if (maparea) pag_physunmap(maparea);
   maparea  = 0;
   mapstart = 0;
   // unmap header
   pag_physunmap(cdh);
   cdh      = 0;

   cdhphys  = 0;
   cdhsize  = 0;
   return rc;
}

qserr _exicc vdisk_query(EXI_DATA, disk_geo_data *geo, u32t *disk,
                         u32t *sectors, u32t *seclo, u32t *physpage)
{
   qserr rc = cdh?0:E_SYS_NONINITOBJ;
   if (!rc) {
      if (disk)     *disk     = chdd;
      if (sectors)  *sectors  = cdhsize;
      if (physpage) *physpage = cdhphys>>PAGESHIFT;
      if (geo) {
         geo->Cylinders    = cdh->h4_cyls;
         geo->Heads        = cdh->h4_heads;
         geo->SectOnTrack  = cdh->h4_spt;
         geo->SectorSize   = 512;
         geo->TotalSectors = cdhsize;
      }
      if (seclo) {
         u32t ii;
         for (ii=0, *seclo=0; ii<cdecount; ii++)
            if (cde[ii].hde_1stpage < _4GBLL>>PAGESHIFT) *seclo+=cde[ii].hde_sectors;
      }
   } else {
      if (geo) memset(geo, 0, sizeof(disk_geo_data));
      if (sectors) *sectors  = 0;
      if (seclo) *seclo = 0;
   }
   return rc;
}

static qserr _exicc vdisk_setgeo(EXI_DATA, disk_geo_data *geo) {
   qserr rc = 0;
   if (!geo) rc = E_SYS_ZEROPTR; else
   if (!cdh) rc = E_SYS_NONINITOBJ; else
   if (geo->TotalSectors!=cdhsize || geo->SectorSize!=512) rc = E_SYS_INVPARM; else
   // check CHS (at least minimal)
   if (!geo->Heads || geo->Heads>255 || !geo->SectOnTrack ||
      geo->SectOnTrack>255 || !geo->Cylinders || (u64t)geo->Cylinders *
         geo->Heads * geo->SectOnTrack > geo->TotalSectors) rc = E_SYS_INVPARM;
   else {
      // copy CHS
      cdh->h4_cyls  = geo->Cylinders;
      cdh->h4_heads = geo->Heads;
      cdh->h4_spt   = geo->SectOnTrack;
      // if disk is mounted - update CHS value in system info
      if (chdd) {
         struct qs_diskinfo *qdi = hlp_diskstruct(chdd, 0);
         // this WRONG! we patching it in system struct directly
         if (qdi) {
            qdi->qd_cyls  = geo->Cylinders;
            qdi->qd_heads = geo->Heads;
            qdi->qd_spt   = geo->SectOnTrack;
         }
      }
   }
   return rc;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// external info class
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

typedef struct {
   qs_vdisk       inst;
} doptdata;

static void _std dopt_init(void *instance, void *data) {
   doptdata *dp = (doptdata*)data;
   // use global class mutex as an easieast way of sync
   dp->inst = exi_createid(classid_vd, 0);
}

static void _std dopt_done(void *instance, void *data) {
   doptdata *dp = (doptdata*)data;
   exi_free(dp->inst);
   memset(dp, 0, sizeof(doptdata));
}

static qserr _exicc dopt_getgeo(EXI_DATA, disk_geo_data *geo) {
   doptdata *dp = (doptdata*)data;
   qserr rc = 0;
   if (!geo) rc = E_SYS_ZEROPTR; else
      if (!cdh) rc = E_SYS_NONINITOBJ; else
         rc = dp->inst->query(geo, 0, 0, 0, 0);
   return rc;
}

static qserr _exicc dopt_setgeo(EXI_DATA, disk_geo_data *geo) {
   doptdata *dp = (doptdata*)data;
   return dp->inst->setgeo(geo);
}

static char* _exicc dopt_getname(EXI_DATA) {
   u32t sectors, seclo;
   doptdata *dp = (doptdata*)data;
   qserr     rc = dp->inst->query(0, 0, &sectors, &seclo, 0);
   return rc?0:sprintf_dyn("PAE ram disk. %u low, %u high pages", seclo>>3, sectors-seclo>>3);
}

static u32t _exicc dopt_state(EXI_DATA, u32t state) {
   //doptdata *dp = (doptdata*)data;
   // set r/o is not supported, cache denied for this disk
   return EDSTATE_NOCACHE|EDSTATE_RAM;
}

static void *qs_vdisk_fn[] = { vdisk_init, vdisk_load, vdisk_free, vdisk_query, vdisk_setgeo, vdisk_clean };

static void *qs_extdisk_fn[] = { dopt_getgeo, dopt_setgeo, dopt_getname, dopt_state };

int unregister_class(void) {
   int err = 0;
   if (classid_vd)
      if (exi_unregister(classid_vd)) classid_vd = 0; else err++;
   if (classid_ext)
      if (exi_unregister(classid_ext)) classid_ext = 0; else err++;
   return err?0:1;
}

int register_class(void) {
   if (!classid_ext) classid_ext = exi_register("qs_vdisk_ext", qs_extdisk_fn,
      sizeof(qs_extdisk_fn)/sizeof(void*), sizeof(doptdata), 0, dopt_init, dopt_done, 0);

   if (!classid_vd) classid_vd = exi_register("qs_vdisk", qs_vdisk_fn,
      sizeof(qs_vdisk_fn)/sizeof(void*), 0, EXIC_GMUTEX, 0, 0, 0);

   if (!classid_ext || !classid_vd) { unregister_class(); return 0; }
   return 1;
}
