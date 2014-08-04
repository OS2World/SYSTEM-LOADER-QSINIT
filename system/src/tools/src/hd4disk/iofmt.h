//
// QSINIT OS/2 "ramdisk" basedev
// some ADD drivers stuff (required only)
//

#ifndef QS_DDIOFMT_H
#define QS_DDIOFMT_H

#include "qstypes.h"

#define UF_REMOVABLE          0x0001  // Media can be removed
#define UF_CHANGELINE         0x0002  // ChangeLine Supported
#define UF_PREFETCH           0x0004  // Supports prefetch reads
#define UF_A_DRIVE            0x0008  // Manages A:
#define UF_B_DRIVE            0x0010  // Manages B:
#define UF_NODASD_SUPT        0x0020  // Suppress DASD Mgr support
#define UF_NOSCSI_SUPT        0x0040  // Suppress SCSI Mgr support
#define UF_DEFECTIVE          0x0080  // Device is defective
#define UF_FORCE              0x0100  // Device presense is forced
#define UF_NOTPRESENT         0x0200  // Device is not present now
#define UF_LARGE_FLOPPY       0x0400  // Treat removable as Large Floppy
#define UF_USB_DEVICE         0x0800  // USB device - could deny any request until real device is attached

#define UIB_TYPE_DISK         0x0000  // All Direct Access Devices
#define UIB_TYPE_TAPE         0x0001  // Sequencial Access Devices
#define UIB_TYPE_PRINTER      0x0002  // Printer Device
#define UIB_TYPE_PROCESSOR    0x0003  // Processor type device
#define UIB_TYPE_WORM         0x0004  // Write Once Read Many Device
#define UIB_TYPE_CDROM        0x0005  // CD ROM Device
#define UIB_TYPE_SCANNER      0x0006  // Scanner Device
#define UIB_TYPE_OPTICAL_MEMORY 0x0007 // some Optical disk
#define UIB_TYPE_CHANGER      0x0008  // Changer device e.g. juke box
#define UIB_TYPE_COMM         0x0009  // Communication devices
#define UIB_TYPE_ATAPI        0x000A  // Unspecific ATAPI protocol device
#define UIB_TYPE_UNKNOWN      0x001F  // Unknown or faked device

typedef struct {
   u16t             AdapterIndex;     // adapter #
   u16t                UnitIndex;     // unit # on this card
   u16t                UnitFlags;     // flags
   u16t                 Reserved;     //
   u16t               UnitHandle;     // assigned by ADD or filter
   u16t          FilterADDHandle;     // handle of filter ADD
   u16t                 UnitType;     // unit type
   u16t             QueuingCount;     // recommended number to queue
   u8t          UnitSCSITargetID;     //
   u8t               UnitSCSILUN;     //
} UNITINFO, far *PUNITINFO;

#define AI_DEVBUS_OTHER       0x0000
#define AI_DEVBUS_ST506       0x0001  // ST-506 CAM-I
#define AI_DEVBUS_ST506_II    0x0002  // ST-506 CAM-II
#define AI_DEVBUS_ESDI        0x0003  // ESDI
#define AI_DEVBUS_FLOPPY      0x0004  // diskette
#define AI_DEVBUS_SCSI_1      0x0005
#define AI_DEVBUS_SCSI_2      0x0006
#define AI_DEVBUS_SCSI_3      0x0007
#define AI_DEVBUS_NONSCSI_CDROM 0x0008 // non-scsi CD-ROM interface bus

#define AI_DEVBUS_FAST_SCSI   0x0100
#define AI_DEVBUS_8BIT        0x0200
#define AI_DEVBUS_16BIT       0x0400
#define AI_DEVBUS_32BIT       0x0800

#define AI_IOACCESS_OTHER       0x00
#define AI_IOACCESS_BUS_MASTER  0x01
#define AI_IOACCESS_PIO         0x02
#define AI_IOACCESS_DMA_SLAVE   0x04
#define AI_IOACCESS_MEMORY_MAP  0x08

#define AI_HOSTBUS_OTHER        0x00
#define AI_HOSTBUS_ISA          0x01
#define AI_HOSTBUS_EISA         0x02
#define AI_HOSTBUS_uCHNL        0x03
#define AI_HOSTBUS_UNKNOWN      0x0F

#define AI_BUSWIDTH_8BIT        0x10
#define AI_BUSWIDTH_16BIT       0x20
#define AI_BUSWIDTH_32BIT       0x30
#define AI_BUSWIDTH_64BIT       0x40
#define AI_BUSWIDTH_UNKNOWN     0xF0

#define AF_16M                0x0001  // supports >16M addresses
#define AF_IBM_SCB            0x0002  // supports IBM SCB commands
#define AF_HW_SCATGAT         0x0004  // supports scatter/gather in HW
#define AF_CHS_ADDRESSING     0x0008  // supports CHS addressing in HW
#define AF_ASSOCIATED_DEVBUS  0x0010  // supports >1 Device Bus

typedef struct {
   char          AdapterName[17];     // adapter name
   u8t                  Reserved;     //
   u16t             AdapterUnits;     // # of units on adapter
   u16t            AdapterDevBus;     // bus type - adapter to device
   u8t           AdapterIOAccess;     // I/O type - adapter to host
   u8t            AdapterHostBus;     // bus type - adapter to host
   u8t       AdapterSCSITargetID;     //
   u8t            AdapterSCSILUN;     //
   u16t             AdapterFlags;     // flags
   u16t              MaxHWSGList;     // max HW S/G list length for adapter
   u32t           MaxCDBTrLength;     // max data length for CDBs
   UNITINFO          UnitInfo[1];     // unit info array
} ADAPTERINFO, far *PADAPTERINFO;

// ADD Level for DEVICETABLE->AddLevelMajor, AddLevelMinor
#define ADD_LEVEL_MAJOR       0x01
#define ADD_LEVEL_MINOR       0x00

typedef struct {
   u8t             ADDLevelMajor;     // ADD major support level
   u8t             ADDLevelMinor;     // ADD minor support level
   u16t                ADDHandle;     // ADD handle
   u16t            TotalAdapters;     // # of adapters supported
   ADAPTERINFO near *pAdapter[1];     // array of adapter info ptrs
} DEVICETABLE, far *PDEVICETABLE;

typedef struct _IORBH far *PIORB;

typedef struct _IORBH {
   u16t                   Length;     // IORB length
   u16t               UnitHandle;     // unit identifier
   u16t              CommandCode;     // command code
   u16t          CommandModifier;     // command modifier
   u16t           RequestControl;     // control flags
   u16t                   Status;     //
   u16t                ErrorCode;     //
   u32t                  Timeout;     // cmd completion timeout
   u16t           StatusBlockLen;     // status block length
   u8t near        *pStatusBlock;     // status block
   u16t               Reserved_1;     //
   PIORB                pNxtIORB;     // next IORB
   void far       *NotifyAddress;     // notification address
   u8t           DMWorkSpace[20];     //
   u8t          ADDWorkSpace[16];     //
} IORBH;

typedef struct {
   IORBH                   iorbh;     // IORB Header
   DEVICETABLE far *pDeviceTable;     // adapter dev table
   u16t           DeviceTableLen;     // length of adapter dev table
} IORB_CONFIGURATION, far *PIORB_CONFIGURATION;

typedef struct {
   u32t             TotalSectors;
   u16t           BytesPerSector;
   u16t                 Reserved;
   u16t                 NumHeads;
   u32t           TotalCylinders;
   u16t          SectorsPerTrack;
} GEOMETRY, far *PGEOMETRY;

typedef struct {
   IORBH                   iorbh;     // IORB Header
   PGEOMETRY           pGeometry;     // geometry block
   u16t              GeometryLen;     // length of geometry block
} IORB_GEOMETRY, far *PIORB_GEOMETRY;

typedef struct {
   IORBH                   iorbh;     //
   u16t                    Flags;
   PUNITINFO           pUnitInfo;     // for IOCM_CHANGE_UNITINFO
   u16t              UnitInfoLen;     // length of UnitInfo structure
} IORB_UNIT_CONTROL, far *PIORB_UNIT_CONTROL;

typedef struct {
   IORBH                   iorbh;
   u16t               UnitStatus;
} IORB_UNIT_STATUS, far *PIORB_UNIT_STATUS;

typedef struct {
   u32t                ppXferBuf;     // Physical pointer to transfer buffer
   u32t               XferBufLen;     // Length of transfer buffer
} SCATGATENTRY, far *PSCATGATENTRY;

typedef struct {
   IORBH                   iorbh;     // IORB Header
   u16t                  cSGList;     // Count of S/G list elements
   PSCATGATENTRY         pSGList;     // far ptr to S/G List
   u32t                 ppSGList;     // physical addr of S/G List
   u32t                      RBA;     // RBA Starting Address
   u16t               BlockCount;     // Block Count
   u16t            BlocksXferred;     // Block Done Count
   u16t                BlockSize;     // Size of a block in bytes
   u16t                    Flags;
} IORB_EXECUTEIO, far *PIORB_EXECUTEIO;


/* IORB CommandCode and CommandModifier codes.
   CommandCode prefixed by IOCC and CommandModifier by IOCM.
   M = manditory, O = optional */
#define IOCC_CONFIGURATION        0x0001  //
#define IOCM_GET_DEVICE_TABLE     0x0001  // M
#define IOCM_COMPLETE_INIT        0x0002  // O
#define IOCM_SAVE_DMD_INFO        0x0008  // O  (USB)

#define IOCC_UNIT_CONTROL         0x0002  //
#define IOCM_ALLOCATE_UNIT        0x0001  // M
#define IOCM_DEALLOCATE_UNIT      0x0002  // M
#define IOCM_CHANGE_UNITINFO      0x0003  // M
#define IOCM_CHANGE_UNITSTATUS    0x0004  // O sent after resume

#define IOCC_GEOMETRY             0x0003  //
#define IOCM_GET_MEDIA_GEOMETRY   0x0001  // M
#define IOCM_SET_MEDIA_GEOMETRY   0x0002  // O (M) >1 Media type
#define IOCM_GET_DEVICE_GEOMETRY  0x0003  // M
#define IOCM_SET_LOGICAL_GEOMETRY 0x0004  // O (M) CHS Addressable

#define IOCC_EXECUTE_IO           0x0004  //
#define IOCM_READ                 0x0001  // M
#define IOCM_READ_VERIFY          0x0002  // M
#define IOCM_READ_PREFETCH        0x0003  // O
#define IOCM_WRITE                0x0004  // M
#define IOCM_WRITE_VERIFY         0x0005  // M

#define IOCC_FORMAT               0x0005  //
#define IOCM_FORMAT_MEDIA         0x0001  // O (M) If HW requires
#define IOCM_FORMAT_TRACK         0x0002  // O (M) If HW requires
#define IOCM_FORMAT_PROGRESS      0x0003  // O

#define IOCC_UNIT_STATUS          0x0006  //
#define IOCM_GET_UNIT_STATUS      0x0001  // M
#define IOCM_GET_CHANGELINE_STATE 0x0002  // M (O) Fixed Media Only
#define IOCM_GET_MEDIA_SENSE      0x0003  // M
#define IOCM_GET_LOCK_STATUS      0x0004  // O

#define IOCC_DEVICE_CONTROL       0x0007  //
#define IOCM_ABORT                0x0001  // O (M) SCSI
#define IOCM_RESET                0x0002  // O (M) SCSI
#define IOCM_SUSPEND              0x0003  // O (M) Floppy Driver
#define IOCM_RESUME               0x0004  // O (M) Floppy Driver
#define IOCM_LOCK_MEDIA           0x0005  // M (O) Fixed Media Only
#define IOCM_UNLOCK_MEDIA         0x0006  // M (O) Fixed Media Only
#define IOCM_EJECT_MEDIA          0x0007  // O (M) SCSI & Floppy driver
#define IOCM_GET_QUEUE_STATUS     0x0008  // O (M) ATA/ATAPI Devices

#define IOCC_ADAPTER_PASSTHRU     0x0008  //
#define IOCM_EXECUTE_SCB          0x0001  // O
#define IOCM_EXECUTE_CDB          0x0002  // O (M) SCSI Adapters
#define IOCM_EXECUTE_ATA          0x0003  // o [001] ATA/ATAPI Devices

#define IOCC_RESOURCE             0x0009  //
#define IOCM_REPORT_RESOURCES     0x0001  // O (M) ATA/ATAPI Devices

#define MAX_IOCC_COMMAND  IOCC_ADAPTER_PASSTHRU

// unit status for IORB_UNIT_STATUS.IOCM_GET_UNIT_STATUS->UnitStatus
#define US_READY                  0x0001  // Unit ready
#define US_POWER                  0x0002  // Unit powered on
#define US_DEFECTIVE              0x0004  // Unit operational?

// unit status for IORB_UNIT_STATUS.IOCM_GET_CHANGELINE_STATE->UnitStatus
#define US_CHANGELINE_ACTIVE      0x0001  // ChangeLine State

// unit status for IORB_UNIT_STATUS.IOCM_GET_MEDIA_SENSE->UnitStatus
#define US_MEDIA_UNKNOWN          0x0000  // unable to determine media
#define US_MEDIA_720KB            0x0001  // 720KB in 3.5" drive
#define US_MEDIA_144MB            0x0002  // 1.44MB in 3.5" drive
#define US_MEDIA_288MB            0x0003  // 2.88MB in 3.5" drive
#define US_MEDIA_LARGE_FLOPPY     0x0004  // > 2.88 Floppies

// unit status for IORB_UNIT_STATUS.IOCM_GET_LOCK_STATE->UnitStatus
#define US_LOCKED                 0x0001  // Unit locked

// status flags returned in IORBH->Status
#define IORB_DONE                 0x0001  // 1 = done, 0 = in progress
#define IORB_ERROR                0x0002  // 1 = error, 0 = no error
#define IORB_RECOV_ERROR          0x0004  // recovered error
#define IORB_STATUSBLOCK_AVAIL    0x0008  // status block available

// request control flags in IORBH->RequestControl
#define IORB_ASYNC_POST           0x0001  // asynchronous post enabled
#define IORB_CHAIN                0x0002  // IORB Chain Link enabled
#define IORB_CHS_ADDRESSING       0x0004  // CHS fmt addr in RBA Field
#define IORB_REQ_STATUSBLOCK      0x0008  // obtain Status Block Data
#define IORB_DISABLE_RETRY        0x0010  // disable retries in ADD

// error Codes returned in IORBH->ErrorCode
#define IOERR_RETRY               0x8000
#define IOERR_CMD                 0x0100
#define IOERR_CMD_NOT_SUPPORTED   IOERR_CMD+1
#define IOERR_CMD_SYNTAX          IOERR_CMD+2
#define IOERR_CMD_SGLIST_BAD      IOERR_CMD+3
#define IOERR_CMD_SW_RESOURCE     IOERR_CMD+IOERR_RETRY+4
#define IOERR_CMD_ABORTED         IOERR_CMD+5
#define IOERR_CMD_ADD_SOFTWARE_FAILURE  IOERR_CMD+6
#define IOERR_CMD_OS_SOFTWARE_FAILURE   IOERR_CMD+7

#define IOERR_UNIT                0x0200
#define IOERR_UNIT_NOT_ALLOCATED  IOERR_UNIT+1
#define IOERR_UNIT_ALLOCATED      IOERR_UNIT+2
#define IOERR_UNIT_NOT_READY      IOERR_UNIT+3
#define IOERR_UNIT_PWR_OFF        IOERR_UNIT+4

#define IOERR_RBA                 0x0300
#define IOERR_RBA_ADDRESSING_ERROR      IOERR_RBA+IOERR_RETRY+1
#define IOERR_RBA_LIMIT           IOERR_RBA+2
#define IOERR_RBA_CRC_ERROR       IOERR_RBA+IOERR_RETRY+3

#define IOERR_MEDIA               0x0400
#define IOERR_MEDIA_NOT_FORMATTED IOERR_MEDIA+1
#define IOERR_MEDIA_NOT_SUPPORTED IOERR_MEDIA+2
#define IOERR_MEDIA_WRITE_PROTECT IOERR_MEDIA+3
#define IOERR_MEDIA_CHANGED       IOERR_MEDIA+4
#define IOERR_MEDIA_NOT_PRESENT   IOERR_MEDIA+5

#define IOERR_ADAPTER             0x0500
#define IOERR_ADAPTER_HOSTBUSCHECK      IOERR_ADAPTER+1
#define IOERR_ADAPTER_DEVICEBUSCHECK    IOERR_ADAPTER+IOERR_RETRY+2
#define IOERR_ADAPTER_OVERRUN     IOERR_ADAPTER+IOERR_RETRY+3
#define IOERR_ADAPTER_UNDERRUN    IOERR_ADAPTER+IOERR_RETRY+4
#define IOERR_ADAPTER_DIAGFAIL    IOERR_ADAPTER+5
#define IOERR_ADAPTER_TIMEOUT     IOERR_ADAPTER+IOERR_RETRY+6
#define IOERR_ADAPTER_DEVICE_TIMEOUT    IOERR_ADAPTER+IOERR_RETRY+7
#define IOERR_ADAPTER_REQ_NOT_SUPPORTED IOERR_ADAPTER+8
#define IOERR_ADAPTER_REFER_TO_STATUS   IOERR_ADAPTER+9
#define IOERR_ADAPTER_NONSPECIFIC       IOERR_ADAPTER+10

#define IOERR_DEVICE              0x0600
#define IOERR_DEVICE_DEVICEBUSCHECK     IOERR_DEVICE+IOERR_RETRY+1
#define IOERR_DEVICE_REQ_NOT_SUPPORTED  IOERR_DEVICE+2
#define IOERR_DEVICE_DIAGFAIL     IOERR_DEVICE+3
#define IOERR_DEVICE_BUSY         IOERR_DEVICE+IOERR_RETRY+4
#define IOERR_DEVICE_OVERRUN      IOERR_DEVICE+IOERR_RETRY+5
#define IOERR_DEVICE_UNDERRUN     IOERR_DEVICE+IOERR_RETRY+6
#define IOERR_DEVICE_RESET        IOERR_DEVICE+7
#define IOERR_DEVICE_NONSPECIFIC  IOERR_DEVICE+8
#define IOERR_DEVICE_ULTRA_CRC    IOERR_DEVICE+IOERR_RETRY+9


void IORB_NotifyCall(IORBH far *pIORB);
#pragma aux IORB_NotifyCall = \
   "push    es"                  \
   "push    di"                  \
   "call    far ptr es:[di+1Ch]" \
   "add     sp,4"                \
   parm [es di] \
   modify exact [ax bx cx dx];

#endif // QS_DDIOFMT_H
