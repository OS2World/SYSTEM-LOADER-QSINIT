//
// QSINIT API
// hardware management
//
#ifndef QSINIT_HMFUNC
#define QSINIT_HMFUNC

#ifdef __cplusplus
extern "C" {
#endif
#include "qstypes.h"

/** Safe version of hlp_readmsr() (read MSR register).
    Function catch an occured exception and return zeroes in this case.
    @param [in]     index      register number
    @param [out]    ddlo       ptr to low dword (eax value), can be 0
    @param [out]    ddhi       ptr to high dword (edx value), can be 0
    @return bool - success flag, return 0 in both dwords if failed */
u32t  _std hlp_getmsrsafe (u32t index, u32t *ddlo, u32t *ddhi);

/** Safe version of hlp_writemsr() (write MSR register).
    Function catch an occured exception and return zero in this case.
    @param  index      register number
    @param  ddlo       low dword (eax value)
    @param  ddhi       high dword (edx value)
    @return bool - success flag */
u32t  _std hlp_setmsrsafe (u32t index, u32t ddlo, u32t ddhi);

/** return rdtsc counter value.
    Note, that QSINIT support started on 486DX and this function will
    always return 0 on 486 ;) */
u64t  _std hlp_tscread    (void);

/** return number of rdtsc cycles in 55 ms.
    On EFI host value is constant between tm_calibrate() calls and VERY
    inaccurate.
    On BIOS host value calulated in real time - every 55 ms and function
    can cause a delay if called immediately after tm_calibrate().

    Now this value must be nearly constant, because both clock modulation
    settings and mtrr changes cannot affect rdtsc counter.

    @return 0 on 486 or timer error, else - value */
u64t  _std hlp_tscin55ms  (void);

/** query cpu temperature.
    Function read Intel CPUs digital sensor value from MSR.
    @return current cpu temperature, in degrees, or 0 if not supported */
u32t  _std hlp_getcputemp (void);

/** query cpu clock modulation.
    @return value      in range of 1..16 for every 6.25% of cpu clock or 0 on error
    (no clock modulation in CPU) */
u32t  _std hlp_cmgetstate (void);

#define CPUCLK_MINFREQ       1    ///< lower possible freqency
#define CPUCLK_AVERAGE       8    ///<  50% speed
#define CPUCLK_MAXFREQ      16    ///< 100% speed

/** set cpu clock modulation.
    Function calls tm_calibrate() on success.
    @param  state      1..16 - new clock modulation value.
    @retval 0          on success
    @retval ENODEV     no clock modulation on this CPU
    @retval EINVAL     invalid argument */
u32t  _std hlp_cmsetstate (u32t state);


/// @name hlp_mtrrquery() flags
//@{
#define MTRRQ_FIXREGS   0x0001    ///< fixed range MTRRs supported
#define MTRRQ_WRCOMB    0x0002    ///< write combining is supported
#define MTRRQ_SMRR      0x0004    ///< system-management MTRR is supported
//@}

/// @name hlp_mtrrquery() state
//@{
#define MTRRS_DEFMASK   0x007F    ///< default cache type mask
#define MTRRS_FIXON     0x0400    ///< fixed range registers are enabled
#define MTRRS_MTRRON    0x0800    ///< MTRR usage is enabled
#define MTRRS_NOCALBRT  0x0100    ///< set state only: do not calibrate i/o delay
//@}

/** query mtrr info.
    @param [out] flags     ptr to mtrr info flags (see MTRRQ_*, can be 0)
    @param [out] state     ptr to mtrr current state (see MTRRS_*, can be 0)
    @param [out] addrbits  ptr to physaddr bits, supported by processor
    @return number of variable range MTRR registers in processor */
u32t  _std hlp_mtrrquery  (u32t *flags, u32t *state, u32t *addrbits);


/// @name MTRR setup error codes
//@{
#define MTRRERR_OK      0x0000
#define MTRRERR_STATE   0x0001    ///< incorrect state parameter
#define MTRRERR_HARDW   0x0002    ///< no mtrr
#define MTRRERR_ACCESS  0x0003    ///< MSR registers access failed
#define MTRRERR_INDEX   0x0004    ///< invalid variable register index
#define MTRRERR_ADDRESS 0x0005    ///< invalid start address
#define MTRRERR_LENGTH  0x0006    ///< invalid block length
#define MTRRERR_ADDRTRN 0x0007    ///< address bits truncated by mask
//@}

/** set mtrr state.
    On temporary MTRR disabling MTRRS_NOCALBRT flag is recommended to use - it
    prevent call of i/o delay calibration code.

    @param state     state flags (MTRRS_*)
    @return MTRRERR_* value */
u32t  _std hlp_mtrrstate  (u32t state);

/// @name hlp_mtrrvread() state parameter
//@{
#define MTRRF_TYPEMASK  0x0007    ///< cache type mask
#define MTRRF_UC        0x0000    ///< uncacheable
#define MTRRF_WC        0x0001    ///< write combining
#define MTRRF_WT        0x0004    ///< write-through
#define MTRRF_WP        0x0005    ///< write-protected
#define MTRRF_WB        0x0006    ///< writeback
#define MTRRF_ENABLED   0x0100    ///< register is enabled for use
//@}

/** read variable range mtrr register.
    @param [in]  reg       variable range register index
    @param [out] start     start address
    @param [out] length    block length
    @param [out] state     block flags (see MTRRF_*)
    @return success flag (1/0) */
int   _std hlp_mtrrvread  (u32t reg, u64t *start, u64t *length, u32t *state);

/** setup variable range mtrr register.
    @param reg        variable range register index
    @param start      start address
    @param length     block length
    @param state      block flags (see MTRRF_*)
    @return MTRRERR_* value */
u32t  _std hlp_mtrrvset   (u32t reg, u64t start, u64t length, u32t state);

#define MTRR_FIXEDMAX       88    ///< max number of fixed ranges

/** read fixed range mtrr register.
    Function merge neighboring fixed blocks with the same cache types
    and return list in readable format.
    All of array addresses are required by call, else function will fail.
    @param [out] start     array of [MTRR_FIXEDMAX] start adresses
    @param [out] length    array of [MTRR_FIXEDMAX] block lengths
    @param [out] state     array of [MTRR_FIXEDMAX] cache types
    @return number of filled entries or 0 on invalid parameter/no mtrr */
u32t  _std hlp_mtrrfread  (u32t *start, u32t *length, u32t *state);

/** return real cache type for specified area (based on MTRR values).
    This value is determined by Intel rules and both fixed and variable range
    MTRR registers. Function takes in mind MTRR state too (i.e. if MTRR is off
    you will recieve UC type).
    Note, that function denies request, which cross 1Mb border (phys 0x100000).
    @param start      start address
    @param length     block length
    @return cache type (MTRRF_*) or MTRRF_TYPEMASK value if area is invalid or
            multiple cache types used over it */
u32t  _std hlp_mtrrsum    (u64t start, u64t length);

/** setup fixed range mtrr registers.
    Function split addr and length to multiple fixed length registers.
    If addr/addr+length not aligned to fixed MTRR boundaries - function
    will fail.
    @param addr       start adresses
    @param length     block length
    @param state      cache type
    @return MTRRERR_* value */
u32t  _std hlp_mtrrfset   (u32t addr, u32t length, u32t state);

/** reset MTRR registers to values, was setuped by BIOS.
    @return success flag (1/0) */
void  _std hlp_mtrrbios   (void);

/** build MTRR batch setup buffer for DOSHLP.
    This call made a copy of current MTRR state in supplied buffer.
    @param buffer     320 bytes length (32 MSRs: dw reg, dd eax, dd edx)
    @return number of actually filled msr enties */
u32t  _std hlp_mtrrbatch  (void *buffer);

/** apply MTRR batch buffer, recieved from hlp_mtrrbatch().
    No any checks preformed on buffer, so, be careful. With wrong buffer you
    will get trap or MTRR off entirely or PC in crazy state.
    tm_calibrate() must be called after it if MTRR om/off state is changed.
    @param buffer     320 bytes length (32 MSRs: dw reg, dd eax, dd edx) */
void  _std hlp_mtrrapply  (void *buffer);

/** check if MTRR changed by QSINIT API calls.
    @param state      MTRR state was modified? (1/0)
    @param fixed      fixed range MTRRs was modified? (1/0)
    @param variable   variable range MTRRs was modified? (1/0)
    @return 1 if one of asked things was modified, else 0 */
int   _std hlp_mtrrchanged(int state, int fixed, int variable);

/** read from PCI config space.
    @param bus        bus number
    @param slot       slot number
    @param func       function number
    @param offs       data offset (aligned to size)
    @param size       data size (1,2,4). 4 will be used if size is 0 or >4
    @return readed value */
u32t  _std hlp_pciread    (u16t bus, u8t slot, u8t func, u8t offs, u8t size);

/** write to PCI config space.
    @param bus        bus number
    @param slot       slot number
    @param func       function number
    @param offs       data offset (aligned to size)
    @param size       data size (1,2,4)
    @param value      data to write */
void  _std hlp_pciwrite   (u16t bus, u8t slot, u8t func, u8t offs, u8t size,
                           u32t value);

#pragma pack(1)
typedef struct {
   u16t         bus;
   u8t         slot;
   u8t         func;
   u16t    vendorid;           ///< vendor id
   u16t    deviceid;           ///< device id
   u16t   classcode;           ///< class/subclass only (word at ofs 9)
   u8t     progface;           ///< programming interface (byte at ofs 9)
   u8t       header;           ///< header type
} pci_location;
#pragma pack()

/** find PCI device by vendor and device id.
    @param       vendor_id  vendor id
    @param       device_id  device id
    @param       index      zero-based index to access identical devices on a system
    @param [out] dev        device location, can be 0
    @param       nodupes    detect devices, mapped to all 8 functions of selected slot
    @return success flag (1/0) */
int   _std hlp_pcifind    (u16t vendor_id, u16t device_id, u16t index,
                           pci_location *dev, int nodupes);

/** find PCI device by class code.
    @param       classcode  (class<<8|subclass) (no interface code here!)
    @param       index      zero-based index to access identical devices on a system
    @param [out] dev        device location, can be 0
    @param       nodupes    detect devices, mapped to all 8 functions of selected slot
    @return success flag (1/0) */
int   _std hlp_pciclass   (u32t classcode, u16t index, pci_location *dev, int nodupes);

/** get PCI device base address list.
    @param [in,out] dev     Only bus/slot/func required, other will be filled on exit.
    @param [out]    bases   Array of six u64t for base addresses (values stored
                            without applying PCI_ADDR_MEM_MASK, so i/o and mem
                            type bits are usable).
    @param [out]    sizes   Array of six u64t for size
    @return number of filled entries */
u32t  _std hlp_pcigetbase (pci_location *dev, u64t *bases, u64t *sizes);

/** enum PCI devices.
    Enum PCI devices. To start process - call hlp_pcigetnext(dev,1), to continue -
    hlp_pcigetnext(dev,0). Bus, slot and func fields in dev must be preserved between
    calls.

    @param [in,out] dev     Only bus/slot/func required, other will be filled on exit.
    @param [in]     init    Set 1 for first call, 0 for next iterations.
    @param [in]     nodupes Detect devices, mapped to all 8 functions of selected slot
    @return 1 if new entry available in dev, 0 if no more devices */
int   _std hlp_pcigetnext (pci_location *dev, int init, int nodupes);

/** query PCI device presence.
    @param [in,out] dev     Only bus/slot/func required, other will be filled on exit.
    @return success flag (1/0) */
int   _std hlp_pciexist   (pci_location *dev);

/** try to find PCI device, who owns the specified memory address.
    Function enums PCI devices and try to find the which one, who includes
    "addr" value into one of used address ranges.
    @param       addr       address to search
    @param [out] dev        device location
    @return 0 if failed or 1-based index of memory range and device data in
            "dev" variable */
u32t  _std hlp_pcifindaddr(u64t addr, pci_location *dev);

/** function convert vendor:id or bus.slot.func string into pci_location data.
    @param [out] dev        device location
    @return PCILOC_* value (0 on error) */
u32t  _std hlp_pciatoloc  (const char *str, pci_location *dev);

/// @name hlp_pciatoloc() result code
//@{
#define PCILOC_ERROR      0
#define PCILOC_VENDOR     1
#define PCILOC_BUSSLOT    2
//@}

/// return maximum PCI bus number
u32t  _std hlp_pcimaxbus  (void);

#ifdef __cplusplus
}
#endif
#endif // QSINIT_HMFUNC
