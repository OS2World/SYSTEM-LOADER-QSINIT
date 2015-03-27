//
// QSINIT
// 32-bit part common functions
//
#ifndef QS_EFI_CALLS
#define QS_EFI_CALLS

#include "qstypes.h"
#include "efnlist.h"

#ifdef __cplusplus
extern "C" {
#endif

/** call 64-bit function.
    Bits in arg8mask - in order of numbering, i.e. bit 0 - arg 1, bit 1 - arg 2, etc.
    @param function  Function index
    @param arg8mask  Bit mask for arguments. Use 1 for 8 bit value, 0 for 1/2/4-bit
    @param argcnt    Number of arguments
    @return function result (from eax) */
u32t _std call64 (int function, u32t arg8mask, u32t argcnt, ...);

/** call 64-bit function.
    @param function  Function index
    @param arg8mask  Bit mask for arguments. Use 1 for 8 bit value, 0 for 1/2/4-bit
    @param argcnt    Number of arguments
    @return function result (from rax, returned in edx:eax) */
u64t _std call64l(int function, u32t arg8mask, u32t argcnt, ...);

// include efnlist.inc to get access
#ifdef QS_FN64_INDEX
/** 64-bit exception common handler function.
    Function can show trap screen or/and modify some values and return.
    @param xdata     64-bit registers (data can be modified). */
typedef void _std (*xcpt64proc)(struct xcpt64_data *xdata);

/** set common 64-bit exception handler.
    @param cxh    Function index
    @return return previous handler (or 0 if no one) */
xcpt64proc _std sys_setxcpt64(xcpt64proc cxh);
#endif

#ifdef __cplusplus
}
#endif
#endif // QS_EFI_CALLS
