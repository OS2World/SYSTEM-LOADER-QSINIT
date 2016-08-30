//
// QSINIT
// codepages lightweight support
//
#ifndef CODEPAGE_LIB
#define CODEPAGE_LIB

#include "qstypes.h"
#include "qsclass.h"
#include "wchar.h"

/** "codepages" shared class.
    Instance-driven codepage access.
    Usage:
    @code
       qs_cpconvert cvt = NEW(qs_cpconvert);
       cvt->setcp(cvt->getsyscp());
       printf("%C\n", cvt->convert('a',1));
       DELETE(cvt);
    @endcode */
typedef struct qs_cpconvert_s {
   /** set conversion code page.
      Default page after instance creation - current active code page (if
      it was selected).
      @return success flag (1/0) */
   u32t    _exicc (*setcp   )(u16t cp);
   /** converted character.
       @param  chr      character code to be converted
       @param  dir      0: Unicode to OEMCP, 1: OEMCP to Unicode
       @return zero on error */
   wchar_t _exicc (*convert )(wchar_t chr, int dir);
   /// OEM codepage toupper
   char    _exicc (*toupper )(char chr);
   /// OEM codepage tolower
   char    _exicc (*tolower )(char chr);
   /** get supported code pages list.
       @return null-terminated list of supported code pages. List must be
       freed by free() call. */
   u16t*   _exicc (*cplist  )(void);
   /* query current system code page.
      @return active code page or 0 if not selected */
   u16t    _exicc (*getsyscp)(void);
   /* set system code page.
      Function simulate to CHCP command.
      @return success flag (1/0) */
   u32t    _exicc (*setsyscp)(u16t cp);
   /** query 128 byte uppercase table.
       @param  cp       codepage number to query, 0 for current
       @return pointer or zero on error */
   u8t*    _exicc (*uprtab  )(u16t cp);
   /// unicode tolower
   wchar_t _exicc (*towlower)(wchar_t chr);
   /// unicode toupper
   wchar_t _exicc (*towupper)(wchar_t chr);
} _qs_cpconvert, *qs_cpconvert;

#endif // CODEPAGE_LIB
