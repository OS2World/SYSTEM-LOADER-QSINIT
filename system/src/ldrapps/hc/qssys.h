//
// QSINIT API
// various system calls
//
#ifndef QSINIT_SYS_FUNCS
#define QSINIT_SYS_FUNCS

#ifdef __cplusplus
extern "C" {
#endif

#include "qsutil.h"

/// @name result of sys_getint()
//@{
#define SINT_TASKGATE    1            ///< vector is task gate
#define SINT_INTGATE     2            ///< vector is interrupt gate
#define SINT_TRAPGATE    3            ///< vector is trap gate
//@}

/** get interrupt vector.
    BIOS host returns 48-bit far pointer for interrupt and trap gates
    and 16-bit tss selector for task gate.
    EFI host returns 64-bit flat pointer.

    @param   [in]  vector  Vector (0..255)
    @param   [out] addr    Buffer for address
    @return vector type (SINT_*) or 0 if failed */
u32t     _std sys_getint(u8t vector, u64t *addr);

/** set interrupt vector.
    BIOS host uses 48-bit far pointer.
    EFI host uses 64-bit flat pointer.

    @param   [in]  vector  Vector (0..255)
    @param   [in]  addr    Buffer with address
    @param   [in]  type    Interrupt type, only SINT_INTGATE and SINT_TRAPGATE
                           accepted here or 0 to use default type (default means
                           interrupt gate for PIC vectors only).
    @return boolean success flag (1/0) */
int      _std sys_setint(u8t vector, u64t *addr, u32t type);

/** set task gate interrupt handler.
    Function is not supported on EFI host (always return 0).
    @param   [in]  vector  Vector (0..255)
    @param   [in]  sel     TSS selector
    @return boolean success flag (1/0) */
int      _std sys_intgate(u8t vector, u16t sel);

/** allocate TSS selector.
    Function allocates selector pair - TSS and writeable TSS+8 for
    easy access.
    @param   tssdata       TSS data (must not cross page boundary, as Intel says)
    @param   limit         TSS limit (use 0 for default)
    @return  tss selector or 0 on error. */
u16t     _std sys_tssalloc(void *tssdata, u16t limit);

/** free TSS selector.
    If next selector have the same base and limit - it will be
    released too.
    Function will fail if TSS is current task or linked to the current task.
    @param   sel           TSS selector
    @return success flag (1/0) */
int      _std sys_tssfree(u16t sel);

/** GDT descriptor setup.
    This function get ready descriptor data (unlike hlp_selsetup()), but
    accept only QSINIT's selectors too (i.e. you cannot change selectors
    of EFI BIOS by this call).
    @param  sel       GDT offset (i.e. selector with 0 RPL field)
    @param  desc      Descriptor data (8 bytes)
    @return bool - success flag. */
int      _std sys_seldesc(u16t sel, void *desc);

/** read GDT descriptor to supplied buffer.
    Function return actual GDT data and fail only on reaching GDT limit.

    @param  sel       GDT offset (i.e. selector with 0 RPL field)
    @param  desc      Buffer for Descriptor data (8 bytes)
    @return bool - success flag. */
int      _std sys_selquery(u16t sel, void *desc);

/** enable/disable interrupts.
    It is good to call it with saving of previous state:
@code
    int state = sys_intstate(0);
    // do something...
    sys_intstate(state);
@endcode
    @param   on      State to set (enable = 1/disable = 0)
    @return previous state. */
int      _std sys_intstate(int on);

/** query current working mode (paging/flat).
    @return 1 if paging mode active, else 0 */
int      _std sys_pagemode(void);

/** query current active mode (32/64).
    Function must always return 1 on EFI host and 0 on BIOS. But it
    checks corresponding registers on every call.
    @return 1 if system is in 64-bit mode, else 0 */
int      _std sys_is64mode(void);

/** get Local APIC address.
    Despite to constant LAPIC address, this function still usable, because
    it guarantee check for LAPIC presence and its mapping in paging mode.
    @return address or 0 on error */
void*    _std sys_getlapic(void);

/** query ACPI RSDP physical address.
    Note, that in UEFI result will point to the top of RAM.
    @return physicall address or 0 on error */
u32t     _std sys_acpiroot(void);

/// query FPU state buffer size
u32t     _std fpu_statesize(void);

/** save FPU state.
    Buffer address must be aligned to 64 bytes. */
void     _std fpu_statesave(void *buffer);

/** restore FPU state.
    Buffer address must be aligned to 64 bytes. */
void     _std fpu_staterest(void *buffer);

/// @name bit flags for sys_isavail
//@{
#define SFEA_PAE     0x00000001            ///< PAE supported
#define SFEA_PGE     0x00000002            ///< PGE supported
#define SFEA_PAT     0x00000004            ///< PAT supported
#define SFEA_CMOV    0x00000008            ///< CMOV instructions (>=P6)
#define SFEA_MTRR    0x00000010            ///< MTRR present
#define SFEA_MSR     0x00000020            ///< MSRs present
#define SFEA_X64     0x00000040            ///< AMD64 present
#define SFEA_CMODT   0x00000080            ///< clock modulation supported
#define SFEA_LAPIC   0x00000100            ///< Local APIC available
#define SFEA_FXSAVE  0x00000200            ///< FXSAVE available
#define SFEA_XSAVE   0x00000400            ///< XSAVE available
#define SFEA_SSE1    0x00000800            ///< SSE available
#define SFEA_SSE2    0x00001000            ///< SSE2 available
#define SFEA_INVM    0x08000000            ///< running in virtual machine
#define SFEA_INTEL   0x10000000            ///< CPU is Intel
#define SFEA_AMD     0x20000000            ///< CPU is AMD
//@}

/** query some CPU features in more friendly way.
    @param  flags   SFEA_* flags combination
    @return actually supported subset of flags parameter */
u32t     _std sys_isavail(u32t flags);

typedef struct {
   u64t      start;                 ///< start of memory block
   u32t      pages;                 ///< number of 4k pages in block
   u32t      flags;                 ///< block flags
} pcmem_entry;

#define PCMEM_FREE        0x0000    ///< free block
#define PCMEM_RESERVED    0x0001    ///< memory, reserved by hardware (BIOS)
#define PCMEM_QSINIT      0x0002    ///< memory, used by QSINIT memory manager
#define PCMEM_RAMDISK     0x0003    ///< memory, used by ram disk app
#define PCMEM_USERAPP     0x0005    ///< memory, used by any custom app

#define PCMEM_HIDE        0x8000    ///< add this flag to hide memory from OS/2
#define PCMEM_TYPEMASK    0x7FFF

/** query PC memory above 1Mb (including entries above 4Gb limit).
    @param   freeonly   Flag to return only free memory blocks.
    @return pcmem_entry array with pages=0 in last entry. Entry must be free()-d. */
pcmem_entry * _std sys_getpcmem(int freeonly);

/** return number of free pages, available on PC initially.
    @param [out] above4gb   Number of free pages, available above 4Gb border.
    @return number of free pages (4kb), available above 4Gb border. */
u32t     _std sys_ramtotal(u32t *above4gb);

/** mark PC memory as used.
    This is NOT a malloc call. This function manages PC memory (for
    information reason, basically - and resolving possible conflicts).
    QSINIT itself - uses only blocks marked as PCMEM_QSINIT in this list.
    Other FREE memory is available for any kind of use.

    OS/2 boot will use only memory, not marked by PCMEM_HIDE flag, so
    you can hide an existing block of physical memory here.

    @param   address    Start address (64 bit).
    @param   pages      Number of 4k pages.
    @param   owner      PCMEM_* constant (except PCMEM_FREE).
    @return 0 on success or error code: EINVAL on non-aligned address, ENOENT
            if range is not intersected with PC memory and EACCES if one of
            touched blocks is used */
u32t     _std sys_markmem(u64t address, u32t pages, u32t owner);

/** unmark PC memory.
    Only PCMEM_RAMDISK..PCMEM_USERAPP entries can be unmapped.

    @param   address    Start address (64 bit).
    @param   pages      Number of 4k pages.
    @return success flag (1/0) */
u32t     _std sys_unmarkmem(u64t address, u32t pages);

/** function return end of RAM in 1st 4Gb of memory.
    There is no mapped pages in paging mode beyond this border by default
    @return page aligned address */
u32t     _std sys_endofram(void);

/** memcpy for memory above 4Gb border.
    @attention Function will turn PAE mode on if requested address above 4Gb.

    Copying protected by exception handler.
    Page 0 cannot be a part of source or destination.
    Both source and destination cannot cross 4Gb border.
    Use hlp_memcpy() for protected copying in first 4Gbs / with
    page 0 in source/destination.

    @param   dst        Destination physical address.
    @param   src        Source physical address.
    @param   length     Number of bytes to copy
    @return success flag (1/0) */
u32t     _std sys_memhicopy(u64t dst, u64t src, u64t length);

/** query QSINIT internal parameters.
    @param   index      Parameter index (QSQI_*).
    @param   outptr     Ptr to buffer for string values, can be 0
                        to query size only.
    @return size of parameter in "outptr" or parameter value
            (depends on index) or 0 on invalid index */
u32t     _std sys_queryinfo(u32t index, void *outptr);

/// index values for sys_queryinfo()
//@{
#define QSQI_VERSTR       0x0000    ///< version string
#define QSQI_OS2LETTER    0x0001    ///< boot drive letter from bpb or 0
#define QSQI_OS2BOOTFLAGS 0x0002    ///< OS/2 boot flags (BF_*, defined in filetab.inc)
#define QSQI_CPUSTRING    0x0003    ///< CPU name string (decoded from cpuid)
#define QSQI_TLSPREALLOC  0x0010    ///< number of constantly used TLS entries
//@}

/// info record for SECB_CPCHANGE event
typedef struct {
   /// code page number
   u16t    cpnum;
   /// OEM-Unicode bidirectional conversion
   u16t _std (*convert)(u16t src, int to_unicode);
   /// unicode upper-case conversion
   u16t _std (*wtoupper)(u16t src);
   /// test if the character is DBC 1st byte
   int  _std (*dbc_1st)(u8t chr);
   /// test if the character is DBC 2nd byte
   int  _std (*dbc_2nd)(u8t chr);
   /// uppercase table for high 128 OEM chars (should be copied, zero for dbcs pages)
   u8t   *oemupr;
} codepage_info;

typedef struct {
   u32t   eventtype;
   union {
      /** info field value:
         SECB_IODELAY  - new IODelay value
         SECB_DISKADD  - disk handle of added disk
         SECB_DISKREM  - disk handle of removing disk
         SECB_CPCHANGE - codepage_info*
         SECB_HOTKEY   - key code in low word and keyboard status in high word.
         SECB_SESTART  - session number
         SECB_SEEXIT   - session number
         SECB_DEVADD   - device id
         SECB_DEVDEL   - device id
         SECB_SETITLE  - session number
      info2 field value:
         SECB_HOTKEY   - device number, where key was pressed */
      struct {
         u32t     info;
         u32t    info2;
         u32t    info3;
      };
      struct {
         void    *data;
         void   *data2;
         void   *data3;
      };
   };
} sys_eventinfo;

/** callback procedure for sys_notifyevent().
    Without SECB_THREAD flag - callback called asynchronously, in other
    process/thread context and in locked MT state! 

    With SECB_THREAD flag - callback called in separate thread (with 16k
    stack). Note, that for deletion events (SECB_DISKREM, SECB_DEVDEL) -
    instance will be removed already at the moment of thread activation.
    Only absence of SECB_THREAD guarantee it presence during call. */
typedef void _std (*sys_eventcb)(sys_eventinfo *info);

/** register callback procedure for selected system events.
    By default, callback function marked as "current process" owned. After
    process exit it will be removed automatically.
    To use it in global modules (DLLs) - SECB_GLOBAL bit should be added.

    SECB_QSEXIT, SECB_PAE and SECB_MTMODE are single-shot events - all
    such notification types will be unregistered automatically after call.
    I.e., if mask combines SECB_PAE|SECB_MTMODE (for example), function
    will be called for every event and only then unregistered.

    SECB_THREAD flag is accepted even if MT mode is off, callback just failed
    to run if MT mode will be off when event occurs. I.e. you can install
    new thread launch at the moment of MT mode activation (for example).

    SECB_THREAD flag cannot be combined with SECB_QSEXIT or SECB_CPCHANGE.
    For SECB_DISKREM only absence of SECB_THREAD flag guarantee disk presence
    at the time of call. The same is true for device in SECB_DEVDEL.

    SECB_HOTKEY bit is not accepted here, use sys_sethotkey().

    Note, that single-shot notifications like SECB_PAE & SECB_MTMODE are
    never called if event was signaled before sys_notifyevent() call.

    Thread (with SECB_THREAD) will be created in caller process context,
    unless SECB_GLOBAL is used. For global notifications thread created in
    pid 1 (system process) context. Thread stack size is fixed to be 16k.

    @param   eventmask  Bit mask of events to call cbfunc (SECB_*).
    @param   cbfunc     Callback function, one address can be specified only
                        once, further calls only changes the mask, use mask 0
                        to remove callback at all.
    @return success flag (1/0) */
u32t     _std sys_notifyevent(u32t eventmask, sys_eventcb cbfunc);

/// event notification event types and flags
//@{
#define SECB_QSEXIT   0x00000001    ///< QSINIT exit
#define SECB_PAE      0x00000002    ///< PAE page mode activated
#define SECB_MTMODE   0x00000004    ///< MTLIB activated (multithreading)
#define SECB_CMCHANGE 0x00000008    ///< CPU clock modulation changed
#define SECB_IODELAY  0x00000010    ///< IODelay value changed (value in info field)
#define SECB_DISKADD  0x00000020    ///< new disk added (disk handle in info field)
#define SECB_DISKREM  0x00000040    ///< disk removing (disk handle in info field)
#define SECB_CPCHANGE 0x00000080    ///< code page changed (codepage_info* in data)
#define SECB_LOWMEM   0x00000200    ///< insufficient of memory
#define SECB_SESTART  0x00000400    ///< new session start (sesno in info)
#define SECB_SEEXIT   0x00000800    ///< session has finished (sesno in info)
#define SECB_DEVADD   0x00001000    ///< vio device added (dev id in info)
#define SECB_DEVDEL   0x00002000    ///< vio device deleting (dev id in info)
#define SECB_SETITLE  0x00004000    ///< session title changed (sesno in info)

#define SECB_HOTKEY   0x00000100    ///< sys_sethotkey() callback (key code in info field)

#define SECB_THREAD   0x40000000    ///< run callback in a new thread
#define SECB_GLOBAL   0x80000000    ///< global callback (see sys_notifyevent())
//@}

/** install/remove system hot-key callback.
    Function can be called multiple times for the same callback function
    with new "code" value, every call will *add* code to the list.

    Callback will recieve sys_eventinfo record with SECB_HOTKEY in "eventtype",
    key code in "info" field and number of source device, where key was
    pressed in "info2" field.

    Callback enumeration for hot-key processing is not intersected with
    sys_notifyevent() events, but the same callback function can be used for
    both cases simultaneously.

    @param   code       Key value (will be removed from the user queue) or
                        0 to remove all hot-keys for this function.
    @param   statuskeys Keyboard status bits (KEY_SHIFT, KEY_ALT, KEY_CTRL)
    @param   flags      Only SECB_THREAD and SECB_GLOBAL accepted here.
    @param   cbfunc     Callback function.
    @return error code or 0 */
qserr    _std sys_sethotkey  (u16t code, u32t statuskeys, u32t flags,
                              sys_eventcb cbfunc);

/// flags for hlp_setupmem()
//@{
#define SETM_SPLIT16M     0x0001    ///< split memory block at 16M border
//@}

/** setup system memory usage (for OS/2 boot basically).
    Result will be written into physmem array (can be readed from storage
    key with STOKEY_PHYSMEM constant name).
    This call does not affect QSINIT memory use, it only calculates address
    and size of usable memory blocks below 4Gb to use by OS/2 boot process.

    @param [in]     fakemem    memory limit size, Mb (or 0)
    @param [in,out] logsize    ptr to doshlp log size, in 64kb blocks (or 0)
    @param [in]     removemem  zero-term list of block's heading address to
                               remove from use (or 0)
    @param [in]     flags      flags
    @return return physical log address (or 0 if no log or failed to alloc)
    and corrected log size in *logsize variable */
u32t     _std hlp_setupmem(u32t fakemem, u32t *logsize, u32t *removemem, u32t flags);

#ifdef __cplusplus
}
#endif

#endif // QSINIT_SYS_FUNCS
