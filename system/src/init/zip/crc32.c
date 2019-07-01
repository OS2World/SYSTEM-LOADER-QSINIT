//
// QSINIT
// zlib crc32 code, modified to use dynamically allocated and created table
// note: do not using modern (and faster) variant to save code size (~1 kb)
//
/* crc32.c -- compute the CRC-32 of a data stream
 * Copyright (C) 1995-1998 Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* @(#) $Id$ */

#include "qstypes.h"
//#include "qsint.h"

u32t *crc_table;

/*
  Generate a table for a byte-wise 32-bit CRC calculation on the polynomial:
  x^32+x^26+x^23+x^22+x^16+x^12+x^11+x^10+x^8+x^7+x^5+x^4+x^2+x+1.

  Polynomials over GF(2) are represented in binary, one bit per coefficient,
  with the lowest powers in the most significant bit.  Then adding polynomials
  is just exclusive-or, and multiplying a polynomial by x is a right shift by
  one.  If we call the above polynomial p, and represent a byte as the
  polynomial q, also with the lowest power in the most significant bit (so the
  byte 0xb1 is the polynomial x^7+x^3+x+1), then the CRC is (q*x^32) mod p,
  where a mod b means the remainder after dividing a by b.

  This calculation is done using the shift-register method of multiplying and
  taking the remainder.  The register is initialized to zero, and for each
  incoming bit, x^32 is added mod p to the register if the bit is a one (where
  x^32 mod p is p+x^32 = x^26+...+1), and the register is multiplied mod p by
  x (which is shifting right by one and adding x^32 mod p if the bit shifted
  out is a one).  We start with the highest power (least significant bit) of
  q and repeat for all eight bits of q.

  The table is simply the CRC of all possible eight bit values.  This is all
  the information needed to generate CRC's on data a byte at a time for all
  combinations of CRC register values and incoming bytes.
*/
void make_crc_table(void *table) {
   u32t   c;
   int n, k;
   u32t poly;            /* polynomial exclusive-or pattern */
   /* terms of polynomial defining this crc (except x^32): */
   static const u8t p[] = {0,1,2,4,5,7,8,10,11,12,16,22,23,26};

   crc_table = (u32t*)table;
 
   /* make exclusive-or pattern from polynomial (0xedb88320L) */
   poly = 0L;
   for (n = 0; n < sizeof(p)/sizeof(u8t); n++)
      poly |= 1L << (31 - p[n]);
 
   for (n = 0; n < 256; n++) {
      c = (u32t)n;
      for (k = 0; k < 8; k++)
        c = c & 1 ? poly ^ (c >> 1) : c >> 1;
      crc_table[n] = c;
   }
}

/* ========================================================================= */
#define DO1(buf) crc = tab[((int)crc ^ (*buf++)) & 0xff] ^ (crc >> 8);
#define DO2(buf)  DO1(buf); DO1(buf);
#define DO4(buf)  DO2(buf); DO2(buf);
#define DO8(buf)  DO4(buf); DO4(buf);

/* ========================================================================= */
u32t _std crc32(u32t crc, u8t* buf, u32t len) {
   register u32t*tab = crc_table;

   if (!buf||!tab) return 0;
   crc = crc ^ 0xFFFFFFFFL;
   while (len >= 8) {
     DO8(buf);
     len -= 8;
   }
   if (len) do {
     DO1(buf);
   } while (--len);
   return crc ^ 0xFFFFFFFFL;
}
