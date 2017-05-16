#include "stdlib.h"
#include "math.h"
#include "errno.h"
#include "syslocal.h"

char* __stdcall _gcvt(double value, int digits, char *str) {
   char fmt[24];
   snprintf(fmt, 24, "%%#%d.%dg", digits, digits-1);
   sprintf(str, fmt, value);
   return str;
}

double __stdcall frexp(double value, int *exp) {
   union {
      double x;
      u16t   a[4];
   } u;
   int n = 0;
   u.x   = value;
   if (value!=0.0) {
      n  = (u.a[3] & 0x7FF0) >> 4;
      n -= 0x03FE;
      u.a[3] &= 0x800F;
      u.a[3] |= 0x3FE0;
   }
   *exp = n;
   return u.x;
}

static long    ltab[] = {100000000, 10000000, 1000000, 100000, 10000, 1000, 100, 10, 1, 0};
static double pow10[] = {10., 1e2, 1e4, 1e8, 1e16, 1e32, 1e64, 1e128, 1e256};

#define MAXSIG   17     // max # of significant digits

// FP to str convertion (used in printf)
int __stdcall fptostr(double value, char *buf, int int_fmt, int prec) {
   int      ii, jj, count, xprec, k, grp, iexp, negAlt;
   char *p0, *buf0, *decpt, *round, *pp;
   char        fmt = int_fmt;
   char      f_fmt, g_fmt, iszero;
   long       lval;
   double     mant;
   union {
      double    fv;
      struct { u32t lo, hi; };
   } vrec;

   vrec.fv = value;
   if ((vrec.hi & 0x7FF00000) == 0x7FF00000) {
      if ((vrec.hi & 0x000FFFFF) || vrec.lo) {
         strcpy(buf, "<NaN>");
         count = 5;
      } else {
         count = 0;
         if (vrec.hi & 0x80000000) {
            *buf++ = '-';
            count++;
         }
         strcpy (buf, "<Inf>");
         count += 5;
      }
      return --count;
   }
   negAlt = 1;
   if (fmt&0x80) {
      negAlt = 0;
      fmt   &= 0x7F;
   }
   count  = 0;
   f_fmt  = g_fmt = 0;      /* assume e format */
   iszero = 0;
   if (prec<0) prec = 6;

   if (fmt=='g'||fmt=='G') {
      g_fmt = 1;
      if (prec == 0) ++prec;
   } else {
      ++prec;
      if (fmt=='f') f_fmt = 1;
   }
   xprec = prec;
   if (xprec>MAXSIG) xprec = MAXSIG;
   // negative value
   if (value<0.0) {
      *buf++ = '-';
      count++;
      value = -value;
   }
   // split value
   mant = value;
   if (mant==0.0) { iexp = 0; iszero = 1; } else {
      int  jexp, negexp = 0;
      frexp(mant, &jexp);
      jexp = jexp * 308 >> 10;     // 308/1024 ~= log10(2)
      iexp = jexp;
      if (jexp<0) { jexp = -jexp; negexp++; }
      for (ii=0; jexp!=0; ii++) {
         if (jexp&1)
            if (negexp) mant*=pow10[ii]; else mant/=pow10[ii];
         jexp = (u32t)jexp>>1;
      }
      if (mant < 1.0) { iexp--; mant*=10.0; } else 
      if (mant>=10.0) { iexp++; mant/=10.0; }
   }
   if (g_fmt && iexp>=-4 && iexp<prec) f_fmt=1;
   pp   = buf;
   buf0 = pp;
   if (iszero) *pp++ = '0'; else {
      ii = 1;
      if (!f_fmt) value = mant; else {
         ii = iexp;
         if (ii<0) ii = 1;
      }
      for (jj=ii; jj>=9; jj-=9) value/=1E9;
      
      for (grp = 0; grp < 3; ++grp) {
         lval = (long) value;    /* convert to long */
         if (grp)
             for (k=0; lval<ltab[k]; ++k) *pp++='0';
         if (lval) {
            _ltoa(lval, pp, 10);
            pp   += strlen(pp);
            value-= (double) lval;
         }
         ii -= jj;
         if (ii==0) break;
         jj  = 9;
         if (grp<2) value*=1e9;   // next 9 digits
      }
      if (f_fmt) {
         if (iexp > MAXSIG)
            for (p0 = buf0+MAXSIG; p0 < pp; p0++) *p0 = '0';
         if (iexp < 0) ii= 1;
         // just fill with zeros
         for ( ; --ii>=0; ) *pp++ = '0';
         if (iexp + xprec >= MAXSIG)
            if ((xprec = MAXSIG-iexp) <= 0) xprec = iszero = 1;
      }
   }
   decpt = pp;
   *pp++ = '.';
   if (iszero) {
      for (ii=prec; --ii; ) *pp++='0';
   } else {
      if (f_fmt && iexp<0) {
         value = mant / 10.0;
         for (ii=iexp; ++ii; ) *pp++='0';
      }
      for (ii= 0; ii<2; ii++) {
         if (value==0.0) {
            for (jj=9; --jj>=0; ) *pp++='0';
            continue;
         }
         // 9 digits at pass
         value*= 1e9;
         lval  = (long) value;
         for (k=0; lval<ltab[k]; ++k) *pp++='0';
         _ltoa(lval, pp, 10);
         pp += strlen(pp);
         if (prec<=9) break;
         if (ii==0) value -= (double)lval;
      }
      if (g_fmt && f_fmt) xprec += -iexp;
      for (p0=decpt+xprec+1; p0<pp; p0++) *p0='0';
   }
   *pp = 0;

   if (g_fmt) {
      if (xprec>prec) prec = xprec;
      if ((p0 = buf0 + prec) >= decpt) p0++;
      if (pp<p0) p0=pp;
   } else {
      p0 = decpt + prec;
      while (pp<=p0) *pp++='0';
   }
   pp = round = decpt + xprec;
   if (*pp>='5') {
      // round it
      while (1) {
         if (*--pp=='.') --pp;
         if ((*pp+=1)<='9') break;
         *pp='0';
         if (pp==buf) {
            char *tptr;
            p0++; round++;
            for (tptr=round; tptr!=pp; tptr--) tptr[0] = tptr[-1];
            *pp = '1';
            buf0 = pp;
            if (!f_fmt) {
               *++pp = '.';
               *++pp = '0';
               iexp++;
               p0--;
            }
            break;
         }
      }
   }
   *round = '0';
   *p0    = 0;
   pp     = p0;

   if (g_fmt) {
      fmt += 'e' - 'g';
      if (negAlt) {
         while (*--pp=='0') ;
         *++pp = 0;
      }
   }
   // strip trailing '.'
   if (pp[-1]=='.' && negAlt) *--pp = 0;
   // exponent part
   if (!f_fmt) {
      *pp++ = fmt;
      if (iexp >= 0) *pp++='+'; else {
         *pp++ = '-';
         iexp  = -iexp;
      }
      ii = iexp;
      if (ii>99) {
         *pp++ = ii / 100 + '0';
         ii   %= 100;
      }
      *pp++ = ii/10 + '0';
      *pp++ = ii%10 + '0';
      *pp   = 0;
   }
   ii = (ptrdiff_t)pp - (ptrdiff_t)buf0;
   return count + ii;
}

const double* _std _get_HugeVal_ptr(void) {
   static u16t const HugeValue[4] = { 0x0000, 0x0000, 0x0000, 0x7FF0 };
   return (double*)&HugeValue;
}

#define MAX_ABS_EXP 325

double __stdcall strtod(const char *str, char **endptr) {
   const char *pp = str;
   int        exp = 0, eexp, negexp, neg = 0, c, progress = 0,
          ndigits = 0;
   double    fnum = 0.0, fnum_org,
           bigval = 4503599627370496.;     /* 2^52 */
   // trim it first
   while (*pp==' ' || *pp>=9 && *pp<=13) ++pp;
   if (*pp=='-') { neg++; pp++; } else
   if (*pp=='+') pp++;

   while ((c = *pp++) >= '0' && c <= '9') {
      if (++ndigits<16 || fnum<bigval) fnum = fnum*pow10[0] + (c - '0');
         else ++exp;
      progress = 1;
   }
   if (c == '.') {
      while ((c = *pp++) >= '0' && c <= '9')
         if (++ndigits<16 || fnum<bigval) {
            fnum = fnum*pow10[0] + (c - '0');
            exp--;
            progress = 1;
         }
   }
   fnum_org = fnum;

   eexp = negexp = 0;
   if (c == 'E' || c == 'e') {
      if ((c=*pp) == '+') pp++; else
      if (c == '-') { negexp++; pp++; }

      while ((c = *pp++) >= '0' && c <= '9') {
         eexp = eexp*10 + (c - '0');
         if (abs (negexp ? exp - eexp : exp + eexp) > MAX_ABS_EXP)
            break;
      }
      if (negexp) exp-=eexp; else exp+=eexp;
   }
   negexp = 0;
   if (exp<0) { negexp++; exp=-exp; }
   if (exp) {
      eexp = 256;
      for (c = 8; c >= 0; c--) {
         while (exp >= eexp) {
            if (negexp) fnum/=pow10[c]; else fnum*=pow10[c];
            exp -= eexp;
         }
         if (exp == 0) break;
         eexp >>= 1;
      }
   }
   if (endptr) *endptr = (char*)(progress ? pp-1 : str);

   if (fnum==0.0 && fnum_org!=0.0) {
      set_errno(ERANGE);
      return fnum;
   }
   if (memcmp(_get_HugeVal_ptr(), &fnum, sizeof(double))==0)
      set_errno(ERANGE);

   if (neg) fnum = -fnum;
   return fnum;
}

double __stdcall atof(const char *str) { return strtod(str, 0); }
