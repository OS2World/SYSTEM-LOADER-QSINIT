//
// QSINIT "start" module
// scanf implementation
// -------------------------------
// there is no 'C', 'S', 'j', 'ls/ws', 'hf' & 'I64' (I64 is also ignored by printf)
//
#include "stdlib.h"
#include "qsint.h"
#include "syslocal.h"

#define chkbit(bs,idx)   (bs[(idx)>>3] & 1 << ((idx)&7))
#define setbit(bs,idx)   bs[(idx)>>3]|= 1 << ((idx)&7)
#define clrbit(bs,idx)   bs[(idx)>>3]&= ~(1 << ((idx)&7))

#define getNext \
   if ((in=(--width>0)?(inputCnt++,(*getc)(fp)):NO_CHAR)==EOF) inputCnt--;

#define F_NONE       0
#define F_EOF        1
#define F_LITERR     2
#define F_WS         3
#define F_LITERAL    4
#define F_CHAR       5
#define F_STR        6
#define F_INT        7
#define F_FLOAT      8
#define F_CHARSET    9
#define F_INPUTCNT  10

#define M_ASSIGN     1
#define M_NEXTCHAR   2
#define M_IGNSPACE   4
#define M_VALID      8
#define M_NCA        (M_NEXTCHAR+M_ASSIGN)
#define M_NCAS       (M_NEXTCHAR+M_ASSIGN+M_IGNSPACE)

static const u8t fmt_flag[F_INPUTCNT+1] = { 0, 0, 0, M_VALID+M_IGNSPACE,
   M_VALID+M_NEXTCHAR, M_VALID+M_NCA, M_VALID+M_NCAS, M_VALID+M_NCAS,
   M_VALID+M_NCAS, M_VALID+M_NCA, M_VALID+M_ASSIGN };

static int chtol(int ch) {
   if (ch<='9' && ch>='0') return ch - '0';
   if (ch<='f' && ch>='a') return ch - 'a' + 10;
   if (ch<='F' && ch>='A') return ch - 'A' + 10;
   return 0;
}

static int __stdcall isodigit(int cc) { return cc>='0' && cc<='7'; }

static int __stdcall str_getc(char **ptr) {
   int ch = **ptr;
   if (!ch) return EOF;
   ++*ptr;
   return ch;
}

#define NO_CHAR     -2
#define SBSIZE      32

/** common internal scanf, used by all scanf functions.
    @param  fp     user data, "stream" parameter for gc/ugc
    @param  fmt    format string
    @param  arg    pointer to variable arguments
    @param  gc     get char callback, can be 0 (fp will be used as string)
    @param  ugc    unget char callback, can be 0
    @return common scanf return value */
int _std _scanf_common(void *fp, const char *fmt, char **args, scanf_getch getc,
                       scanf_ungetch ungetc)
{
   int _std (*ccheck)(int);
   char      sb[SBSIZE], *pvar, *inputstr, chsize;
   int       in = NO_CHAR, inputCnt = 0, fch, width, base, assign,
         format = F_WS, nmatch = 0, ismatch, cvt_on = 0, isptr;

   if (!getc) {
      getc     = (int _std(*)(void*))str_getc;
      inputstr = fp;
      fp       = &inputstr;
   }
   while (fmt_flag[format] & M_VALID) {
      format=F_NONE; width=0; ismatch=0; assign=1; isptr=0;

      switch (fch = *fmt++) {
         case 0   : break;
         case ' ' : case '\t': case '\r': case '\n':
            format = F_WS;
            break;
         case '%' :
            fch = *fmt++;
            if (fch=='*') { assign=0; fch=*fmt++; }
            // get field width
            for (; isdigit(fch); fch = *fmt++) width = 10*width + fch-'0';
            // format char?
            if (fch=='z' || fch=='t') fch = 'l';
            if (fch=='h' || fch=='l' || fch=='L') { 
               chsize = fch; 
               fch    =*fmt++; 
               // hh - signed byte
               if (chsize=='h' && fch=='h') {
                  chsize = 'b';
                  fch    =*fmt++; 
               } else
               if (chsize=='l' && fch=='l') {
                  chsize = 'L';
                  fch    =*fmt++; 
               }
            } else 
               chsize = 0;
   
            switch (fch) {
               case   0: format = F_EOF;      break;
               case '%': format = F_LITERAL;  break;
               case 'n': format = F_INPUTCNT; break;
               case 'c': format = F_CHAR; cvt_on = 1; break;
               case 's': format = F_STR;  cvt_on = 1; break;
               case 'i': format = F_INT;  cvt_on = 1; base = 0;
                         break;
               case 'o': format = F_INT;  cvt_on = 1; base = 8;
                         ccheck = isodigit;
                         break;
               case 'u':
               case 'd': format = F_INT;  cvt_on = 1; base = 10;
                         ccheck = isdigit;
                         break;
               case 'p':
                         cvt_on = 1;
                         isptr++;
               case 'X':
               case 'x': format = F_INT;  cvt_on = 1; base = 16;
                         ccheck = isxdigit;
                         break;
               case 'e':
               case 'f':
               case 'g':
               case 'E':
               case 'G': format = F_FLOAT; cvt_on = 1;
                         if (width >= SBSIZE-1) width = SBSIZE-1;
                         break;
               case '[': {
                  int neg = 0, ii;
                  cvt_on  = 1;
                  format     = F_CHARSET;
                  
                  fch = *fmt++;
                  if (fch=='^') { neg = 1; fch = *fmt++; }
                  memset(&sb, 0, SBSIZE);
     
                  if (fch==']') {
                     setbit (sb, ']');
                     fch = (unsigned)*fmt++;
                  }
                  for (;fch && fch!=']'; fch = (unsigned)*fmt++)
                     setbit(sb, fch);
                  if (!fch) format = F_EOF;
                  if (neg)
                     for (ii=0; ii<SBSIZE; ii++) sb[ii]^=0xFF;
                  clrbit(sb, 0);
                  break;
               }
            }
            break;
         default:
            format = F_LITERAL;
            break;
      }
   
      if (width == 0)
         switch (format) {
            case F_CHAR   : width = 1; break;
            case F_STR    :
            case F_INT    :
            case F_CHARSET: width = _64KB-1; break;
            case F_FLOAT  : width = SBSIZE - 1; break;
         }
   
      if (fmt_flag[format] & M_IGNSPACE) {
         if (in==NO_CHAR)
            if ((in = getc(fp))!=EOF) inputCnt++;
         while (isspace(in))
            if ((in = getc(fp))!=EOF) inputCnt++; else break;
      }
   
      if ((fmt_flag[format] & M_NEXTCHAR) && in==NO_CHAR)
         if ((in = getc(fp))!=EOF) inputCnt++;
   
      pvar = (fmt_flag[format] & M_ASSIGN) && assign ? *args++ : 0;
   
      switch (format) {
         case F_LITERAL:
            if (in==fch) in = NO_CHAR; else format = F_LITERR;
            break;

         case F_CHAR:
            while (in!=EOF) {
               ismatch = 1;
               if (assign) *pvar++ = in;
               getNext;
               if (in==NO_CHAR) break;
            }
            break;

         case F_STR:
            while (in!=EOF && !isspace(in)) {
               ismatch = 1;
               if (assign) *pvar++ = in;
               getNext;
               if (in==NO_CHAR) break;
            }    
            if (ismatch && assign) *pvar = 0;
            break;

         case F_CHARSET:
            while (in!=EOF && chkbit(sb,in)) {
               ismatch = 1;
               if (assign) *pvar++ = in;
               getNext;
               if (in==NO_CHAR) break;
            }
            if (ismatch && assign) *pvar = 0;
            break;
   
         case F_INT: {
            int  isminus = 0,
                  iszero = 0;
            s64t     num = 0;
   
            if (in=='+' || in=='-') { isminus = in=='-'; getNext; }
            if (base == 0) {
               if (in!='0') { base = 10; ccheck = isdigit; } else {
                  getNext;
                  if (in=='x' || in=='X') {
                     base = 16;
                     ccheck = isxdigit;
                     getNext;
                  } else {
                     base = 8;
                     ccheck = isodigit;
                  }
               }
            } else 
            if (base == 16) {
               if (in == '0') {
                  getNext;
                  if (in=='x'||in=='X') { getNext; } else 
                     if (!ccheck(in)) iszero = 1;
               }
            }
            if (width>0 && ccheck(in) || iszero) {
               if (!iszero)
                  do {
                     if (base==10) num = num*10 + chtol(in); else
                     if (base==16) num = (num<<4) + chtol(in); else 
                        num = (num<<3) + chtol(in); // base 8
                     getNext;
                  } while (ccheck(in)); 
               ismatch = 1;
               if (assign) {
                  if (isminus) num = -num;
     
                  if (isptr) *(void **)pvar = (void*)(ptrdiff_t)num; else 
                  if (chsize=='b') *(s8t  *)pvar = (s8t)num; else
                  if (chsize=='h') *(short*)pvar = (short)num; else
                  if (chsize=='l') *(long *)pvar = (long)num; else
                  if (chsize=='L') *(s64t *)pvar = num; else
                     *(int*)pvar = (int)num;
               }
            }
            break;
         }
         case F_INPUTCNT:
            if (assign) {
               int count = inputCnt - (in==EOF||in==NO_CHAR?0:1);
     
               if (chsize=='b') *(s8t  *)pvar = (s8t)count; else 
               if (chsize=='h') *(short*)pvar = (short)count; else 
               if (chsize=='l') *(long *)pvar = count; else 
               if (chsize=='L') *(s64t *)pvar = count; else 
                  *(int*)pvar = (int)count;
            }
            break;
     
         case F_FLOAT: {
            char  *pc = sb;
            int valid = 0;
            if (in=='+' || in=='-') { *pc++=in; getNext; }

            if (width>0)
               while (isdigit(in))  { *pc++ = in; valid = 1; getNext; }
            if (width>0 && in=='.') { *pc++ = in; valid = 1; getNext; }
            if (width>0)
               while (isdigit(in))  { *pc++ = in; valid = 1; getNext; }

            if (width>0 && valid && (in=='e'||in=='E')) {
               *pc++ = in;
               valid = 0;    // check for digits after
               getNext;
               if (width>0 && (in=='+'||in=='-')) { *pc++ = in; getNext; }
               if (width>0)
                  while (isdigit(in)) { *pc++ = in; valid = 1; getNext; }
            }
            *pc = 0;
            if (valid) {
               ismatch = 1;
               if (assign) {
                  double dval = atof(sb);
                  if (chsize=='l' || chsize=='L') *(double*)pvar = dval; else
                     *(float*)pvar = (float)dval;
               }
            }
         }
         break;
      }
      if (ismatch && assign) nmatch++; else 
         if (!ismatch && format!=F_WS && format!=F_LITERAL && format!=F_INPUTCNT)
            break; // stops on no match
   }
   if (in!=EOF && in!=NO_CHAR && ungetc) ungetc(in,fp);
   // signal error
   if (in==EOF && !nmatch && (format==F_LITERR || cvt_on)) return EOF;

   return nmatch;
}
