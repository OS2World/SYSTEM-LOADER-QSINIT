//
// QSINIT
// fill memory info
//
#include "qsint.h"
#include "qsutil.h"
#include "stdlib.h"
#include "qsstor.h"
#include "qslog.h"
#include "meminit.h"
#include "qspage.h"
#include "classes.hpp"
#include "qsmodext.h"
#include "internal.h"
#include "pagedesc.h"
#include "qssys.h"
#include "errno.h"

#define LASTPAGE   0xFFFFF000    // limit phys maps to this page

typedef u32t (_std *pfmemisfree)(u32t physaddr, u32t length, u32t reserve);

typedef struct {
   u64t      start;
   u64t        len;
   u32t       type;
} acpi_entry;

typedef struct {
   u64t      start;
   u64t        len;
   u32t      flags;     // for MEM_DIRECT only
   u32t      owner;
   u32t     res[2];
} mem_entry;

static physmem_block  *physmem = 0; // exported physmem array
u32t                  maxmap4g = 0; // top address to be mapped in 1st 4GBs

void setup_memlist(void);
void hlp_pgprint(void);

static u32t _std memisfree(u32t physaddr, u32t length, u32t reserve);

typedef List<mem_entry>  TSysMemList;
typedef List<vmem_entry> TSysPGAllocList;

IMPLEMENT_FAKE_COMPARATION(mem_entry)
IMPLEMENT_FAKE_COMPARATION(vmem_entry)
// can use static class, because List init don`t call to any heap allow
static TSysMemList       pcmem;
static TSysPGAllocList    memx;

// sort callback
static int __stdcall mem_addr_compare(const void *b1, const void *b2) {
   register mem_entry *m1 = (mem_entry*)b1,
                      *m2 = (mem_entry*)b2;
   if (m1->start > m2->start) return 1;
   if (m1->start == m2->start) return 0;
   return -1;
}

extern "C"
void setup_memory(void) {
   u32t  availmem, ii, acnt,
            zero = hlp_segtoflat(0),
         membase = sto_dword(STOKEY_BASEMEM) - zero;
   /* query range, used by common memory manager: membase..membase+availmem-1,
      hlp_memavail makes an additional check of memory structs */
   hlp_memavail(0, &availmem);
   // parse full E820 list, but copying data from disk buffer first
   AcpiMemInfo *rtbl = int15mem();
   acnt = 0;
   while (rtbl[acnt].LengthLow || rtbl[acnt].LengthHigh) acnt++;
   if (acnt) {
      AcpiMemInfo *tbl = new AcpiMemInfo[acnt];
      u32t        mcnt = 0;

      memcpy(tbl, rtbl, sizeof(AcpiMemInfo)*acnt);
      // allocate array (one for possible 4Gb border-cross and one for 1st Mb)
      pcmem.SetCount(acnt+2);
      // 640 kb
      pcmem[0].start = 0;
      pcmem[0].len   = (u32t)int12mem()<<10;
      pcmem[0].flags = MEM_DIRECT;
      pcmem[0].owner = PCMEM_RESERVED;
      mcnt = 1;
      // ACPI list
      for (ii=0; ii<acnt; ii++) {
         acpi_entry *ci = (acpi_entry*)(tbl + ii);
         mem_entry  &me = pcmem[mcnt];
         if (ci->start<_1MB) continue;   // below 1Mb

         if (ci->len>=_4KB) {
            me.start = ci->start;
            me.len   = ci->len;
            // align start address and length to page
            if (me.start & 0xFFF) {
               u64t diff = 0x1000 - (me.start & 0xFFF);
               me.start += diff;
               me.len   -= diff;
            }
            if (me.len & 0xFFF) me.len &= ~(u64t)0xFFF;
            if (!me.len) continue;

            me.flags = 0;
            me.owner = ci->type==1 ? PCMEM_FREE : PCMEM_HIDE|PCMEM_RESERVED;
            mcnt++;
            // split hypotetic block on the 4Gb border (never see the one)
            if (me.start<=FFFF && me.start+me.len>_4GBLL) {
               mem_entry &nme = pcmem[mcnt];
               pcmem[mcnt].start = _4GBLL;
               pcmem[mcnt].len   = nme.start + nme.len - _4GBLL;
               nme.len           = _4GBLL - nme.start;
               mcnt++;
            }
         }
      }
      delete tbl;
      pcmem.SetCount(mcnt);
   } else {
      // no E820, get physmem from initial parsing (here must be int15 88/E8)
      physmem_block *phm = (physmem_block*)sto_data(STOKEY_PHYSMEM);
      u32t     phm_count = sto_size(STOKEY_PHYSMEM)/sizeof(physmem_block);

      pcmem.SetCount(phm_count);
      for (ii=0; ii<phm_count; ii++) {
         pcmem[ii].start = phm[ii].startaddr;
         pcmem[ii].len   = phm[ii].blocklen;
         pcmem[ii].flags = 0;
         pcmem[ii].owner = PCMEM_FREE;
      }
   }
   // search for lowest border of usable memory in first 4Gbs
   maxmap4g = _1MB;
   for (ii = 0; ii<pcmem.Count(); ii++) {
      mem_entry  &me = pcmem[ii];
      if (me.start<_4GBLL && me.owner==PCMEM_FREE) {
         u32t eaddr = me.start + me.len;
         if (eaddr > maxmap4g) maxmap4g = eaddr;
      }
   }
   /* mark blocks:
      * 1st Mb            - DIRECT
      * QSINIT mem        - DIRECT + QSINIT
      * other free in 4Gb - DIRECT              */
   for (ii=0; ii<pcmem.Count(); ii++) {
      u64t start = pcmem[ii].start;
      if (start<_1MB) pcmem[ii].flags |= MEM_DIRECT;
         else
      if (start<=membase && start+pcmem[ii].len>=membase+availmem) {
         pcmem[ii].flags|= MEM_DIRECT;
         pcmem[ii].owner = PCMEM_QSINIT;
      } else
      if (start<maxmap4g && pcmem[ii].owner==PCMEM_FREE) 
         pcmem[ii].flags |= MEM_DIRECT;
   }
   // dump to log
   log_it(2, "phys top: %08X\n", maxmap4g);
   // sort by address
   qsort((void*)pcmem.Value(), pcmem.Count(), sizeof(mem_entry), mem_addr_compare);
   // update physmem to actual values
   hlp_setupmem(0, 0, 0, SETM_SPLIT16M);
   /* leave pcmem array for multiple hlp_setupmem() calls (it contain pure
      PC memory list) and make a copy of it for page allocator */
   setup_memlist();
   // print it
   hlp_memcprint();
}

void _std hlp_memcprint(void) {
   log_it(2,"<=====PC memory dump=====>\n");
   for (u32t ii=0; ii<pcmem.Count(); ii++) {
      u64t start = pcmem[ii].start,
             len = pcmem[ii].len;
      log_it(2, "%2d. %010LX-%010LX, size: %8u kb, flags %02X %04X\n", ii+1,
         start, start+len-1, (u32t)(len>>10), pcmem[ii].flags, pcmem[ii].owner);
   }
   // print page allocator table
   hlp_pgprint();
}

pcmem_entry* _std sys_getpcmem(int freeonly) {
   if (!pcmem.Count()) return 0;
   pcmem_entry *rc = (pcmem_entry*)malloc((pcmem.Count()+1)*sizeof(pcmem_entry));
   u32t ii=0, ti=0;
   for (; ii<pcmem.Count(); ii++) {
      u64t start = pcmem[ii].start;
      u32t flags = pcmem[ii].owner;

      if (start>=_1MB && (!freeonly || (flags&PCMEM_TYPEMASK)==PCMEM_FREE)) {
         rc[ti].start = start;
         rc[ti].pages = pcmem[ii].len>>PAGESHIFT;
         rc[ti].flags = flags;
         ti++;
      }
   }
   rc[ti].start = 0;
   rc[ti].pages = 0;
   return rc;
}

u32t _std sys_endofram(void) {
   return maxmap4g;
}

// insert block into the right place
static void insert_block_a(mem_entry &nb) {
   for (u32t ii=0; ii<pcmem.Count(); ii++)
      if (pcmem[ii].start>nb.start) {
         pcmem.Insert(ii,nb);
         return;
      }
   pcmem.Add(nb);
}

#define IS_INCLUDED()  (start<=address && epos>start && epos<=end)

#define IS_INCLUDE()   (start>=address && epos>start && epos>=end)

#define IS_INTERSECT() (start>address && start<epos && end>epos || \
                        start<address && end>address && end<epos)

// search blocks, interferenced with addr range
static u32t find_block(u64t address, u32t pages, u32t &bfirst, u32t &blast) {
   u32t   rc = 0, ii;
   u64t epos = address + ((u64t)pages<<PAGESHIFT) - 1;
   bfirst    = 0;
   blast     = 0;
   for (ii=0; ii<pcmem.Count(); ii++) {
      u64t start = pcmem[ii].start,
             end = pcmem[ii].start + pcmem[ii].len;
      // search for first..last touched blocks
      if (IS_INCLUDED() || IS_INCLUDE() || IS_INTERSECT()) {
         if (!bfirst) bfirst = ii;
         blast = ii;
         rc++;
      } else
      if (rc) break;
   }
   return rc;
}

static u32t _std sys_markprocess(u64t address, u32t pages, u32t owner) {
   u32t bf, bl, ii, cnt;
   if ((address&PAGEMASK) || !pages) return EINVAL;
   if ((owner & ~(PCMEM_HIDE|PCMEM_TYPEMASK))!=0) return EINVAL;

   cnt = find_block(address, pages, bf, bl);
   if (!cnt) return ENOENT;
#if 0
   hlp_memcprint();
   log_printf("cnt=%d, bf=%d, bl=%d\n", cnt, bf, bl);
#endif
   // check - all requested blocks have compatible type?
   for (ii=bf; ii<=bl; ii++) {
      u32t n_ow = pcmem[ii].owner&PCMEM_TYPEMASK,
         set_ow = owner&PCMEM_TYPEMASK;

      if (set_ow==PCMEM_FREE) {
         if (n_ow<PCMEM_RAMDISK) return EACCES;
      } else {
         // not a free block and not changing of PCMEM_HIDE flag
         if (!(set_ow==n_ow && (owner^PCMEM_HIDE)==pcmem[ii].owner) &&
            n_ow!=PCMEM_FREE) return EACCES;
      }
   }
   for (ii=bf; ii<=bl; ii++) {
      int  included = ii>bf && ii<bl,
             theone = ii==bf && ii==bl;
      u32t oneowner = pcmem[ii].owner;
      if (ii==bf)   // block can be splitted on start addr
         if (address>pcmem[ii].start) {
            mem_entry sb;
            sb.start = address;
            sb.len   = pcmem[ii].len - (address - pcmem[ii].start);
            sb.flags = pcmem[ii].flags;
            sb.owner = owner;

            pcmem[ii].len = address - pcmem[ii].start;
            insert_block_a(sb);
            bl++; ii++;
         } else
            included = 1;
      if (ii==bl) {   // block can be splitted at the end addr
         u64t bend = pcmem[ii].start+pcmem[ii].len,
             rbend = address+((u64t)pages<<PAGESHIFT);

         if (rbend<bend) {
            mem_entry sb;
            sb.start = rbend;
            sb.len   = bend - rbend;
            sb.flags = pcmem[ii].flags;
            sb.owner = theone ? oneowner : pcmem[ii].owner;

            pcmem[ii].len   = sb.start - pcmem[ii].start;
            pcmem[ii].owner = owner;
            insert_block_a(sb);
            included = 0;
         } else
         if (!included) included = !theone;
      }
      if (included) // block included into requested range
         pcmem[ii].owner = owner & (PCMEM_HIDE|PCMEM_TYPEMASK);
   }
   // merge blocks with the same owner/flags
   ii = 0;
   while (ii+1<pcmem.Count())
      if (pcmem[ii].owner==pcmem[ii+1].owner && pcmem[ii].start+pcmem[ii].len==
         pcmem[ii+1].start && pcmem[ii].flags==pcmem[ii+1].flags)
      {
         pcmem[ii].len+=pcmem[ii+1].len;
         pcmem.Delete(ii+1);
      } else ii++;
#if 1
   hlp_memcprint();
#endif
   return 0;
}

u32t _std sys_markmem(u64t address, u32t pages, u32t owner) {
   return sys_markprocess(address, pages, owner);
}

u32t _std sys_unmarkmem(u64t address, u32t pages) {
   return sys_markprocess(address, pages, PCMEM_FREE);
}

u32t _std hlp_setupmem(u32t fakemem, u32t *logsize, u32t *removemem, u32t flags) {
   u32t lphysaddr = 0, ii, jj, icnt = pcmem.Count(), ccount, csize,
         lactsize = logsize? *logsize : 0;
   // search for first block above 4Gb
   for (ii=0; ii<pcmem.Count(); ii++)
      if (pcmem[ii].start>=_4GBLL) { icnt = ii; break; }
   // update physmem pointer
   if (physmem) free(physmem);
   physmem = (physmem_block*)malloc(sizeof(physmem_block)*(icnt+1));
   register physmem_block *pm = physmem;
   // MEMLIMIT in 64k blocks
   if (fakemem) fakemem = (fakemem<16?0:fakemem-1) << 4;
   // fill physmem array
   for (ii=0, ccount=0, csize=0; ii<icnt; ii++) {
      // block, hidden from OS/2
      if ((pcmem[ii].owner&PCMEM_HIDE)!=0) continue;
      // remove manually specified blocks
      if (removemem) {
         for (jj=0; removemem[jj]; jj++)
            if (removemem[jj]==pcmem[ii].start) break;
         if (removemem[jj]) continue;
      }
      // skip block < one page dir entry
      if (pcmem[ii].len<_4MB) continue;

      u32t nbstart = pcmem[ii].start,
             nblen = pcmem[ii].len;
      // check hole size
      if (ccount) {
         physmem_block *cb = pm + ccount;
         u32t         pend = cb[-1].startaddr + cb[-1].blocklen;
         // hole on the same page dir entry - truncate both blocks
         if ((nbstart & PD4M_ADDRMASK) == (pend & PD4M_ADDRMASK)) {
            u32t  diff1 = pend & PD4M_MASK,
                  diff2 = PD4M_MASK + 1 - (nbstart & PD4M_MASK);

            if (diff1)
               if (diff1 < cb[-1].blocklen) cb[-1].blocklen -= diff1;
            if (diff2)
               if (diff2 >= nblen) continue; else {
                  nblen   -= diff2;
                  nbstart += diff2;
               }
         }
      }
      // copy block
      pm[ccount].startaddr = nbstart;
      pm[ccount].blocklen  = nblen;
      // MEMLIMIT: only blocks above 1st Mb have used in calc
      if (pm[ccount].startaddr >= _1MB) {
         u32t blocklen = pm[ccount].blocklen >> 16;
         // truncate block to fit MEMLIMIT
         if (fakemem) {
            if (csize + blocklen > fakemem) {
               blocklen = fakemem - csize;
               if (!blocklen) break;
               pm[ccount].blocklen = blocklen << 16;
               ii = icnt; // break cycle!
            }
         }
         // update total size in 64k blocks
         csize += blocklen;
      }
      // merge bounded blocks
      if (ccount && pm[ccount].startaddr==pm[ccount-1].startaddr+
         pm[ccount-1].blocklen)
      {
         pm[ccount-1].blocklen += pm[ccount].blocklen;
      } else
         ccount++;
   }
   // fix too large log size
   if (lactsize && lactsize + (15 << 4) > csize)
      lactsize = csize <= (15 << 4) ? 0 : csize - (15 << 4);
   // split block on 16M border (we always must fit to icnt+1)
   if (flags&SETM_SPLIT16M) {
      ii = 0;
      while (ii<ccount) {
         register physmem_block *ppm = pm+ii;
         if (ppm->startaddr<_16MB && ppm->startaddr+ppm->blocklen>_16MB) {
            memmove(ppm+1, ppm, (ccount-ii)*sizeof(physmem_block));
            ppm[1].startaddr = _16MB;
            ppm[1].blocklen  = ppm->startaddr + ppm->blocklen - _16MB;
            ppm->blocklen    = _16MB - ppm->startaddr;
            ccount++;
            break;
         } else ii++;
      }
   }
   // search for suitable place to place log buffer
   if (lactsize) {
      ii = ccount-1;
      while (ii > 0) { // we can skip 1st entry, because it contain 1Meg block
         physmem_block *pb = pm+ii--;
         u32t len = pb->blocklen >> 16;
         if (len >= lactsize) {
            pb->blocklen = len - lactsize << 16;
            lphysaddr = pb->startaddr + pb->blocklen;
            *logsize  = lactsize;
            break;
         }
      }
      if (!lphysaddr) *logsize = 0;
   }
   // update storage key
   sto_save(STOKEY_PHYSMEM, physmem, ccount * sizeof(physmem_block), 0);

   return lphysaddr;
}

// -----------------------------------------------------------------------
// -----------------------------------------------------------------------

void setup_memlist(void) {
   if (memx.Count()) return;
   memx.SetCount(pcmem.Count());
   // copy mem_entry into vmem_entry, pcmem sorted by address here
   u32t ii=0, ti=0;
   while (ii<pcmem.Count() && pcmem[ii].start<maxmap4g) {
      mem_entry  &me = pcmem[ii];
      // skip "reserved" BIOS entries
      if ((me.owner&PCMEM_TYPEMASK)!=PCMEM_RESERVED) {
         memx[ti].start = me.start;
         memx[ti].len   = me.len;
         memx[ti].flags = me.flags;
         memx[ti].usage = 1;
         memx[ti].paddr = 0;
         ti++;
      }
      ii++;
   }
   memx.SetCount(ti);
   memx[ti-1].flags |= MEM_PHYSEND;
}

/// find block index in memx array
static int pag_findidx(u32t physaddr, u32t len, u32t flags = 0) {
   if (!memx.Count()) return -1;
   // len with overflow?
   if (physaddr+len < physaddr) return -1;
   // searching block
   for (u32t ii=0; ii<memx.Count(); ii++)
      if (memx[ii].start<=physaddr && memx[ii].start+memx[ii].len>=
         physaddr+len) return ii;
   return -1;
}

#if 0
int pag_findblock(void *address, u32t flags, vmem_entry *rc) {
   int idx = pag_findidx(address, 1, flags);
   if (rc && idx>=0) *rc = memx[idx];
   return idx<0 ? 0 : 1;
}
#endif

/* searching for intersection with one or more existing map entries.
   We must unite it, else page dir creation code will be confused -
   it assume, what mapped entries, coming from non-paged mode not intersect
   each other.
   Check intersection with allocating block only, because NO intersection
   before allocation, so all of it we produce by new address. */
static void merge_nonpaged(u32t address, u32t size) {
   u32t icnt, ii;
   do {
      icnt = memx.Count();
      ii   = 0;
      while (ii<memx.Count()) {
         if ((memx[ii].flags&MEM_MAPPED)!=0) {
            u32t pa = memx[ii].start,
                 pe = pa + memx[ii].len,
               aend = address + size;
            // asked address include entire mapped entry
            if (pa>=address && pa<aend && pe<=aend) {
               memx[ii].usage++;
               memx[ii].start = address;
               memx[ii].len   = size;
               memx[ii].paddr = address;
               // exit inner cycle
               ii = FFFF; continue;
            } else
            // not included and not include and not equal - only intersection!
            if (pa>address && pa<aend && pe>aend ||
                pa<address && pe>address && pe<aend)
            {
               memx[ii].usage++;
               if (pa>address) memx[ii].start = address;
               if (aend>pe) memx[ii].len = aend - memx[ii].start;
               memx[ii].paddr = memx[ii].start;
               ii = FFFF; continue;
            }
         }
         ii++;
      }
   // repeat while inner cycle can remove entries
   } while (icnt > memx.Count());
}

// insert block into the right place
static void insert_block_b(vmem_entry &nb) {
   for (u32t ii=0; ii<memx.Count(); ii++)
      if (memx[ii].start>nb.start) {
         memx.Insert(ii,nb);
         return;
      }
   memx.Add(nb);
}

void* pag_physmap(u64t address, u32t size, u32t flags) {
   if (!in_pagemode) {
      if (address>(u64t)FFFF) return 0;
      if (address+size>(u64t)FFFF+1) return 0;
   }
   // align request to nearest page boundary
   u32t pgdiff = address & PAGEMASK;
   address -= pgdiff;
   size    += pgdiff;
   if (size&PAGEMASK) size = PAGEROUND(size); // size is 32bit
   // map on border of physical memory
   if (address<maxmap4g && address+size>maxmap4g) return 0;

   if (address<maxmap4g) {
      // do nothing, just return offset
      return (char*)hlp_segtoflat(0) + (u32t)address + pgdiff;
   } else {
      u32t  zero = hlp_segtoflat(0), ii;
      // called before structs init?
      if (!memx.Count()) return 0;
      /* in plane non-paged mode we MUST bound this request to partially
         intersected existing map (to avoid dupes), so catch any kind of
         such entries and merge it */
      if (!in_pagemode) merge_nonpaged(address, size);
      // search for mapped block (no diff here between paged/unpaged mode)
      for (ii=0; ii<memx.Count(); ii++)
         if ((memx[ii].flags&MEM_MAPPED)!=0)
            if (memx[ii].paddr<=address && memx[ii].paddr+memx[ii].len>=
               address+size)
            {
               memx[ii].usage++;
               return (char*)memx[ii].start + (u32t)(address-memx[ii].paddr) +
                  zero + pgdiff;
            }
      if (!in_pagemode) {
         // just create new map here, code above guarantee it to be unique
         vmem_entry  ne;
         ne.start = address;
         ne.len   = size;
         ne.flags = MEM_MAPPED;
         ne.usage = 1;
         ne.paddr = address;
         insert_block_b(ne);
         return (char*)zero + ne.start + pgdiff;
      } else {
         u32t p_end = maxmap4g,
              saddr = 0, slen = 0, once = 0;
         // catch only _start_ address
         int  big_aligned = (address&PD2M_MASK)==0 && size>=1<<PD2M_ADDRSHL ?1:0,
              pass;

         for (pass = 0; pass<1+big_aligned; pass++) {
            // dumb searching for smallest hole above maxmap4g
            for (ii=0; ii<=memx.Count(); ii++) {
               u32t cstart = ii==memx.Count() ? LASTPAGE : memx[ii].start;
               if (cstart >= p_end) {
                  // align block to 2Mb
                  if (big_aligned)
                     p_end = p_end + PD2M_MASK & ~PD2M_MASK;

                  if (p_end && cstart > p_end) {
                      u32t  len = cstart - p_end;

                      if (len >= size && (!slen || len<slen)) {
                         saddr = p_end;
                         slen  = len;
                      }
                  }
                  if (ii<memx.Count()) p_end = cstart + memx[ii].len;
                  once++;
               }
            }
            if (saddr)
               if (pag_mappages(saddr, size, address, MEM_MAPPED)) {
                  vmem_entry  ne;
                  ne.start = saddr;
                  ne.len   = size;
                  ne.flags = MEM_MAPPED;
                  ne.usage = 1;
                  ne.paddr = address;
                  insert_block_b(ne);
                  return (char*)zero + ne.start + pgdiff;
               }
            // drop "aligned" flag and try to use any hole
            big_aligned = 0;
         }
      }
   }
   return 0;
}

u32t pag_physunmap(void *address) {
   u32t zero = hlp_segtoflat(0),
        addr = (u32t)address - zero;
   // fake unmap of common memory
   if (addr<maxmap4g) return 1;
   // real unmap, with decrementing and address match
   if (!memx.Count()) return 0;
   int idx = pag_findidx(addr, 1);
   if (idx<0) return 0;

   if (memx[idx].usage<=1) {
      if ((memx[idx].flags&MEM_MAPPED)==0) return 0;
      // update page dir
      if (in_pagemode) pag_unmappages(memx[idx].start, memx[idx].len);
      // delete entry
      memx.Delete(idx);
   } else
      memx[idx].usage--;
   return 1;
}

void pag_inittables(void) {
   // map 1 Mb
   pag_mappages(0, _1MB, 0, MEM_DIRECT);
   // map other memory
   for (u32t ii=0; ii<memx.Count(); ii++) {
      u32t start = memx[ii].start,
             len = memx[ii].len,
              fl = memx[ii].flags;
      if (start>=_1MB)
         pag_mappages(start, len, fl&MEM_MAPPED ? memx[ii].paddr : start, fl);
   }
   // map page on FLAT:0 (QSINIT CS/DS segments, not physical 0) as read-only
   pag_mappages(-(long)hlp_segtoflat(0), PAGESIZE, -(long)hlp_segtoflat(0),
      MEM_DIRECT|MEM_READONLY);
}

void hlp_pgprint(void) {
   log_it(2,"<=====PG memory dump=====>\n");
   for (u32t ii=0; ii<memx.Count(); ii++) {
      u32t start = memx[ii].start,
             len = memx[ii].len,
              fl = memx[ii].flags;
      static char buf[128];
      int outlen = sprintf(buf, "%2d. %08X-%08X, size: %8u kb, flags %04X",
         ii+1, start, start+len-1, len>>10, memx[ii].flags);

      if (fl&MEM_MAPPED) outlen += sprintf(buf+outlen, ", cnt %d, phys %010LX",
         memx[ii].usage, memx[ii].paddr);
      strcpy(buf+outlen, "\n");

      log_it(2, buf);
   }
}
