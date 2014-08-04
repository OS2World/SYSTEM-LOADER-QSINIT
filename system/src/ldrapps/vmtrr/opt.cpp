#include "opt.h"
#include "stdlib.h"

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

int mtrropt(u64t wc_addr, u64t wc_len, u32t *memlimit) {
   u32t        reg;
   int          ii, 
            sv4idx = 0;
   mtrrentry save4[16];
   memset(&save4,0,sizeof(save4));
   *memlimit = 0;

   if (is_included(wc_addr,wc_len)<0 && is_intersection(wc_addr, wc_len)<0 &&
      is_regavail(&reg))
   {
      mtrr[reg].start = wc_addr;
      mtrr[reg].len   = wc_len;
      mtrr[reg].cache = MTRRF_WC;
      mtrr[reg].on    = 1;
      return 0;
   }
   // video memory in not in 4th GB
   if (wc_addr<_3GbLL || wc_addr+wc_len>_4GbLL) return OPTERR_VIDMEM3GB;
   /* turn off previous write combine on the same memory,
      but leave this block to catch low UC border successfully */
   ii = is_include(wc_addr, wc_len);
   if (ii>=0 && mtrr[ii].cache==MTRRF_WC) mtrr[ii].cache=MTRRF_UC;
   // only WB and UC allowed in first 4Gb
   for (ii=0; ii<regs; ii++) 
      if (mtrr[ii].on && mtrr[ii].cache!=MTRRF_UC && mtrr[ii].cache!=MTRRF_WB
         && mtrr[ii].start<_4GbLL) return OPTERR_UNKCT;
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
   for (ii=0; ii<regs; ii++)
      if (mtrr[ii].on)
         if (mtrr[ii].cache==MTRRF_WB)
            if (mtrr[ii].start+mtrr[ii].len > wbend)
               wbend = mtrr[ii].start+mtrr[ii].len;
   // searching for lower UC border (but ignore small blocks)
   for (ii=0; ii<regs; ii++)
      if (mtrr[ii].on)
         if (mtrr[ii].cache==MTRRF_UC) {
            int pwr = bsf64(mtrr[ii].len);
            if (pwr>=27) {
               if (ucstart>mtrr[ii].start) ucstart = mtrr[ii].start;
               clearreg(ii);
            }
         }
   // pass #2 - removing small blocks above selected border
   for (ii=0; ii<regs; ii++)
      if (mtrr[ii].on)
         if (mtrr[ii].cache==MTRRF_UC && ucstart<=mtrr[ii].start)
            clearreg(ii);
   // if no UC entries - use the end of WB as border
   if (ucstart>wbend) ucstart = wbend;
   // this can occur on small video memory size (<128Mb)
   if (wc_addr<ucstart) return OPTERR_BELOWUC;
   // build new WB list
   if (ucstart<wbend) {
      if (ucstart<_1GbLL) return OPTERR_LOWUC;

      for (ii=0; ii<regs; ii++)
         if (mtrr[ii].on)
            if (mtrr[ii].cache==MTRRF_WB) clearreg(ii);
      // split memory to list
      u64t nextpos = 0;
      ii = 0;
      for (u64t size=_2GbLL; size>=_128MbLL; size>>=1) {
         if (ucstart>=size) {
            if (!is_regavail(&reg)) return OPTERR_NOREG;
            mtrr[reg].start = nextpos;
            nextpos += (mtrr[reg].len = size);
            mtrr[reg].cache = MTRRF_WB;
            mtrr[reg].on    = 1;
            ucstart -= size;
            // use only 3 mtrr regs
            if (++ii==3) break;
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
      // check lost items for included UC entries
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
