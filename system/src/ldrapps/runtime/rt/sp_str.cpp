/* Notes:
   1. unique() call allocstr()
   2. allocstr() MUST BE CALLED BEFORE reallocstr()
*/

#include "sp_str.h"
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include "minmax.h"

char *spnullstr="";

spstr::spstr(spstr const &from,d __pos,d __n) {//   from another string
#ifndef NO_SP_ASSERTS
  assert(!from.Lock);
#endif
  data=NULL;
  Lock=0;
  l tocopy=from.length()-(l)__pos;
  if (!from.data||tocopy<=0) return; else
  if (!__pos&&!__n) {
    from.data->usecnt++;
    data=from.data;
  } else initstring(from.data->buf+__pos,min(__n,(d)tocopy));
}

void spstr::initstring(char const *str, d __n) {
  l len;
  if (__n&&str) {
    char*pos=(char*)memchr(str,0,__n);
    len=pos?pos-str:__n;
  } else len=str?strlen(str):0;
  if (len>0) {
    allocstr(len);
    if (len) memcpy(data->buf,str,len);
    *(data->buf+len)=0;
    data->chars=len;
    data->hash =FFFF;
  } else data=NULL;
}

spstr::spstr(char const *str, l __n) {          //   from C string
  data=NULL;
  Lock=0;
  initstring(str,__n);
}

spstr::spstr(char str[]) {
  data=NULL;
  Lock=0;
  initstring(str,0);
}

spstr::spstr(char ch, d __rep) {                //   from single character
  data=NULL;
  Lock=0;
  if (__rep>0) {
    allocstr(__rep);
    if (__rep>1) memset(data->buf,ch,__rep); else *data->buf=ch;
    *(data->buf+__rep)=0;
    data->chars=__rep;
    data->hash =d(ch)*__rep;
  }
}

//spstr::~spstr() {                                 // destructor
//#ifndef NO_SP_ASSERTS
//  assert(!Lock);
//#endif
//  if (data&&--data->usecnt==0) disposestr();
//}
char& spstr::operator[] (l __pos) {
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  _unique();
  return __pos>=0&&__pos<data->chars?data->buf[__pos]:data->buf[data->chars-1];
}

spstr & spstr::operator = (spstr const &from) {
#ifndef NO_SP_ASSERTS
  assert(!Lock&&!from.Lock);
#endif
  if (from.data) from.data->usecnt++;    // don't reorder this two lines! ;)
  if (data&&--data->usecnt==0) disposestr();
  data=from.data;
  return *this;
}

spstr & spstr::operator = (char const *from)  {
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  if (data&&--data->usecnt==0) disposestr();
  data=NULL;
  initstring(from,0);
  return *this;
}

spstr::operator a() const {
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  a res={0.0,APPLY_ADD/*APPLY_SET*/}; // 04.09.2002
  if (data&&data->chars>0) {
    switch (data->buf[0]) {
      case '+':res.Op=APPLY_ADD; break;
      case '-':res.Op=APPLY_SUB; break;
      case '*':res.Op=APPLY_MUL; break;
      case '/':res.Op=APPLY_DIV; break;
      default :res.Op=APPLY_SET; break;
    }
    l vv=res.Op==APPLY_SET?0:1;
    if (data->chars>vv) res.Value=(float)atof(data->buf+vv);
  }
  return res;
}

spstr& spstr::operator= (a const &value) {
  tofloat(value.Value,7);
  switch (value.Op) {
    case APPLY_ADD:insert("+",0); break;
    case APPLY_SUB:insert("-",0); break;
    case APPLY_MUL:insert("*",0); break;
    case APPLY_DIV:insert("/",0); break;
  }
  return *this;
}

#ifndef NO_SPSTR_SPRINTF
spstr& spstr::sprintf(const char *fmt,...) {
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  if (data&&--data->usecnt==0) disposestr();
  data=NULL;
  allocstr(strlen(fmt)*5);
  va_list arglist;
  va_start(arglist,fmt);
  l sz;
  do {
    sz=_vsnprintf(data->buf,data->maxnow-1,fmt,arglist);
    if (sz<0) reallocstr(data->maxnow*2);
  } while (sz<0);
  va_end(arglist);
  if (sz==data->maxnow-1) data->buf[sz]=0;
  data->chars=strlen(data->buf);
  data->hash =FFFF;
  return *this;
}
#endif // NO_SPSTR_SPRINTF

spstr& spstr::operator= (int const ll) {
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  if (data&&--data->usecnt==0) disposestr();
  data=NULL;
  allocstr(33);
  ITOA(ll,data->buf,10);
  data->chars=strlen(data->buf);
  data->hash =FFFF;
  return *this;
}

spstr& spstr::operator= (d const ll) {
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  if (data&&--data->usecnt==0) disposestr();
  data=NULL;
  allocstr(33);
  UTOA(ll,data->buf,10);
  data->chars=strlen(data->buf);
  data->hash =FFFF;
  return *this;
}

spstr &spstr::tofloat(float const ff,d digits) {
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  if (data&&--data->usecnt==0) disposestr();
  data=NULL;
  allocstr(digits+7);
  GCVT(ff,digits,data->buf);
  data->chars=strlen(data->buf);
  data->hash =FFFF;
  return *this;
}

                                                // append operators
spstr & spstr::operator += ( spstr const &from) {   //   a string
#ifndef NO_SP_ASSERTS
  assert(!from.Lock);
#endif
  if (!from.data) return *this;
  if (!data) {
    data=from.data;
    data->usecnt++;
    return *this;
  }
  unique(data->chars+from.data->chars+1);
  memcpy(data->buf+data->chars,from.data->buf,from.data->chars+1);
  data->chars+=from.data->chars;
  if (data->hash!=FFFF)
    if (from.data->hash!=FFFF) data->hash+=from.data->hash;
  return *this;
}

spstr & spstr::operator += ( char const *from) {      //   a C string
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  l len=strlen(from);
  unique((data?data->chars:0)+len);
  memcpy(data->buf+data->chars,from,len+1);
  data->chars+=len;
  data->hash =FFFF;
  return *this;
}

spstr& spstr::operator+= (char cc) {                  // char
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  unique((data?data->chars:14)+1);
  data->buf[data->chars++]=cc;
  data->buf[data->chars]  = 0;
  if (data->hash!=FFFF) data->hash+=d(cc);
  return *this;
}

spstr& spstr::operator+=(l ll) {                  // char
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  unique((data?data->chars:0)+14);
  char *lptr=data->buf+data->chars;
  ITOA(ll,lptr,10);
  data->chars+=strlen(lptr);
  data->hash =FFFF;
  return *this;
}

void spstr::unique(d len) {
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  if (!data) allocstr(len?len:10); else
  if (data->usecnt>1) {
    pstrinfo olddata;
    olddata=data; olddata->usecnt--;
    data=NULL;
#ifdef LOMEM_STR
    allocstr(len>olddata->chars?len:olddata->chars);
#else
    allocstr(len>olddata->maxnow?len:olddata->maxnow);
#endif
    memcpy(data->buf,olddata->buf,olddata->chars);
    *(data->buf+olddata->chars)=0;
    data->chars=olddata->chars;
    data->hash =olddata->hash;
    if (!olddata->usecnt) FREE(olddata);
  } else
  if (len) reallocstr(len);
}

void spstr::_unique() {
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  if (!data) allocstr(10);
  if (data->usecnt>1) {
    pstrinfo olddata;
    olddata=data; olddata->usecnt--;
    data=NULL;
#ifdef LOMEM_STR
    allocstr(olddata->chars);
#else
    allocstr(olddata->maxnow);
#endif
    memcpy(data->buf,olddata->buf,olddata->chars);
    *(data->buf+olddata->chars)=0;
    data->chars=olddata->chars;
    data->hash =olddata->hash;
    if (!olddata->usecnt) FREE(olddata);
  }
}

Bool operator == (spstr const &s1, spstr const &s2) {// string == string?
#ifndef NO_SP_ASSERTS
  assert(!s1.Lock&&!s2.Lock);
#endif
  register strinfo *S1=s1.data, *S2=s2.data;
  if (S1==S2) return true;
  if (!S1||!S2) return s1.length()==s2.length();
//  if (S1->hash!=FFFF&&S2->hash!=FFFF&&S1->hash!=S2->hash) return false;
  return (S1->chars==S2->chars&&(strcmp(S1->buf,S2->buf)==0));
}

Bool operator == (spstr const &s1, char const *s2) {  // string == C string?
#ifndef NO_SP_ASSERTS
  assert(!s1.Lock);
#endif
//  if (s1.data) *(s1.data->buf+s1.data->chars)=0;
  return (strcmp(s1(),s2)==0);
}

Bool operator == (char const *s1, spstr const &s2) {  // C string == string?
#ifndef NO_SP_ASSERTS
  assert(!s2.Lock);
#endif
//  if (s2.data) *(s2.data->buf+s2.data->chars)=0;
  return (strcmp(s2(),s1)==0);
}

Bool operator == (spstr const &s1, char c2) {         // string == character?
#ifndef NO_SP_ASSERTS
  assert(!s1.Lock);
#endif
  return (s1.length()==1&&*s1.data->buf==c2);
}

Bool operator == (char c1, spstr const &s2) {         // character == string?
#ifndef NO_SP_ASSERTS
  assert(!s2.Lock);
#endif
  return (s2.length()==1&&*s2.data->buf==c1);
}

Bool operator != (spstr const &s1, spstr const &s2) {// string != string?
#ifndef NO_SP_ASSERTS
  assert(!s1.Lock&&!s2.Lock);
#endif
  if (!s1.data||!s2.data) return s1.length()!=s2.length();
  if (s1.data->hash!=FFFF&&s2.data->hash!=FFFF&&s1.data->hash!=s2.data->hash)
    return true;
//  *(s1.data->buf+s1.data->chars)=0;
//  *(s2.data->buf+s2.data->chars)=0;
  return (s1.data->chars!=s2.data->chars||strcmp(s1.data->buf,s2.data->buf));
}

Bool operator != (spstr const &s1, char const *s2) {  // string != C string?
#ifndef NO_SP_ASSERTS
  assert(!s1.Lock);
#endif
//  if (s1.data) *(s1.data->buf+s1.data->chars)=0;
  return strcmp(s1(),s2);
}

Bool operator != (char const *s1, spstr const &s2) {  // C string != string?
#ifndef NO_SP_ASSERTS
  assert(!s2.Lock);
#endif
//  if (s2.data) *(s2.data->buf+s2.data->chars)=0;
  return strcmp(s2(),s1);
}

Bool operator != (spstr const &s1, char c2) {         // string != character?
#ifndef NO_SP_ASSERTS
  assert(!s1.Lock);
#endif
  return (s1.length()!=1||*s1.data->buf!=c2);
}

Bool operator != (char c1, spstr const &s2) {         // character != string?
#ifndef NO_SP_ASSERTS
  assert(!s2.Lock);
#endif
  return (s2.length()!=1||*s2.data->buf!=c1);
}

spstr operator+(spstr const &s1, spstr const &s2) {// string + string
#ifndef NO_SP_ASSERTS
  assert(!s1.Lock&&!s2.Lock);
#endif
  l len=s1.length()+s2.length();
  spstr str;
  if (!len) return str;
  str.unique(len);
  if (s1.length()) memcpy(str.data->buf,s1.data->buf,s1.length());
  if (s2.length()) memcpy(str.data->buf+s1.length(),s2.data->buf,s2.length());
  str.data->chars=len;
  str.data->hash =FFFF;
  str.data->buf[str.data->chars]=0;
  return str;
}

spstr operator+(spstr const &s1, char const *s2) {  // string + C string
#ifndef NO_SP_ASSERTS
  assert(!s1.Lock);
#endif
  if (*s2==0) return s1;
  l s2len=strlen(s2),len=s1.length()+s2len;
  spstr str;
  str.unique(len);
  if (s1.length()) memcpy(str.data->buf,s1.data->buf,s1.length());
  memcpy(str.data->buf+s1.length(),s2,s2len+1);
  str.data->chars=len;
  str.data->hash =FFFF;
  return str;
}

spstr operator+(char const *s1, spstr const &s2) {  // C string + string
#ifndef NO_SP_ASSERTS
  assert(!s2.Lock);
#endif
  spstr str(s1);
  return operator+(str,s2);
}

spstr operator+(spstr const &s1, char c2) {         // string + character
#ifndef NO_SP_ASSERTS
  assert(!s1.Lock);
#endif
  char ch[2];
  ch[0]=c2; ch[1]=0;
  return operator+(s1,ch);
}

spstr operator+(char c1, spstr const &s2) {         // character + string
#ifndef NO_SP_ASSERTS
  assert(!s2.Lock);
#endif
  spstr ss(c1);
  return operator+(ss,s2);
}

                                            // find substring location
l spstr::pos(spstr const &s,l startfrom) const { // search string is a string
#ifndef NO_SP_ASSERTS
  assert(!Lock&&!s.Lock);
#endif
  if (!data||data->chars-startfrom<s.data->chars) return -1; else {
//    *(data->buf+data->chars)=0;
//    *(s.data->buf+s.data->chars)=0;
    char *tmp=strstr(data->buf+startfrom,s.data->buf);
    return tmp?tmp-data->buf:-1;
  }
}

l spstr::pos(char const *s,l startfrom) const {   // search string is a C string
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  if (!data) return -1;
  if (startfrom<0||startfrom>=data->chars) return -1;
  char *tmp=strstr(data->buf+startfrom,s);
  return tmp?tmp-data->buf:-1;
}

// search for string by it begin and end
l spstr::bepos(const char *begin,const char *end,l &epos,l startfrom) const {
#ifndef NO_SP_ASSERTS
  assert(!Lock&&begin&&end);
#endif
  if (!data) return -1;
  if (startfrom<0||startfrom>=data->chars) return -1;
  startfrom=pos(begin,startfrom);
  if (startfrom<0) return -1;
  epos=pos(end,startfrom+1);
  if (epos<0) return -1;
  return startfrom;
}

l spstr::cpos(char cc,l startfrom) const {
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  if (!data) return -1;
  if (startfrom<0||startfrom>=data->chars) return -1;
  char *cp=strchr(data->buf+startfrom,cc);
  return cp?cp-data->buf:-1;
}

l spstr::crpos(char cc) const {
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  if (!data) return -1;
  char *cp=strrchr(data->buf,cc);
  return cp?cp-data->buf:-1;
}

l spstr::rpos(char const *ss) const {
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  if (!data) return -1;
  d len=strlen(ss);
  if (!len) return -1;
  char *res=NULL,*tmp=data->buf-len;
  do {
    tmp=strstr(tmp+len,ss);
    if (tmp) res=tmp;
  } while (tmp);
  return res?res-data->buf:-1;
}

spstr& spstr::upper() {
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  if (!data) return *this;
  _unique();
  for (l ii=0;ii<data->chars;ii++) {data->buf[ii]=TOUPPER(data->buf[ii]);}
  data->hash=FFFF;
  return *this;
}

spstr& spstr::lower() {
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  if (!data) return *this;
  _unique();
  for (l ii=0;ii<data->chars;ii++) {data->buf[ii]=TOLOWER(data->buf[ii]);}
  data->hash=FFFF;
  return *this;
}

spstr& spstr::capitalize() {
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  if (!data) return *this;
  _unique();
  if (data->chars>1) data->buf[0]=TOUPPER(data->buf[0]);
  data->hash=FFFF;
  return *this;
}

spstr& spstr::insert(const spstr &s1,l pos) {
#ifndef NO_SP_ASSERTS
  assert(!Lock&&!s1.Lock);
#endif
  return insert(s1(),pos);
}

spstr& spstr::insert(const char *ss,l pos) {
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  l len=strlen(ss);
  unique((data?data->chars:0)+len);
  if (pos<0) pos=0;
  if (pos>=data->chars) pos=data->chars; else
    memmove(data->buf+pos+len,data->buf+pos,data->chars-pos);
  memcpy(data->buf+pos,ss,len);
  data->chars+=len;
  *(data->buf+data->chars)=0;
  data->hash =FFFF;
  return *this;
}

#ifdef SP_BEBO
#define IsSpaceSeparator() (*(w*)separators==0x2000||*(w*)separators==0x0900|| \
  *(w*)separators==0x2009)
#else
#define IsSpaceSeparator() (*(w*)separators==0x0020||*(w*)separators==0x0009|| \
  *(w*)separators==0x0920)
#endif

l spstr::words(const char *separators) const {
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  if (!data||!data->chars) return 0; else {
    l count=0;
//    *(data->buf+data->chars)=0;
    Bool Zero=IsSpaceSeparator();
    char *str=data->buf;
    if (Zero) while (*str&&strchr(separators,*str)) str++;   // skip separators at start...
    if (!str) return 0;
    do {
      str=strpbrk(str,separators);
      if (str&&Zero) {
        while (strpbrk(str+1,separators)==str+1) str++; // skip same chars
      }
      count++;
    } while (str++);
    return count;
  }
}

spstr spstr::word(l ord, const char *separators) const {
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  if (!data||ord<=0||!data->chars) return spstr();
  char *prev,*str=data->buf;
//  *(data->buf+data->chars)=0;
  Bool Zero=IsSpaceSeparator();
  if (Zero) while (*str&&strchr(separators,*str)) str++;    // skip separators at start...
  if (!str) return spstr();
  str--;
  do {
    prev=++str;
    str=strpbrk(prev,separators);
    ord--;
    if (str&&Zero&&ord) while (strpbrk(str+1,separators)==str+1) str++; // skip same chars
  } while (str&&ord);
  if (!ord&&str!=prev) return spstr(*this,prev-data->buf, str?str-prev:0);
    else return spstr();
}

l spstr::word_Int(l ord,const char *separators) const {
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  if (!data||ord<=0||!data->chars) return 0;
  char *prev,*str=data->buf;
  Bool Zero=IsSpaceSeparator();
  if (Zero) while (*str&&strchr(separators,*str)) str++;    // skip separators at start...
  if (!str) return 0;
  str--;
  do {
    prev=++str;
    str=strpbrk(prev,separators);
    if (str&&Zero) while (strpbrk(str+1,separators)==str+1) str++; // skip same chars
    ord--;
  } while (str&&ord);
  if (!ord) return Int(prev-data->buf); else return 0;
}

d spstr::word_Dword(l ord,const char *separators) const {
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  if (!data||ord<=0||!data->chars) return 0;
  char *prev,*str=data->buf;
  Bool Zero=IsSpaceSeparator();
  if (Zero) while (*str&&strchr(separators,*str)) str++;    // skip separators at start...
  if (!str) return 0;
  str--;
  do {
    prev=++str;
    str=strpbrk(prev,separators);
    if (str&&Zero) while (strpbrk(str+1,separators)==str+1) str++; // skip same chars
    ord--;
  } while (str&&ord);
  if (!ord) return Dword(prev-data->buf); else return 0;
}

l spstr::wordpos(l ord, const char *separators) const {
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  if (!data||ord<=0||!data->chars) return -1;
  char *prev,*str=data->buf;
//  *(data->buf+data->chars)=0;
  Bool Zero=IsSpaceSeparator();
  if (Zero) while (*str&&strchr(separators,*str)) str++;    // skip separators at start...
  if (!str) return spstr();
  str--;
  do {
    prev=++str;
    str=strpbrk(prev,separators);
    if (str&&Zero) while (strpbrk(str+1,separators)==str+1) str++; // skip same chars
    ord--;
  } while (str&&ord);
  if (!ord) return prev-data->buf; else return -1;
}

spstr& spstr::replaceword(l ord,const char *separators,const char *str) {
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  l pos=wordpos(ord,separators);
  if (pos>=0) {
    if (word(ord,separators)!=str) {
      _unique();
      l len=strpbrk(data->buf+pos,separators)-data->buf-pos;
      if (len) del(pos,len);
      insert(str,pos);
    }
  }
  return *this;
}

spstr& spstr::trim() {
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  if (!data) return *this;
  l cc=0;
  while (*(data->buf+cc)==' '&&cc<data->chars) cc++;
  if (cc) {
    _unique();
    memmove(data->buf,data->buf+cc,data->chars-cc);
    data->chars-=cc;
    while (data->chars&&*(data->buf+data->chars-1)==' ') data->chars--;
    *(data->buf+data->chars)=0;
    data->hash=FFFF;
  } else {
    cc=data->chars;
    while (cc&&*(data->buf+cc-1)==' ') cc--;
    if (cc!=data->chars) {
      _unique();
      data->chars=cc;
      *(data->buf+cc)=0;
      data->hash=FFFF;
    }
  }
  return *this;
}

spstr& spstr::trimleft() {
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  if (!data) return *this;
  l cc=0;
  while (*(data->buf+cc)==' '&&cc<data->chars) cc++;
  if (cc) {
    _unique();
    memmove(data->buf,data->buf+cc,data->chars-cc);
    data->chars-=cc;
    *(data->buf+data->chars)=0;
    data->hash=FFFF;
  }
  return *this;
}

spstr& spstr::trimright() {
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  if (!data) return *this;
  l cc=data->chars;
  while (cc&&*(data->buf+cc-1)==' ') cc--;
  if (cc!=data->chars) {
    _unique();
    data->chars=cc;
    *(data->buf+cc)=0;
    data->hash=FFFF;
  }
  return *this;
}

spstr& spstr::expandtabs(int alignpos) {
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  if (!data) return *this;
  if (!data->chars) return *this;

  char *cp=strchr(data->buf,'\t');
  l ps=cp?cp-data->buf:-1;
  if (ps>=0) {
    _unique();
    data->hash=FFFF;
    do {
      data->buf[ps++]=' ';
      l len=RoundN(alignpos,ps)-ps;
      if (len) {
        if (data->chars+len>data->maxnow) reallocstr(data->chars+alignpos*4);

        memmove(data->buf+ps+len,data->buf+ps,data->chars-ps);
        memset(data->buf+ps,' ',len);
        data->chars+=len;
        data->buf[data->chars]=0;
      }
      cp=strchr(data->buf+ps+len,'\t');
      ps=cp?cp-data->buf:-1;
    } while (ps>=0);
  }
  return *this;
}

spstr& spstr::replace(const spstr &s1,const spstr &s2) {
#ifndef NO_SP_ASSERTS
  assert(!Lock&&!s1.Lock&&!s2.Lock);
#endif
  return replace(s1(),s2());
}

spstr& spstr::replace(const char *src,const char *dst) {
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  l ii=pos(src),len=strlen(src), dlen=strlen(dst);
  while (ii>=0) {
    del(ii,len);
    insert(dst,ii);
    ii=pos(src,ii+dlen);
  }
  return *this;
}

spstr& spstr::del(l pos,d len) {
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  if (data&&pos>=0&&pos<data->chars) {
    _unique();
    if ((d)pos+len>=(d)data->chars) data->chars=pos; else {
      memmove(data->buf+pos,data->buf+pos+len,(data->chars-len-pos)*sizeof(char));
      data->chars-=len;
    }
    *(data->buf+data->chars)=0;
    data->hash =FFFF;
  }
  return *this;
}

spstr& spstr::unquote(char quote) {
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  if (data&&data->chars>1)
    if (data->buf[0]==quote&&data->buf[data->chars-1]==quote) {
      _unique();
      memmove(data->buf,data->buf+1,(data->chars-2)*sizeof(char));
      data->chars-=2;
      *(data->buf+data->chars)=0;
      if (data->hash!=FFFF) data->hash-=d(quote)*2;
    }
  return *this;
}

spstr& spstr::quote(char quote) {
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  if (!data||data->chars<2||data->buf[0]!=quote||data->buf[data->chars-1]!=quote) {
    _unique();
    unique((data?data->chars:0)+2);
    if (data->chars)
      memmove(data->buf+1,data->buf,data->chars*sizeof(char));
    data->chars+=2;
    data->buf[0]=quote;
    data->buf[data->chars-1]=quote;
    *(data->buf+data->chars)=0;
    if (data->hash!=FFFF) data->hash+=d(quote)*2;
  }
  return *this;
}

spstr spstr::operator() (l __pos,l __len) const {
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  return data&&__len>0&&__pos>=0&&__pos<data->chars?spstr(*this,__pos,__len):spstr();
}

l spstr::Int(l __pos) const {
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  int res=0;
  if (data&&data->chars-__pos>0) {
//    *(data->buf+data->chars)=0;
    char *ptr=data->buf+__pos;              // prevent octal conversion
    while (*ptr&&*ptr==' ') ptr++;
    while (*ptr=='0'&&ptr[1]>='0'&&ptr[1]<='9') ptr++;
    res=strtol(ptr,NULL,0);
  }
  return res;
}

d spstr::Dword(l __pos) const {
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  d res=0;
  if (data&&data->chars-__pos>0) {
//    *(data->buf+data->chars)=0;
    char *ptr=data->buf+__pos;              // prevent octal conversion
    while (*ptr&&*ptr==' ') ptr++;
    while (*ptr=='0'&&ptr[1]>='0'&&ptr[1]<='9') ptr++;
    res=strtoul(ptr,NULL,0);
  }
  return res;
}

#if defined(__LONGLONG_SUPPORT) && defined(STRTOUL64)
s64 spstr::Int64(l __pos) const {
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  s64 res=0;
  if (data&&data->chars-__pos>0) {
    char *ptr=data->buf+__pos;              // prevent octal conversion
    while (*ptr&&*ptr==' ') ptr++;
    while (*ptr=='0'&&ptr[1]>='0'&&ptr[1]<='9') ptr++;
    res=STRTOL64(ptr,NULL,0);
  }
  return res;
}

u64 spstr::Dword64(l __pos) const {
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  u64 res=0;
  if (data&&data->chars-__pos>0) {
    char *ptr=data->buf+__pos;              // prevent octal conversion
    while (*ptr&&*ptr==' ') ptr++;
    while (*ptr=='0'&&ptr[1]>='0'&&ptr[1]<='9') ptr++;
    res=STRTOUL64(ptr,NULL,0);
  }
  return res;
}
#endif // __LONGLONG_SUPPORT

float spstr::Float(l __pos) const {
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  float res=0;
  if (data&&data->chars-__pos>0) {
//    *(data->buf+data->chars)=0;
    sscanf(data->buf+__pos,"%f",&res);
  }
  return res;
}

char *spstr::LockPtr(l Size) {
  if (!Lock) {
    unique(Size);
    Lock=1;
  } else
  if (Size>data->maxnow) return NULL; else Lock++;
  return data->buf;
}

void spstr::UnlockPtr() {
  if (Lock>0)
    if (--Lock==0) {
      data->chars=strlen(data->buf);
      data->hash =FFFF;
    }
}

Bool spstr::UnlockPtr(l Len) {
  if (Lock<=0||Len<0||Len>=data->maxnow) return 0;
  if (--Lock==0) {
    data->chars=Len;
    data->hash =FFFF;
    data->buf[Len]=0;
  }
  return true;
}

spstr spstr::left(l count) const {
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  if (count&&data) return spstr(*this,0,count);
    else return spstr();
}

spstr spstr::right(l count) const {
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  if (!data) return spstr();
  l from=data->chars-count;
  from=range(from,0,data->chars);
  return spstr(*this,from,data->chars-from);
}

d spstr::calchash() const {
  if (!data) return 0;
  register d value=0;
  register char *cc=data->buf;
  while (*cc) value+=d(*cc++);
  return data->hash=value;
}

d spstr::countchar(char chr) const {
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  if (!data) return 0;
  if (!data->chars) return 0;
  d cnt=0;
  char *ch=strchr(data->buf,chr);
  while (ch++) {
    cnt++;
    ch=strchr(ch,chr);
  }
  return cnt;
}

spstr& spstr::replacechar(char from,char to) {
#ifndef NO_SP_ASSERTS
  assert(!Lock);
#endif
  if (!data) return *this;
  if (!data->chars) return *this;
  if (from==to) return *this;
  _unique();
  char *ch=strchr(data->buf,from);
  while (ch) {
    *ch++=to;
    if (*ch!=from) ch=strchr(ch,from);
  }
  return *this;
}

