#include "opt.h"
#include "stdlib.h"
#include "qslog.h"

#define _64MbLL   ((u64t)_1MB*64)
#define _128MbLL  ((u64t)_1MB*128)
#define _256MbLL  ((u64t)_1MB*256)
#define _512MbLL  ((u64t)_1MB*512)
#define _1GbLL    ((u64t)_1GB)
#define _2GbLL    ((u64t)_1GB*2)
#define _3GbLL    ((u64t)_1GB*3)
#define _4GbLL    ((u64t)_1GB*4)

mtrrentry *mtrr = 0;
int        regs = 0;

/// return <0 if not, else - register index
static int is_equal(u64t wc_addr, u64t wc_len, int start_index=0) {
   for (int ii=start_index; ii<regs; ii++) 
      if (mtrr[ii].on && mtrr[ii].start==wc_addr && mtrr[ii].len==wc_len)
         return ii;
   return -1;
}

/** is block completly included into active reg?
    @return <0 if not, else - register index */
static int is_included(u64t wc_addr, u64t wc_len, int start_index=0) {
   wc_len+=wc_addr;
   for (int ii=start_index; ii<regs; ii++) 
      if (mtrr[ii].on && mtrr[ii].start<=wc_addr && wc_len>mtrr[ii].start &&
         wc_len<=mtrr[ii].start+mtrr[ii].len) return ii;
   return -1;
}

/** is block completly include one of active reg?
    @return <0 if not, else - register index */
static int is_include(u64t wc_addr, u64t wc_len, int start_index=0) {
   wc_len+=wc_addr;
   for (int ii=start_index; ii<regs; ii++) 
      if (mtrr[ii].on && mtrr[ii].start>=wc_addr && wc_len>mtrr[ii].start &&
         wc_len>=mtrr[ii].start+mtrr[ii].len) return ii;
   return -1;
}

/** is block intersected with active reg?
    Not included and not include and not equal - only intersection!
    @return <0 if not, else - register index */
static int is_intersection(u64t wc_addr, u64t wc_len, int start_index=0) {
   wc_len+=wc_addr;
   for (int ii=start_index; ii<regs; ii++) 
      if (mtrr[ii].on) {
         u64t bepos = mtrr[ii].start+mtrr[ii].len;
         if (mtrr[ii].start>wc_addr && mtrr[ii].start<wc_len && bepos>wc_len ||
             mtrr[ii].start<wc_addr && bepos>wc_addr && bepos<wc_len)
                return ii;
      }
   return -1;
}

/// return 0/1
static int is_power2(u64t length) {
   if (length)
      if ((u64t)1<<bsf64(length)==length) return 1;
   return 0;
}

/// return number of set bits in the value
static int nbits(u64t value) {
   int cnt = 0;
   while (value) {
      cnt+=value&1;
      value>>=1;
   }
   return cnt;
}

/// clear register
static void clearreg(u32t reg) {
   mtrr[reg].start = 0; mtrr[reg].len = 0;  mtrr[reg].cache = MTRRF_UC;
   mtrr[reg].on = 0;
}

/// return 0/1
static int is_regavail(u32t *number) {
   int rc = 0;
   while (rc<regs)
      if (!mtrr[rc].on) {
         if (number) *number=rc;
         return 1;
      } else rc++;
   return 0;
}

/// number of available regs
static int regsavail(void) {
   int rc = 0, cnt = 0;
   while (rc<regs)
      if (!mtrr[rc++].on) cnt++;
   return cnt;
}

int mtrropt(u64t wc_addr, u64t wc_len, u32t *memlimit, int defWB) {
   u32t        reg;
   int          ii,
            sv4idx = 0;
   mtrrentry save4[16];
   memset(&save4,0,sizeof(save4));
   *memlimit = 0;

   if (is_included(wc_addr,wc_len)<0 && is_intersection(wc_addr, wc_len)<0 &&
      is_include(wc_addr,wc_len)<0 && is_regavail(&reg))
   {
      mtrr[reg].start = wc_addr;
      mtrr[reg].len   = wc_len;
      mtrr[reg].cache = MTRRF_WC;
      mtrr[reg].on    = 1;
      return 0;
   }
   // video memory is not in the 3-4th GB area
   if (wc_addr<_2GbLL || wc_addr+wc_len>_4GbLL) return OPTERR_VIDMEM3GB;
   /* turn off previous write combine on the same memory,
      but leave this block to catch low UC border successfully,
      also denies any included block other than UC (WT allowed
      only on exact match to the video memory block) */
   for (ii=0; ii>=0; ) {
      ii = is_include(wc_addr, wc_len, ii);
      if (ii>=0)
         if (mtrr[ii].cache==MTRRF_WC || mtrr[ii].cache==MTRRF_WT &&
            mtrr[ii].start==wc_addr && mtrr[ii].len==wc_len)
               mtrr[ii].cache=MTRRF_UC;
         else
         if (mtrr[ii].cache!=MTRRF_UC) return OPTERR_UNKCT; else ii++;
   }

   // only WB and UC/WT allowed in first 4Gb
   for (ii=0; ii<regs; ii++) {
      int ct = mtrr[ii].cache;
      if (mtrr[ii].on && ct!=MTRRF_UC && ct!=MTRRF_WB && ct!=MTRRF_WT &&
         mtrr[ii].start<_4GbLL) return OPTERR_UNKCT;
   }
   // is block intersected with someone?
   ii = is_intersection(wc_addr, wc_len);
   if (ii>=0) return OPTERR_INTERSECT;
   // remove/truncate all above 4Gb (but save it)
   for (ii=0; ii<regs; ii++)
      if (mtrr[ii].on)
         if (mtrr[ii].start<_4GbLL && mtrr[ii].start+mtrr[ii].len>_4GbLL) {
            u64t newlen = _4GbLL - mtrr[ii].start, 
                 remain = mtrr[ii].len - newlen;
            if (!is_power2(newlen)) return OPTERR_SPLIT4GB;
            mtrr[ii].len = newlen;
            // save block
            if (is_power2(remain)) {
               save4[sv4idx].start = _4GbLL;
               save4[sv4idx].len   = remain;
               save4[sv4idx].cache = mtrr[ii].cache;
               sv4idx++;
            } else
            if (is_power2(remain/3)) {
            }
         } else
         if (mtrr[ii].start>=_4GbLL || mtrr[ii].start+mtrr[ii].len>_4GbLL) {
            save4[sv4idx].start = mtrr[ii].start;
            save4[sv4idx].len   = mtrr[ii].len;
            save4[sv4idx].cache = mtrr[ii].cache;
            sv4idx++;
            clearreg(ii);
         }
   u64t  wbend = 0,
       ucstart = FFFF64;
   // searching for upper WB border
   if (defWB) wbend = _4GbLL;
      else
   for (ii=0; ii<regs; ii++)
      if (mtrr[ii].on)
         if (mtrr[ii].cache==MTRRF_WB)
            if (mtrr[ii].start+mtrr[ii].len > wbend)
               wbend = mtrr[ii].start+mtrr[ii].len;
   // searching for lower UC/WT border (but ignore small blocks)
   for (ii=0; ii<regs; ii++)
      if (mtrr[ii].on) {
         int ct = mtrr[ii].cache;
         if (ct==MTRRF_UC || ct==MTRRF_WT) {
            int pwr = bsf64(mtrr[ii].len);
            if (pwr>=27) {
               if (ucstart>mtrr[ii].start) ucstart = mtrr[ii].start;
               if (!defWB && ct==MTRRF_UC) clearreg(ii);
            }
         }
      }
   // if no UC entries - use the end of WB as a border
   if (ucstart>wbend) ucstart = wbend;
   // step #2 - removing small blocks above selected border
   if (!defWB)
      for (ii=0; ii<regs; ii++)
         if (mtrr[ii].on)
            if (mtrr[ii].cache==MTRRF_UC && ucstart<=mtrr[ii].start)
               clearreg(ii);
   // searching for duplicates
   for (ii=0; ii<regs; ) {
      if (mtrr[ii].on)
         if (mtrr[ii].cache==MTRRF_UC) {
            mtrr[ii].on = 0;
            int idx = is_include(mtrr[ii].start, mtrr[ii].len);
            mtrr[ii].on = 1;
            if (idx>=0 && mtrr[idx].cache==MTRRF_UC) {
               clearreg(idx);
               continue;
            }
         }
      ii++;
   }
   // this can occur on small video memory size (<128Mb)
   if (wc_addr<ucstart) return OPTERR_BELOWUC;
   // WB default type - find register with UC for video memory area
   if (defWB) {
      u64t  len[2] = {0,0};
      int      vmi = is_included(wc_addr,wc_len);

      if (vmi<0 || mtrr[vmi].cache==MTRRF_WB) return OPTERR_OPTERR;

      len[0] = wc_addr - mtrr[vmi].start,
      len[1] = mtrr[vmi].start + mtrr[vmi].len - (wc_addr + wc_len);
      // printf("%08LX %08LX\n", len[0], len[1]);

      /* we need 1 reg for WC (re-used vmi), ALL saved above 4Gb (because they
         can be UC only for the WB default) and a lot of regs for UC block(s)
         around video memory */
      if (regsavail() < sv4idx + nbits(len[0]) + nbits(len[1]))
         return OPTERR_OPTERR;
      for (ii=0; ii<2; ii++)
         if (len[ii]) {
            u64t  bs = ii ? wc_addr + wc_len : mtrr[vmi].start,
                  be = bs + len[ii];
            int  dir = bsf64(bs) > bsf64(be) ? -1 : 1,
               shift = bsf64(bs);
            //printf("%08X %08X %i %i\n", (u32t)bs, (u32t)be, dir, shift);

            // process from start position with inc/decreasing block size
            while (len[ii]) {
               u64t size = 1<<shift;
               if (size&len[ii]) {
                  if (!is_regavail(&reg)) return OPTERR_NOREG; //?
                  mtrr[reg].start = bs;
                  mtrr[reg].len   = size;
                  // UC or WT
                  mtrr[reg].cache = mtrr[vmi].cache;
                  mtrr[reg].on    = 1;
                  len[ii] -= size;
                  bs      += size;
               }
               shift += dir;
            }
         }
      // free splitted register
      mtrr[vmi].on = 0;
   } else
   // UC default type - build new WB list
   if (ucstart<wbend) {
      if (ucstart<_1GbLL) return OPTERR_LOWUC;

      for (ii=0; ii<regs; ii++)
         if (mtrr[ii].on)
            if (mtrr[ii].cache==MTRRF_WB) clearreg(ii);

      int regsfree = regsavail() - sv4idx - 1;
      log_it(2, "regs free: %i \n", regsfree);
      // force 3 registers (some memory above 4Gb can be lost)
      if (regsfree<3) regsfree = 3;

      // split memory to list
      u64t nextpos = 0;
      ii = 0;
      for (u64t size=_2GbLL; size>=_64MbLL; size>>=1) {
         if (ucstart>=size) {
            if (!is_regavail(&reg)) return OPTERR_NOREG;
            mtrr[reg].start = nextpos;
            nextpos += (mtrr[reg].len = size);
            mtrr[reg].cache = MTRRF_WB;
            mtrr[reg].on    = 1;
            ucstart -= size;
            // use only 3 mtrr regs
            if (++ii==regsfree) break;
         }
      }
      // save memlimit value
      *memlimit = nextpos>>20;
      /** and again removing small blocks above selected border...
         splitted blocks sum can be smaller than previously selected
         UC border and some blocks can be cleared here */
      for (ii=0; ii<regs; ii++)
         if (mtrr[ii].on)
            if (mtrr[ii].cache==MTRRF_UC && nextpos<=mtrr[ii].start)
               clearreg(ii);
   }
   // final check 
   if (is_included(wc_addr,wc_len)>=0 || is_intersection(wc_addr,wc_len)>=0 ||
      is_include(wc_addr,wc_len)>=0) return OPTERR_OPTERR;
   // add entry
   if (is_regavail(&reg)) {
      mtrr[reg].start = wc_addr;
      mtrr[reg].len   = wc_len;
      mtrr[reg].cache = MTRRF_WC;
      mtrr[reg].on    = 1;
   }
   // restore some of above 4Gb memory blocks
   if (sv4idx && regsavail()>0) {
      for (ii=0; ii<sv4idx; ii++) {
         if (!is_regavail(&reg)) break;
         mtrr[reg].start = save4[ii].start;
         mtrr[reg].len   = save4[ii].len;
         mtrr[reg].cache = save4[ii].cache;
         mtrr[reg].on    = 1;
      }
      /* check lost items for included UC entries.
         should never run this in WB default, because it fails if cannot
         reserve all sv4idx registers */
      while (ii<sv4idx) {
         if (mtrr[ii].cache==MTRRF_UC) {
            int idx = is_included(save4[ii].start,save4[ii].len);
            if (idx<0) idx = is_intersection(save4[ii].start,save4[ii].len);
            // check it multiple times (for intersection)
            if (idx>=0) { clearreg(idx); continue; }
         }
         ii++;
      }
   }
   return 0;
}
