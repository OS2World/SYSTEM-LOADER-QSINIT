//
// QSINIT "start" module
// bit map shared class
//
#include "stdlib.h"
#include "qcl/bitmaps.h"

#define BMPF_SIGN        0x46504D42          // BMPF

#define instance_ret(inst,err)              \
   bitmap_data *inst = (bitmap_data*)data;  \
   if (inst->sign!=BMPF_SIGN) return err;

#define instance_void(inst)                 \
   bitmap_data *inst = (bitmap_data*)data;  \
   if (inst->sign!=BMPF_SIGN) return;

typedef struct {
   u32t         sign;
   u8t           *bm;
   u32t         size;
   int        extmem;
} bitmap_data;

/// # of contiguous low cleared bits
u32t bit_c_low(u8t value);
#ifdef __WATCOMC__
#pragma aux bit_c_low = \
    "movzx   eax,al"    \
    "bsf     ax,ax"     \
    "jnz     found"     \
    "mov     ax,8"      \
"found:"                \
    parm [al] value [eax];
#endif

/// # of contiguous high cleared bits
u32t bit_c_hi(u8t value);
#ifdef __WATCOMC__
#pragma aux bit_c_hi =  \
    "movzx   eax,al"    \
    "bsr     ax,ax"     \
    "jnz     found"     \
    "mov     ax,0FFh"   \
"found:"                \
    "sub     al,7"      \
    "neg     al"        \
    parm [al] value [eax];
#endif

/// pos<<4|length of longest contiguous cleared bits area in byte
static u8t clbits_cont[] = {
   0x08,0x17,0x26,0x26,0x35,0x35,0x35,0x35,0x44,0x44,0x44,0x44,0x44,0x44,0x44,0x44,
   0x04,0x13,0x53,0x53,0x53,0x53,0x53,0x53,0x03,0x53,0x53,0x53,0x53,0x53,0x53,0x53,
   0x05,0x14,0x23,0x23,0x02,0x32,0x32,0x32,0x03,0x12,0x62,0x62,0x02,0x62,0x62,0x62,
   0x04,0x13,0x22,0x22,0x02,0x62,0x62,0x62,0x03,0x12,0x62,0x62,0x02,0x62,0x62,0x62,
   0x06,0x15,0x24,0x24,0x33,0x33,0x33,0x33,0x03,0x12,0x42,0x42,0x02,0x42,0x42,0x42,
   0x04,0x13,0x22,0x22,0x02,0x11,0x01,0x31,0x03,0x12,0x01,0x21,0x02,0x11,0x01,0x51,
   0x05,0x14,0x23,0x23,0x02,0x32,0x32,0x32,0x03,0x12,0x01,0x21,0x02,0x11,0x01,0x41,
   0x04,0x13,0x22,0x22,0x02,0x11,0x01,0x31,0x03,0x12,0x01,0x21,0x02,0x11,0x01,0x71,
   0x07,0x16,0x25,0x25,0x34,0x34,0x34,0x34,0x03,0x43,0x43,0x43,0x43,0x43,0x43,0x43,
   0x04,0x13,0x22,0x22,0x02,0x52,0x52,0x52,0x03,0x12,0x52,0x52,0x02,0x52,0x52,0x52,
   0x05,0x14,0x23,0x23,0x02,0x32,0x32,0x32,0x03,0x12,0x01,0x21,0x02,0x11,0x01,0x41,
   0x04,0x13,0x22,0x22,0x02,0x11,0x01,0x31,0x03,0x12,0x01,0x21,0x02,0x11,0x01,0x61,
   0x06,0x15,0x24,0x24,0x33,0x33,0x33,0x33,0x03,0x12,0x42,0x42,0x02,0x42,0x42,0x42,
   0x04,0x13,0x22,0x22,0x02,0x11,0x01,0x31,0x03,0x12,0x01,0x21,0x02,0x11,0x01,0x51,
   0x05,0x14,0x23,0x23,0x02,0x32,0x32,0x32,0x03,0x12,0x01,0x21,0x02,0x11,0x01,0x41,
   0x04,0x13,0x22,0x22,0x02,0x11,0x01,0x31,0x03,0x12,0x01,0x21,0x02,0x11,0x01,0x0 };


#define bit_c(value)     ((u32t)(clbits_cont[value]&0xF))
#define bit_c_pos(value) ((u32t)(clbits_cont[value]>>4))
#define get_next_byte()  \
   bb = *pos++;          \
   if (on) bb=~bb;

static u8t setmask[] = { 0x00,0x01,0x03,0x07,0x0F,0x1F,0x3F,0x7F,0xFF };
static u8t clbits4[] = { 4,3,3,2,3,2,2,1,3,2,2,1,2,1,1,0 };

static u32t _exicc bm_set(EXI_DATA, int on, u32t pos, u32t length);

/** init bitmap with external pointer.
    Init data is unchanged unlike alloc() call.
    @param  bmp      pointer to bitmap data, will be used by class code!
    @param  size     size of bitmap
    @return zero on error (invalid parameter) */
static u32t _exicc bm_init(EXI_DATA, void *bmp, u32t size) {
   instance_ret(bm,0);
   // both args must be zero or non-zero
   if (Xor(bmp,size)) return 0;
   if (!bm->extmem && bm->bm) free(bm->bm);

   bm->bm     = size?bmp:0;
   bm->size   = size;
   bm->extmem = 1;
   return 1;
}

/** init bitmap with internally allocated buffer.
    Bitmap will be zeroed after allocation.
    @param  size     size of bitmap
    @return zero on error (no memory) */
static u32t _exicc bm_alloc(EXI_DATA, u32t size) {
   instance_ret(bm,0);
   if (!size) {
      if (!bm->extmem && bm->bm) free(bm->bm);
      bm->bm   = 0;
      bm->size = 0;
   } else {
      if (bm->extmem) bm->bm = 0;
      bm->bm   = (u8t*)realloc(bm->bm, Round32(size)>>3);
      // no memory?
      if (!bm->bm) { bm->size = 0; return 0; }
      bm->size = size;
      memsetd((u32t*)bm->bm, 0, Round32(size)>>5);
   }
   bm->extmem = 0;
   return 1;
}

/** change bitmap size.
    Function will fail on trying to expand external buffer.
    @param  size     new size of bitmap
    @param  on       state of expanded space (1/0)
    @return zero on error (no memory or external buffer expansion). */
static u32t _exicc bm_resize(EXI_DATA, u32t size, int on) {
   instance_ret(bm,0);
   if (size==bm->size) return 1;

   if (bm->extmem) {
      if (size>bm->size) return 0;
      bm->size = size;
      if (!size) bm->bm = 0;
   } else {
      if (!size) {
         free(bm->bm); bm->bm = 0;
         bm->size = 0;
      } else {
         u32t oldsize = bm->size,
               nbsize = Round32(size)>>3;
         // realloc if nessesary
         if (nbsize != Round32(oldsize)>>3) {
            u8t *newptr = (u8t*)realloc(bm->bm, nbsize);
            if (!newptr) return 0;
            bm->bm = newptr;
         }
         bm->size = size;
         // clear new bits
         if (oldsize<size) bm_set(data, 0, on?1:0, oldsize, size - oldsize);
      }
   }
   return 1;
}

/// return bitmap size
static u32t _exicc bm_size(EXI_DATA) {
   instance_ret(bm,0);
   return bm->size;
}

static u8t* _exicc bm_mem(EXI_DATA) {
   instance_ret(bm,0);
   return bm->bm;
}

static void _exicc bm_setall(EXI_DATA, int on) {
   instance_void(bm);
   if (bm->size && bm->bm)
      memsetd((u32t*)bm->bm, on?FFFF:0, Round32(bm->size)>>5);
}

/** set bits.
    @param  on       set value (1/0)
    @param  pos      start position
    @param  length   number of bits to set
    @return number of actually changed bits */
static u32t _exicc bm_set(EXI_DATA, int on, u32t pos, u32t length) {
   instance_ret(bm,0);
   if (!length || pos>=bm->size) return 0;
   if (pos+length >= bm->size) length = bm->size-pos;

   setbits(bm->bm, pos, length, on?SBIT_ON:0);

   return length;
}

/** set area for value.
    @param  on       check value (1/0)
    @param  start    start position
    @param  length   number of bits to check
    @return 1 on full match, 0 if any difference is present */
static u32t _exicc bm_check(EXI_DATA, int on, u32t start, u32t length) {
   u32t endidx, st_byte, e_byte, st_bit, e_bit;
   u8t  *pos, bb;
   instance_ret(bm,0);

   if (!length || start+length>bm->size) return 0;
   endidx  = start + length - 1;

   st_byte = start>>3;  st_bit = start&7;
   e_byte  = endidx>>3; e_bit  = endidx&7;
   pos = bm->bm+st_byte;
   // get first byte
   get_next_byte();
   if (st_byte==e_byte) // single byte check
      return (~setmask[st_bit]&setmask[e_bit+1]&bb)==0 ? 1 : 0;
   else {
      u32t ii;
      if ((~setmask[st_bit]&bb)!=0) return 0;

      for (ii=st_byte+1; ii<e_byte; ii++) {
         get_next_byte();
         if (bb) return 0;
      }
      get_next_byte();
      if ((setmask[e_bit+1]&bb)!=0) return 0;
   }

   return TRUE;

}

/** search for contiguous area.
    @param  on       search value (1/0)
    @param  length   number of bits to search
    @param  hint     position where to start
    @return position of found area or 0xFFFFFFFF if no one */
static u32t _exicc bm_find(EXI_DATA, int on, u32t length, u32t hint) {
   u32t  bidx, size, sbyte, step;
   u8t  *pos;
   instance_ret(bm,FFFF);
   if (length>bm->size) return FFFF;
   if (hint>=bm->size) hint = 0;
   bidx  = hint&7;
   size  = bm->size;
   sbyte = Round8(size)>>3;
   // invert unused bits in last byte to prevent incorrect count of them
   if ((size&7)!=0)
      if (on) bm->bm[sbyte-1]&=setmask[size&7]; else
         bm->bm[sbyte-1]|=~setmask[size&7];

   for (step=0; step<2; step++) {
      u32t   sidx, eidx;
      u8t      bb;

      if (!step) { sidx = hint>>3; eidx = sbyte; } else
      if (hint) {
         if (length<2) eidx = hint>>3; else {
            eidx = (hint>>3) + (length-2>>3) + 2;
            if (eidx>sbyte) eidx = sbyte;
         }
         hint = 0; bidx = 0; sidx = 0;
      } else
         return FFFF;

      pos = bm->bm+sidx;
      get_next_byte();
      // invert disallowed bits in first byte
      bb |= setmask[bidx];

      if (length<=9) {
         u32t cbit = sidx * 8;
         u8t   pbb = 0xFF;

         while (1) {
            if (bit_c_hi(pbb)+bit_c_low(bb) >= length) {
               u32t rcpos = cbit - bit_c_hi(pbb);
               if (rcpos + length <= size) return rcpos;
            }
            if (bit_c(bb) >= length) return cbit + bit_c_pos(bb);
            pbb = bb; cbit+= 8;
            if (cbit<eidx*8) {
               get_next_byte();
            } else
               break;
         } // end loop cbit
      } else
      if (length<15) {
         u32t  cbit = sidx * 8, rcpos;
         u8t   ppbb = 0xFF,
                pbb = 0xFF;
         while (1) {
            if (bit_c_hi(pbb)+bit_c_low(bb) >= length)
               if ((rcpos = cbit - bit_c_hi(pbb)) + length <= size) return rcpos;
            if (pbb==0 && bit_c_hi(ppbb)+8+bit_c_low(bb)>=length)
               if ((rcpos = (cbit - 8) - bit_c_hi(ppbb)) + length <= size) return rcpos;
            // next byte
            ppbb = pbb; pbb = bb; cbit+= 8;
            if (cbit<eidx*8) {
               get_next_byte();
            } else
               break;
         }
      } else {
         u32t  cbyte = sidx, nreq = length-7>>3, nfound = 0;
         u8t startbb = 0xFF;
         s32t  start = sidx - 1;

         while (1) {
            if (nfound>=nreq && bit_c_hi(startbb) + nfound*8 + bit_c_low(bb)>=length) {
               u32t rcpos = (start * 8) + (8 - bit_c_hi(startbb));
               if (rcpos + length<= size) return rcpos;
            }
            if (!bb) nfound++; else { nfound = 0; startbb = bb; start = cbyte; }
            cbyte++;
            if (cbyte<eidx) {
               get_next_byte();
            } else
               break;
         }
      }
   }
   return FFFF;
}

// find bits for direct call
u32t _std findbits(void *bmp, u32t bmplen, int on, u32t length, u32t hint) {
   bitmap_data bm;
   bm.sign   = BMPF_SIGN;
   bm.bm     = (u8t*)bmp;
   bm.size   = bmplen;
   bm.extmem = 1;
   return bm_find(&bm, 0, on, length, hint);
}

/** search and invert contiguous area.
    @param  on       search value (1/0)
    @param  length   number of bits to search
    @param  hint     position where to start
    @return position of found area or 0xFFFFFFFF if no one */
static u32t _exicc bm_findset(EXI_DATA, int on, u32t length, u32t hint) {
   u32t  pos;
   instance_ret(bm,FFFF);

   pos = bm_find(data, 0, on, length, hint);
   if (pos!=FFFF) bm_set(data, 0, !on, pos, length);
   return pos;
}

/** search for longest contiguous area.
    @param  [in]  on      search value (1/0)
    @param  [out] start   pointer to found position
    @return length of area or zero if no one */
static u32t _exicc bm_longest(EXI_DATA, int on, u32t *start) {
   u32t  size, sbyte, idx, lsize, lindex, csize, cindex;
   u8t  *pos;
   instance_ret(bm,0);
   if (!start || !bm->size) return 0;

   size  = bm->size;
   sbyte = Round8(size)>>3;
   // invert unused bits in last byte to prevent incorrect count of them
   if ((size&7)!=0)
      if (on) bm->bm[sbyte-1]&=setmask[size&7]; else
         bm->bm[sbyte-1]|=~setmask[size&7];

   lsize = 0; lindex = 0; csize = 0; cindex = 0;
   pos = bm->bm;

   for (idx=0; idx<sbyte; idx++) {
      u8t bb;
      get_next_byte();

      if (bb) {
         csize += bit_c_low(bb);
         if (csize>lsize) { lsize = csize; lindex = cindex; }

         csize = bit_c_hi(bb);
         cindex = (idx * 8) + (8 - csize);

         if (lsize<8 && csize<8) {
            u32t inlen = bit_c(bb);
            if (inlen>lsize && inlen>csize) {
               lindex = (idx<<3) + bit_c_pos(bb);
               lsize  = inlen;
            }
         }
      } else
         csize += 8;
   }
   if (csize > lsize) { lsize = csize; lindex = cindex; }
   *start = lindex;
   return lsize;
}

/** return total number of set or cleared bits.
    @param  on       search value (1/0)
    @return calculated value */
static u32t _exicc bm_total(EXI_DATA, int on) {
   u32t  size, sbyte, ii, rc;
   u8t  *pos;
   instance_ret(bm,0);
   if (!bm->size) return 0;

   size  = bm->size;
   sbyte = Round8(size)>>3;
   // invert unused bits in last byte to prevent incorrect count of them
   if ((size&7)!=0)
      if (on) bm->bm[sbyte-1]&=setmask[size&7]; else
         bm->bm[sbyte-1]|=~setmask[size&7];

   pos = bm->bm;
   for (rc=0, ii=0; ii<sbyte; ii++) {
      u8t bb;
      get_next_byte();
      rc += clbits4[bb&0xF] + clbits4[bb>>4];
   }
   return rc;
}

/// return current instance state (BMS_* flags set).
static u32t _exicc bm_state(EXI_DATA) {
   bitmap_data *bm = (bitmap_data*)data;
   u32t        res = 0;
   if (bm->sign!=BMPF_SIGN) res = res; else {
      if (!bm->bm) res|=BMS_EMPTY;
      if (bm->extmem) res|=BMS_EXTMEM;
   }
   return res;
}

static void *methods_list[] = { bm_init, bm_alloc, bm_resize, bm_size, bm_mem,
   bm_setall, bm_set, bm_check, bm_find, bm_findset, bm_longest, bm_total,
   bm_state };

static void _std bm_initialize(void *instance, void *data) {
   bitmap_data *bm = (bitmap_data*)data;
   bm->sign     = BMPF_SIGN;
   // other fields zeroed by caller
}

static void _std bm_finalize(void *instance, void *data) {
   instance_void(bm);
   if (!bm->extmem && bm->bm) free(bm->bm);
   bm->bm     = 0;
   bm->sign   = 0;
   bm->size   = 0;
}

void exi_register_bitmaps(void) {
   exi_register("bit_map", methods_list, sizeof(methods_list)/sizeof(void*),
      sizeof(bitmap_data), 0, bm_initialize, bm_finalize, 0);
}
