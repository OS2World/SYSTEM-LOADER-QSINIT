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

/** 64-bit exception common handler function.
    Function can show trap screen or/and modify some values and return.
    Interrupts is disabled on entering this function.
    Function executed on static non-reenterable stack, provided by 64-bit part.
    @param xdata     64-bit registers (data can be modified). */
typedef void _std (*xcpt64proc)(struct xcpt64_data *xdata);

/** set common 64-bit exception handler.
    @param cxh    Exception handler function.
    @return return previous handler (or 0 if no one) */
xcpt64proc _std sys_setxcpt64(xcpt64proc cxh);

/** set 64-bit timer irq handler.
    @param tmi    Timer interrupt handler function.
    @return return previous handler (or 0 if no one) */
xcpt64proc _std sys_tmirq64(xcpt64proc tmi);

// include qsint.h to get access
#ifdef QSINIT_HIDFUNCS
/** function calls EFI`s ExitBootServices() and return memory map after it.
    exit_prepare() must be called before this function or it will be called
    during it.

    Most of complex API calls would fail (or hang) after this call (due lack
    of EFI boot services).

    QSINIT memory should be aligned to the top of 4th Gb. Size may vary from
    32Mb to ~2Gb. User must preserve this memory, because it executing in it
    just now.

    Be careful, this function moves GDT!

    @param [out] loadermem   loader memory start address
    @param [out] loadersize  loader memory size

    @return PC memory table (after EFI boot services terminaion), with zero
            length in the last entry, memory block must be free()-ed */
AcpiMemInfo* _std exit_efi(void **loadermem, u32t *loadersize);
#endif

#ifdef __cplusplus
}
#endif
#endif // QS_EFI_CALLS
