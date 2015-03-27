//
//  strings
//
//  (c) dixie/SP 1997-2013. Freeware.
//
#ifndef _SP_STRINGS_INCLUDED
#define _SP_STRINGS_INCLUDED

#ifndef __cplusplus
#error strings.hpp is for use with C++
#endif

#include "sp_defs.h"
#include <stdlib.h>
#ifndef NO_SP_ASSERTS
#include <assert.h>
#endif

typedef struct {
      l    chars;
      l   maxnow;
      l   usecnt;
      d     hash;
      char buf[1];
     } strinfo;
typedef strinfo *pstrinfo;

extern char *spnullstr;

class  spstr {
private:
  strinfo *data;
  void reallocstr(l newsize) {                // realloc memory
    if (!data||data->maxnow<=newsize) {
      data=data?(pstrinfo)REALLOC(data,newsize+sizeof(strinfo)+1):
                (pstrinfo)MALLOC(newsize+sizeof(strinfo)+1);
      data->maxnow=newsize;
    }
  }
  void allocstr(l size) {                     // alloc new string
    reallocstr(size+1);
    data->chars =0;
    data->hash  =FFFF;
    data->buf[0]=0;
    data->usecnt=1;
  }
  void disposestr(void) { if (data) FREE(data); data=NULL; }  // free string
  void initstring(char const *str,d __n=0);    // from C string
  spstr &tofloat(float const ff,d digits=15);
  d calchash() const;
  d  Lock;
  void _unique();
public:
// constructors
  spstr() { data=NULL; Lock=0; }
  spstr(int FromInt) { data=NULL; Lock=0; operator= ((int)FromInt); }
  spstr(l FromInt)   { data=NULL; Lock=0; operator= ((int)FromInt); }
  spstr(d FromDword) { data=NULL; Lock=0; operator= (FromDword); }
  spstr(float FromFloat) { data=NULL; Lock=0; operator= (FromFloat); }
  spstr(const a &avalue) { data=NULL; Lock=0; operator= (avalue); }
  spstr(spstr const &,                        // from another string
          d __pos=0,                          // from __pos, first __n chars
          d __n=0);
  spstr(char []);                             // from C string, first __n chars
  spstr(char const*, l __n=0);                // --'--'--
  spstr(char,d __rep=1);                      // from single character

  ~spstr() { clear(); }                       // destructor

  void clear() {
#ifndef NO_SP_ASSERTS
    assert(!Lock);
#endif
    if (data&&--data->usecnt==0) disposestr();
    data=NULL;
  }

// conversion operators
  const char *operator()() const {            // to C string
    return data?data->buf:spnullstr;
  }
  const char *c() const { return operator()();}    // to C string
  operator char () const
    { return data?data->buf[0]:*spnullstr; }  // to single character
  operator l() const { return Int(); }
  operator d() const { return Dword(); }
  operator a() const;

  spstr& sprintf(const char *fmt,...);       // replace old contents with string...

// assignment operators
  spstr& operator= (spstr const& );          // from another string
  spstr& operator= (char const* );           // from a C string
  spstr& operator= (int const );             // from an integer
  spstr& operator= (d const );               // from an dword
  spstr& operator= (float const ff)          // from a float
    { return tofloat(ff); }
  spstr& operator= (a const& );              // from an ApplyValue

// append operators
  spstr& operator+= (spstr const& );         // a string
  spstr& operator+= (char const* );          // a C string
  spstr& operator+= (l);                     // long
  inline
  spstr& operator+= (int ii)                 // int
    { return operator+=(l(ii)); }
  spstr& operator+= (char);                  // char

  spstr operator() (l __pos,l __len) const;  // substring operator

  char& operator[] (l __pos);                // reference-to-char operator
  // return last character
  char lastchar() const { return data?(data->chars?data->buf[data->chars-1]:0):0; }

  l Int(l __pos=0) const;                 // try to read int
  d Dword(l __pos=0) const;               // try to read dword
  float Float(l __pos=0) const;           // try to read float
#if defined(__LONGLONG_SUPPORT) && defined(STRTOUL64)
  s64 Int64(l __pos=0) const;             // try to read long long
  u64 Dword64(l __pos=0) const;           // try to read qword
#endif
  char *LockPtr(l Size=256);              // lock pointer
  void UnlockPtr();                       // unlock pointer
  Bool UnlockPtr(l Len);                  // unlock with size (MFC-like)

// comparison operators
  friend Bool operator== (spstr const&,spstr const&);   // string == string?
  friend Bool operator== (spstr const&,char const*);    // string == C string?
  friend Bool operator== (char const*,spstr const&);    // C string == string?
  friend Bool operator== (spstr const&,char);           // string == character?
  friend Bool operator== (char,spstr const&);           // character == string?

  friend Bool operator!= (spstr const &,spstr const &); // string != string?
  friend Bool operator!= (spstr const &,char const* );  // string != C string?
  friend Bool operator!= (char const *,spstr const &);  // C string != string?
  friend Bool operator!= (spstr const &, char);         // string != character?
  friend Bool operator!= (char, spstr const &);         // character != string?

// concatenate string operators
  friend spstr operator+ (spstr const&,spstr const&);  // string + string
  friend spstr operator+ (spstr const&,char const*);   // string + C string
  friend spstr operator+ (char const*,spstr const&);   // C string + string
  friend spstr operator+ (spstr const&,char);          // string + character
  friend spstr operator+ (char,spstr const &);         // character + string

                                               // input/output operators
// Member functions:
  l length() const { return data?data->chars:0; }      // length of string

// find substring loc
  l pos(spstr const &,l startfrom=0) const;    // search string is a string
  l pos(char const *,l startfrom=0) const;     // search string is a C string
  l rpos(char const *) const;                  // search string from right
  l cpos(char,l startfrom=0) const;            // search char
  l crpos(char) const;                         // search char from right

  l bepos(const char *begin,const char *end,l &epos,l startfrom=0) const;
                                               // search for string by it
                                               // begin and end
// letter case conversion & misc
  spstr& upper();                             // to UPPER CASE
  spstr& lower();                             // to lower case
  spstr& capitalize();                        // to Lower Case
  spstr& trim();                              // remove both sides spaces
  spstr& trimleft();                          // remove start spaces
  spstr& trimright();                         // remove trailing spaces
  spstr& expandtabs(int alignpos=8);          // tabs -> spaces
  spstr& replace(const spstr&,const spstr&);  // replace
  spstr& replace(const char*,const char*);    // replace
  spstr& insert(const spstr&,l pos);          // insert string
  spstr& insert(const char*,l pos);           // insert string
  spstr& del(l pos,d len=1);                  // delete substring
  spstr& dellast() { return del(length()-1); }
  spstr  deltostr(l pos,d len);               // del to new string
  spstr& unquote(char quote='\"');            // unquote string
  spstr& quote(char quote='\"');              // quote string if not quoted
  void unique(d len=0);                       // make string unique
                                              // with buffer lenght at
                                              // least == len
  l words(const char *separators=" \x9") const;
  l wordpos(l ord,const char *separators=" \x9") const;
  spstr word(l ord,const char *separators=" \x9") const;
  l word_Int(l ord,const char *separators=" \x9") const;
  d word_Dword(l ord,const char *separators=" \x9") const;
  spstr& replaceword(l ord,const char *separators,const char *str);

  spstr left(l count) const;
  spstr right(l count) const;

  d countchar(char chr) const;               // count number of this chars
  spstr& replacechar(char from,char to);     // fast replace for chars

  d hash() const { return data?(data->hash!=FFFF?data->hash:calchash()):0; }

  Bool operator! () const { return data?!data->chars:1; }  // zero string?
  spstr& Init(l size=12)
    { data=NULL; Lock=false; allocstr(size); return *this; } // constructor
                                                  // do not call directly!!
};

#if !defined(__BORLANDC__)
#define string spstr
#endif

typedef const spstr cspstr;

#endif
