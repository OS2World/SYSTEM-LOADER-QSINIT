//
// QSINIT OS/2 "ramdisk" basedev
// dd req header
//
#ifndef QS_DDSTRAT_H
#define QS_DDSTRAT_H

#include "qstypes.h"

#define MAKEULONG(lo,hi) ((u32t)(((u16t)(lo)) | ((u32t)((u16t)(hi)))<<16))
#define MAKEP(sel,off)   ((void far*)MAKEULONG(off, sel))
#define FLATTOP(addr)    ((void far*)MAKEULONG(addr, (addr)>>13|7))

#ifdef SELECTOROF
#undef SELECTOROF
#undef OFFSETOF
#endif
#define SELECTOROF(p)    (((u16t far*)&(p))[1])
#define OFFSETOF(p)      (((u16t far*)&(p))[0])

#define STERR        0x8000           // Bit 15 - Error
#define STBUI        0x0200           // Bit  9 - Busy
#define STDON        0x0100           // Bit  8 - Done
#define STECODE      0x00FF           // Error code
#define WRECODE      0x0000

#define ST_BADCMD    0x8003

#define CMDOpen          13           // device open
#define CMDClose         14           // device close
#define CMDGenIOCTL      16           // Generic IOCTL
#define CMDInitBase      27           // INIT command for base DDs
#define CMDShutdown      28           //
#define CMDGetDevSupport 29           //
#define CMDInitComplete  31           //

typedef struct _ReqPacket {
   u8t            Length;
   u8t              Unit;
   u8t           Command;
   u16t           Status;
   u8t             Flags;
   u8t            Res[3];
   struct
   _ReqPacket far  *Link;
} ReqPacket;

typedef struct {
   u8t            Length;
   u8t              Unit;
   u8t           Command;
   u16t           Status;
   u8t             Flags;
   u8t            Res[7];
   u8t            Data_1;
   void far   *Pointer_1;
   void far   *Pointer_2;
   u8t            Data_2;
} InitPacket;

typedef struct {
   u8t            Length;
   u8t              Unit;
   u8t           Command;
   u16t           Status;
   u8t             Flags;
   u8t            Res[3];
   struct
   _ReqPacket far  *Link;
   u8t          Category;
   u8t          Function;
   u8t far   *ParmPacket;
   u8t far   *DataPacket;
   u16t              sfn;
   u16t          ParmLen;
   u16t          DataLen;
} IOCTLPacket;

typedef struct {
   u8t            Length;
   u8t              Unit;
   u8t           Command;
   u16t           Status;
   u8t             Flags;
   u8t            Res[3];
   struct
   _ReqPacket far  *Link;
   u16t              sfn;
} OpenClosePacket;

typedef struct {
   u8t            Length;
   u8t              Unit;
   u8t           Command;
   u16t           Status;
   u8t             Flags;
   u8t           Res[10];
   void far    *DCS_Addr;             // 16:16 of driver caps struc
   void far    *VCS_Addr;             // 16:16 of volume char struc
} ExtCapPacket;

typedef struct {
   u16t             Res1;
   u16t     Disk_Tab_Ofs;
   u16t             Res2;
   u16t     Cmd_Line_Ofs;
   u16t     Mach_Cfg_Ofs;
} InitArgs;

typedef struct {
   u8t            Length;
   u8t              Unit;
   u8t           Command;
   u16t           Status;
   u8t             Flags;
   u8t            Res[7];
   u8t              Func;
   u32t         Reserved;
} ShutdownPacket;

typedef struct {
   u16t         Reserved;             // reserved, set to zero
   u8t          VerMajor;             // major version of interface supported
   u8t          VerMinor;             // minor version of interface supported
   u32t     Capabilities;             // bitfield for driver capabilties
   void far   *Strategy2;             // entry point for strategy-2
   void far    *EndofInt;             // entry point for DD_EndOfInt
   void far *ChgPriority;             // entry point for DD_ChgPriority
   void far  *SetRestPos;             // entry point for DD_SetRestPos
   void far *GetBoundary;             // entry point for DD_GetBoundary
   void far   *Strategy3;             // entry point for strategy-3
} DriverCaps;

// driver Capabilites bit mask (DriverCaps.Capabilities)
#define GDC_DD_READ2      0x0001      // Read2 supported with DMA hardware
#define GDC_DD_DMA_WORD   0x0002      // DMA on word-aligned buffers supported
#define GDC_DD_DMA_BYTE   0x0006      // DMA on byte-aligned buffers supported
#define GDC_DD_MIRROR     0x0008      // Disk Mirroring supported by driver
#define GDC_DD_DUPLEX     0x0010      // Disk Duplexing supported by driver
#define GDC_DD_NO_BLOCK   0x0020      // Driver does not block in Strategy 2
#define GDC_DD_16M        0x0040      // >16M memory supported
#define GDC_DD_STRAT3     0x0080      // strat-3 entry point

typedef struct {
   u16t    VolDescriptor;             // see equates below
   u16t      AvgSeekTime;             // milliseconds, if unknown, FFFFh
   u16t       AvgLatency;             // milliseconds, if unknown, FFFFh
   u16t   TrackMinBlocks;             // blocks on smallest track
   u16t   TrackMaxBlocks;             // blocks on largest track
   u16t HeadsPerCylinder;             // if unknown or not applicable use 1
   u32t VolCylinderCount;             // number of cylinders on volume
   u32t   VolMedianBlock;             // block in center of volume for seek
   u16t        MaxSGList;             // Adapter scatter/gather list limit
} VolChars;

// Volume Descriptor bit masks (VolChars.VolDescriptor)
#define VC_REMOVABLE_MEDIA    0x0001  /* Volume resides on removable media   */
#define VC_READ_ONLY          0x0002  /* Volume is read-only                 */
#define VC_RAM_DISK           0x0004  /* Seek time independant of position   */
#define VC_HWCACHE            0x0008  /* Outboard cache supported            */
#define VC_SCB                0x0010  /* SCB protocol supported              */
#define VC_PREFETCH           0x0020  /* Prefetch read supported             */

#define IOCTL_CAT               0xC0
#define IOCTL_F_FIRST            100
#define IOCTL_F_CHECK            100
#define IOCTL_F_SIZE             101
#define IOCTL_F_READ             102
#define IOCTL_F_WRITE            103
#define IOCTL_F_CHS              104
#define IOCTL_F_INFO             105
#define IOCTL_F_LAST             105

typedef struct {
   u32t     sector;
   u32t      count;
   u32t    lindata;
   int       write;
} RW_PKT;

typedef struct {
   u32t       cyls;
   u32t      heads;
   u32t        spt;
   u32t     active;
} GEO_PKT;

typedef struct {
   u32t        loc;
   u32t    entries;
} INFO_PKT;

void (far *PrintLog)(char far*,...);

#define info_str(str)       vioprint(str)
#define log_str(str)        if (PrintLog) (*PrintLog)("HD4D: " str)
#define log_print(str,...)  if (PrintLog) (*PrintLog)("HD4D: " str,__VA_ARGS__)

#ifdef DEBUG
#define debug_str(str)      log_str(str)
#else
#define debug_str(str)
#endif

void memcpy(void far *dst, const void far *src, u16t length);

#define PAE_OK          0x0000
#define PAE_NOPAE       0x0001

// read/write data from upper/physical memory
int  paecopy(u32t lo_physaddr, u32t src_page, u16t sectors, u8t writehi);

u16t paeio(u32t SGList, u16t cSGList, u32t sector, u16t count, u8t write);

/** fill pae page tables.
    @param pdsel    Selector to contig 32k */
void paemake(u16t pdsel);

/** fill switch 32-bit page tables.
    @param pdsel    Selector to contig 32k */
void linmake(u16t pdsel);

/** change GDT selector base by direct GDT editing.
    @param size     Size (not limit), in bytes. */
void selsetup(u16t sel, u32t base, u16t size, u8t access, u8t attrs);

/** fill switch mode GDT.
    Note, what pdmake.asm used from QSINIT code too.
    @param selbase  SWSEL_CODELIN base
    @param sellim   SWSEL_CODELIN limit */
void gdtmake(u32t selbase, u16t sellim);

/** set disk header page number.
    @param hdrpage  Disk header page number
    @param hdrcopy  Phys addr of disk header page copy 
    @param dwbuf    Phys addr of 64k buffer */
void setdisk(u32t hdrpage, u32t hdrcopy, u32t dwbuf);

/// common ioctl process
void ioctl(IOCTLPacket far *iopkt);

#endif // QS_DDSTRAT_H
