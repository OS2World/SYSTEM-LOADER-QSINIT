//
// QSINIT
// bit map shared class
//
#ifndef QSINIT_BITCLASS
#define QSINIT_BITCLASS

#include "qstypes.h"
#include "qsclass.h"

/** "bit map" shared class.
    Usage:
    @code
       bit_map bmp = NEW(bit_map);
       bmp->alloc(1000);
       bmp->set(1, 512, 10);
       DELETE(bmp);
    @endcode */
typedef struct bit_map_s {
   /** init bitmap with external pointer.
       Init data is unchanged unlike alloc() call.
       @param  bmp      pointer to bitmap data, will be used by class code!
       @param  size     size of bitmap (in bits!)
       @return zero on error (invalid parameter) */
   u32t    _exicc (*init   )(void *bmp, u32t size);
   /** init bitmap with internally allocated buffer.
       Bitmap will be zeroed after allocation, buffer size aligned to dword.
       @param  size     size of bitmap (in bits!, zero to free buffer).
       @return zero on error (no memory) */
   u32t    _exicc (*alloc  )(u32t size);
   /** change bitmap size.
       Function will fail on trying to expand external buffer.
       @param  size     new size of bitmap (zero to free buffer).
       @param  on       state of expanded space (1/0)
       @return zero on error (no memory or external buffer expansion). */
   u32t    _exicc (*resize )(u32t size, int on);
   /// return bitmap size (in bits!).
   u32t    _exicc (*size   )(void);
   /// return bitmap buffer
   u8t*    _exicc (*mem    )(void);
   /** set entire bitmap.
       @param  on       set value (1/0) */
   void    _exicc (*setall )(int on);
   /** set bits.
       @param  on       set value (1/0)
       @param  pos      start position
       @param  length   number of bits to set
       @return number of actually changed bits */
   u32t    _exicc (*set    )(int on, u32t pos, u32t length);
   /** check area for value.
       @param  on       check value (1/0)
       @param  pos      start position
       @param  length   number of bits to check
       @return 1 on full match, 0 if any difference is present */
   u32t    _exicc (*check  )(int on, u32t pos, u32t length);
   /** search for contiguous area.
       @param  on       search value (1/0)
       @param  length   number of bits to search
       @param  hint     position where to start
       @return position of found area or 0xFFFFFFFF if no one */
   u32t    _exicc (*find   )(int on, u32t length, u32t hint);
   /** search and invert contiguous area.
       @param  on       search value (1/0)
       @param  length   number of bits to search
       @param  hint     position where to start
       @return position of found area or 0xFFFFFFFF if no one */
   u32t    _exicc (*findset)(int on, u32t length, u32t hint);
   /** search for longest contiguous area.
       @param  [in]  on      search value (1/0)
       @param  [out] start   pointer to found position
       @return length of area or zero if no one */
   u32t    _exicc (*longest)(int on, u32t *start);
   /** return total number of set or cleared bits.
       @param  on       search value (1/0)
       @return calculated value */
   u32t    _exicc (*total  )(int on);
   /// return current instance state (BMS_* flags set).
   u32t    _exicc (*state  )(void);
} _bit_map, *bit_map;

/// @name bitmap state
//@{
#define BMS_EXTMEM       0x0001  ///< bitmap uses external memory
#define BMS_EMPTY        0x0002  ///< bitmap is empty (no memory allocated)
//@}

#endif // QSINIT_BITCLASS
