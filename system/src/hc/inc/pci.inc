;cc #ifndef QS_BASE_PCI
;cc #define QS_BASE_PCI

;
; PCI bios & configuration space
;

PCI_STACK_SIZE            = 1024         ;
PCI_ROM_ADDR          = 000F0000h        ;
PCI_ENTRY_OFFSET         = 0FE6Eh        ;

PCI_BIOS_PRESENT         = 0B101h        ;
PCI_FIND_DEVICE          = 0B102h        ;
PCI_FIND_CLASS_CODE      = 0B103h        ;
PCI_GENERATE_S_CYCLE     = 0B106h        ;
PCI_READ_CONFIG_BYTE     = 0B108h        ;
PCI_READ_CONFIG_WORD     = 0B109h        ;
PCI_READ_CONFIG_DWORD    = 0B10Ah        ;
PCI_WRITE_CONFIG_BYTE    = 0B10Bh        ;
PCI_WRITE_CONFIG_WORD    = 0B10Ch        ;
PCI_WRITE_CONFIG_DWORD   = 0B10Dh        ;
PCI_GET_IRQ_ROUTING      = 0B10Eh        ; 2.1
PCI_SET_IRQ              = 0B10Fh        ; 2.1

PCI_BAD_VID              = 0FFFFh        ;
PCI_BAD_DID              = 0FFFFh        ;
PCI_NO_INTERRUPT_PIN      =    0h        ;
PCI_FUNC_MASK             =  080h        ;
PCI_MULTI_FUNC            =   07h        ;
PCI_SINGLE_FUNC           =    0h        ;

PCI_MAX_NUM_DEV           =   20h        ;

BIOS32_NAME           = 5F32335Fh        ;
PCI_NAME              = 49435024h        ;

PCI_BIOS_ERR_OK           =   00h        ; successful
PCI_BIOS_ERR_NOSUP        =   81h        ; unsupported function
PCI_BIOS_ERR_VENDOR       =   83h        ; bad vendor ID
PCI_BIOS_ERR_NODEV        =   86h        ; device not found
PCI_BIOS_ERR_BADREG       =   87h        ; bad PCI register number

PCI_ADDR_PORT             = 0CF8h        ;
PCI_DATA_PORT             = 0CFCh        ;

; PCI Configuration space

PCI_HEADER_SIZE           =   64
PCI_VENDOR_ID             = 0000h        ;
PCI_DEVICE_ID             = 0002h        ;
PCI_COMMAND               = 0004h        ;
PCI_STATUS                = 0006h        ;
PCI_CLASS_REV             = 0008h        ; 
PCI_REVISION_ID           = 0008h        ; revision ID
PCI_PROG_IF               = 0009h        ; programming interface
PCI_DEV_CLASS             = 000Ah        ; device class
PCI_CACHE_LINE            = 000Ch        ;
PCI_LATENCY_TIMER         = 000Dh        ;
PCI_HEADER_TYPE           = 000Eh        ;
PCI_BIST                  = 000Fh        ;
PCI_BASE_ADDR0            = 0010h        ;
PCI_BASE_ADDR1            = 0014h        ; type 0,1
PCI_BASE_ADDR2            = 0018h        ; type 0
PCI_BASE_ADDR3            = 001Ch        ;
PCI_BASE_ADDR4            = 0020h        ;
PCI_BASE_ADDR5            = 0024h        ;
; Header type 0 (normal devices)
PCI_CARDBUS_CIS           = 0028h        ;
PCI_SUBSYSTEM_VENDOR_ID   = 002Ch        ;
PCI_SUBSYSTEM_ID          = 002Eh        ;
PCI_ROM_ADDRESS           = 0030h        ; address:21, reserved:10, enabled:1
PCI_CAPABILITY_LIST       = 0034h        ; offset of first entry
PCI_INTERRUPT_LINE        = 003Ch        ;
PCI_INTERRUPT_PIN         = 003Dh        ;
PCI_MIN_GNT               = 003Eh        ;
PCI_MAX_LAT               = 003Fh        ;
; Header type 1 (PCI-to-PCI bridges)
PCI_PRIMARY_BUS           = 0018h        ; primary bus number
PCI_SECONDARY_BUS         = 0019h        ; secondary bus number
PCI_SUBORDINATE_BUS       = 001Ah        ; highest bus number behind the bridge
PCI_SEC_LATENCY_TIMER     = 001Bh        ; latency timer for secondary interface
PCI_IO_BASE               = 001Ch        ; I/O range behind the bridge
PCI_IO_LIMIT              = 001Dh        ;
PCI_SEC_STATUS            = 001Eh        ; secondary status register, only bit 14 used
PCI_MEMORY_BASE           = 0020h        ; memory range behind
PCI_MEMORY_LIMIT          = 0022h        ;
PCI_PREF_MEMORY_BASE      = 0024h        ; prefetchable memory range behind
PCI_PREF_MEMORY_LIMIT     = 0026h        ;
PCI_PREF_BASE_UPPER32     = 0028h        ; upper half of prefetchable memory range
PCI_PREF_LIMIT_UPPER32    = 002Ch        ;
PCI_IO_BASE_UPPER16       = 0030h        ; upper half of I/O addresses
PCI_IO_LIMIT_UPPER16      = 0032h        ;
;PCI_CAPABILITY_LIST      = 0034h        ; the same as in type 0
PCI_ROM_ADDRESS1          = 0038h        ; PCI_ROM_ADDRESS, but on other location
;PCI_INTERRUPT_LINE       = 003Ch        ; the same as in type 0
;PCI_INTERRUPT_PIN        = 003Dh        ; the same as in type 0
PCI_BRIDGE_CONTROL        = 003Eh        ;
; Header type 2 (CardBus bridges)
PCI_CB_CAPABILITY_LIST    = 0014h        ;
PCI_CB_SEC_STATUS         = 0016h        ; secondary status
PCI_CB_PRIMARY_BUS        = 0018h        ; PCI bus number
PCI_CB_CARD_BUS           = 0019h        ; CardBus bus number
PCI_CB_SUBORDINATE_BUS    = 001Ah        ; subordinate bus number
PCI_CB_LATENCY_TIMER      = 001Bh        ; CardBus latency timer
PCI_CB_MEMORY_BASE0       = 001Ch        ;
PCI_CB_MEMORY_LIMIT0      = 0020h        ;
PCI_CB_MEMORY_BASE1       = 0024h        ;
PCI_CB_MEMORY_LIMIT1      = 0028h        ;
PCI_CB_IO_BASE0           = 002Ch        ;
PCI_CB_IO_BASE0HI         = 002Eh        ;
PCI_CB_IO_LIMIT0          = 0030h        ;
PCI_CB_IO_LIMIT0HI        = 0032h        ;
PCI_CB_IO_BASE1           = 0034h        ;
PCI_CB_IO_BASE1HI         = 0036h        ;
PCI_CB_IO_LIMIT1          = 0038h        ;
PCI_CB_IO_LIMIT1HI        = 003Ah        ;
;PCI_INTERRUPT_LINE       = 003Ch        ; the same as in type 0
;PCI_INTERRUPT_PIN        = 003Dh        ; the same as in type 0
PCI_CB_BRIDGE_CONTROL     = 003Eh        ;
PCI_CB_SUBSYS_VENDOR_ID   = 0040h        ;
PCI_CB_SUBSYS_ID          = 0042h        ;
PCI_CB_LEGACY_MODE_BASE   = 0044h        ; 16-bit PC Card legacy mode base address (ExCa)


; PCI_COMMAND:
PCI_CMD_IO                = 0001h        ; enable response in I/O space
PCI_CMD_MEMORY            = 0002h        ; enable response in Memory space
PCI_CMD_MASTER            = 0004h        ; enable bus mastering
PCI_CMD_SPECIAL           = 0008h        ; enable response to special cycles
PCI_CMD_INVALIDATE        = 0010h        ; use memory write and invalidate
PCI_CMD_VGAPAL            = 0020h        ; enable palette snooping
PCI_CMD_PARITY            = 0040h        ; enable parity checking
PCI_CMD_WAIT              = 0080h        ; enable address/data stepping
PCI_CMD_SERR              = 0100h        ; enable SERR
PCI_CMD_FAST_BACK         = 0200h        ; enable back-to-back writes
PCI_CMD_INTX_DISABLE      = 0400h        ; INTx Emulation Disable

; PCI_STATUS:
PCI_STAT_INTERRUPT        = 0008h        ; interrupt status
PCI_STAT_CAP_LIST         = 0010h        ; support Capability List
PCI_STAT_66MHZ            = 0020h        ; 66 Mhz PCI 2.1 bus
PCI_STAT_UDF              = 0040h        ; user Definable Features (obsolete)
PCI_STAT_FAST_BACK        = 0080h        ; accept fast-back to back
PCI_STAT_PARITY           = 0100h        ; detected parity error
PCI_STAT_DEVSEL_MASK      = 0600h        ; DEVSEL timing
PCI_STAT_DEVSEL_FAST      = 0000h        ;
PCI_STAT_DEVSEL_MEDIUM    = 0200h        ;
PCI_STAT_DEVSEL_SLOW      = 0400h        ;
PCI_STAT_SIG_TGT_ABORT    = 0800h        ; set on target abort
PCI_STAT_REC_TGT_ABORT    = 1000h        ; master ack of "
PCI_STAT_REC_MST_ABORT    = 2000h        ; set on master abort
PCI_STAT_SIG_SYS_ERR      = 4000h        ; set when we drive SERR
PCI_STAT_PARITY_ERR       = 8000h        ; set on parity error

; PCI_HEADER_TYPE:
PCI_HEADER_NORMAL         =    0         ;
PCI_HEADER_BRIDGE         =    1         ;
PCI_HEADER_CARDBUS        =    2         ;

; PCI_BIST:
PCI_BIST_CODE_MASK        =   0Fh        ; return result
PCI_BIST_START            =   40h        ; 1 to start BIST, 2 secs or less
PCI_BIST_CAPABLE          =   80h        ; 1 if BIST capable

; PCI_BASE_ADDRESSX:
; Base addresses specify locations in memory or I/O space.
; Decoded size can be determined by writing a value of 0xFFFFFFFF into register
; and reading it back.  Only 1 bits are decoded.
PCI_ADDR_SPACE            = 0001h        ; 0 = memory, 1 = I/O
PCI_ADDR_SPACE_MEMORY     = 0000h        ;
PCI_ADDR_SPACE_IO         = 0001h        ;
PCI_ADDR_MEMTYPE_MASK     = 0006h        ;
PCI_ADDR_MEMTYPE_32       = 0000h        ; 32 bit address
PCI_ADDR_MEMTYPE_1M       = 0002h        ; below 1M (obsolete)
PCI_ADDR_MEMTYPE_64       = 0004h        ; 64 bit address
PCI_ADDR_MEM_PREFETCH     = 0008h        ; prefetchable?
PCI_ADDR_MEM_MASK    = 0FFFFFFF0h        ;
PCI_ADDR_IO_MASK     = 0FFFFFFFCh        ;
; bit 1 is reserved if address_space = 1

; PCI_ROM_ADDRESS:
PCI_ROM_ADDRESS_ENABLE    =   01h        ;
PCI_ROM_ADDRESS_MASK = 0FFFFF800h        ;

; PCI_IO_LIMIT:
PCI_IO_RANGE_TYPE_MASK    =   0Fh        ; I/O bridging type
PCI_IO_RANGE_TYPE_16      =   00h        ;
PCI_IO_RANGE_TYPE_32      =   01h        ;
PCI_IO_RANGE_MASK    = 0FFFFFFF0h        ; standard 4K I/O windows
PCI_IO_1K_RANGE_MASK = 0FFFFFFFCh        ; Intel 1K I/O windows
; PCI_MEMORY_LIMIT:
PCI_MEM_RANGE_TYPE_MASK   =   0Fh        ;
PCI_MEM_RANGE_MASK   = 0FFFFFFF0h        ;

; PCI_PREF_MEMORY_LIMIT:
PCI_PREF_RANGE_TYPE_MASK  =   0Fh        ;
PCI_PREF_RANGE_TYPE_32    =   00h        ;
PCI_PREF_RANGE_TYPE_64    =   01h        ;
PCI_PREF_RANGE_MASK  = 0FFFFFFF0h        ;

; PCI_BRIDGE_CONTROL:
PCI_BG_CTL_PARITY         = 0001h        ; enable parity detection on secondary interface
PCI_BG_CTL_SERR           = 0002h        ; the same for SERR forwarding
PCI_BG_CTL_ISA            = 0004h        ; enable ISA mode
PCI_BG_CTL_VGA            = 0008h        ; forward VGA addresses
PCI_BG_CTL_MST_ABORT      = 0020h        ; report master aborts
PCI_BG_CTL_BUS_RESET      = 0040h        ; secondary bus reset
PCI_BG_CTL_FAST_BACK      = 0080h        ; fast Back2Back enabled on secondary interface

; PCI_CB_BRIDGE_CONTROL:
PCI_CB_BG_CTL_PARITY      = 0001h        ; similar to standard bridge control register
PCI_CB_BG_CTL_SERR        = 0002h        ;
PCI_CB_BG_CTL_ISA         = 0004h        ;
PCI_CB_BG_CTL_VGA         = 0008h        ;
PCI_CB_BG_CTL_MST_ABORT   = 0020h        ;
PCI_CB_BG_CTL_CB_RESET    = 0040h        ; CardBus reset
PCI_CB_BG_CTL_16BIT_INT   = 0080h        ; enable interrupt for 16-bit cards
PCI_CB_BG_CTL_PREF_MEM0   = 0100h        ; prefetch enable for both memory regions
PCI_CB_BG_CTL_PREF_MEM1   = 0200h        ;
PCI_CB_BG_CTL_POST_WRITES = 0400h        ;

; Capability lists
PCI_CAP_LIST_ID           =    0         ; Capability ID
PCI_CAP_LIST_NEXT         =    1         ; next capability in the list
PCI_CAP_FLAGS             =    2         ; capability defined flags (16 bits)
PCI_CAP_SIZEOF            =    4         ;

; PCI_CAP_LIST_ID:
PCI_CAP_ID_PM             = 0001h        ; Power Management
PCI_CAP_ID_AGP            = 0002h        ; Accelerated Graphics Port
PCI_CAP_ID_VPD            = 0003h        ; Vital Product Data
PCI_CAP_ID_SLOTID         = 0004h        ; Slot Identification
PCI_CAP_ID_MSI            = 0005h        ; Message Signalled Interrupts
PCI_CAP_ID_CHSWP          = 0006h        ; CompactPCI HotSwap
PCI_CAP_ID_PCIX           = 0007h        ; PCI-X
PCI_CAP_ID_HT             = 0008h        ; HyperTransport
PCI_CAP_ID_VNDR           = 0009h        ; Vendor specific
PCI_CAP_ID_DBG            = 000Ah        ; Debug port
PCI_CAP_ID_CCRC           = 000Bh        ; CompactPCI Central Resource Control
PCI_CAP_ID_SHPC           = 000Ch        ; PCI Standard Hot-Plug Controller
PCI_CAP_ID_SSVID          = 000Dh        ; Bridge subsystem vendor/device ID
PCI_CAP_ID_AGP3           = 000Eh        ; AGP Target PCI-PCI bridge
PCI_CAP_ID_SECDEV         = 000Fh        ; Secure Device
PCI_CAP_ID_EXP            = 0010h        ; PCI Express
PCI_CAP_ID_MSIX           = 0011h        ; MSI-X
PCI_CAP_ID_SATA           = 0012h        ; SATA Data/Index Conf.
PCI_CAP_ID_AF             = 0013h        ; PCI Advanced Features
PCI_CAP_ID_MAX            equ PCI_CAP_ID_AF

; Power Management Registers
PCI_PM_PMC                =    2         ; PM Capabilities Register
PCI_PM_CTRL               =    4         ; PM control and status register
PCI_PM_PPB_EXTENSIONS     =    6         ; PPB support extensions
PCI_PM_DATA_REGISTER      =    7         ;
PCI_PM_SIZEOF             =    8         ;

; PCI_PM_PMC:
PCI_PM_CAP_VER_MASK       = 0007h        ; version
PCI_PM_CAP_PME_CLOCK      = 0008h        ; PME clock required
PCI_PM_CAP_RESERVED       = 0010h        ; reserved field
PCI_PM_CAP_DSI            = 0020h        ; device specific initialization
PCI_PM_CAP_AUX_POWER      = 01C0h        ; auxiliary power support mask
PCI_PM_CAP_D1             = 0200h        ; D1 power state support
PCI_PM_CAP_D2             = 0400h        ; D2 power state support
PCI_PM_CAP_PME            = 0800h        ; PME pin supported
PCI_PM_CAP_PME_MASK      = 0F800h        ; PME Mask of all supported states
PCI_PM_CAP_PME_D0         = 0800h        ; PME# from D0
PCI_PM_CAP_PME_D1         = 1000h        ; PME# from D1
PCI_PM_CAP_PME_D2         = 2000h        ; PME# from D2
PCI_PM_CAP_PME_D3         = 4000h        ; PME# from D3 (hot)
PCI_PM_CAP_PME_D3cold     = 8000h        ; PME# from D3 (cold)
PCI_PM_CAP_PME_SHIFT      =   11         ; PME mask shift

; PCI_PM_CTRL:
PCI_PM_CTRL_STATE_MASK    = 0003h        ; current power state (D0 to D3)
PCI_PM_CTRL_NO_SOFT_RESET = 0008h        ; no reset for D3hot->D0
PCI_PM_CTRL_PME_ENABLE    = 0100h        ; PME pin enable
PCI_PM_CTRL_SEL_MASK      = 1E00h        ; data select
PCI_PM_CTRL_SCALE_MASK    = 6000h        ; data scale
PCI_PM_CTRL_PME_STATUS    = 8000h        ; PME pin status

; PCI_PM_PPB_EXTENSIONS:
PCI_PM_PPB_B2_B3          =   40h        ; stop clock when in D3hot
PCI_PM_BPCC_ENABLE        =   80h        ; bus power/clock control enable


; Class codes
PCI_CB_LEGACY             = 0000h        ; device was built before class code definitions were finalized
PCI_CB_STORAGE            = 0001h        ; mass storage controller
PCI_CB_NETWORK            = 0002h        ; network controller
PCI_CB_DISPLAY            = 0003h        ; display controller
PCI_CB_MULTIMEDIA         = 0004h        ; multimedia device
PCI_CB_MEMORY             = 0005h        ; memory controller
PCI_CB_BRIDGE             = 0006h        ; bridge device
PCI_CB_COMM               = 0007h        ; simple communication controllers
PCI_CB_BASE               = 0008h        ; base system peripherals
PCI_CB_INPUT              = 0009h        ; input devices
PCI_CB_DOCKING            = 000Ah        ; docking stations
PCI_CB_PROCESSOR          = 000Bh        ; processors
PCI_CB_SB                 = 000Ch        ; serial bus controllers
PCI_CB_WIRELESS           = 000Dh        ; wireless controller
PCI_CB_IO                 = 000Eh        ; intelligent I/O controllers
PCI_CB_SAT                = 000Fh        ; satellite communication controllers
PCI_CB_ENCRYPT            = 0010h        ; encryption/Decryption controllers
PCI_CB_DSP                = 0011h        ; data acquisition and signal processing controllers
PCI_CB_STRANGE            = 00FFh        ; device does not fit in any defined

; PCI_CB_STORAGE:
PCI_CS_STORAGE_SCSI       = 0000h        ; SCSI bus controller          
PCI_CS_STORAGE_IDE        = 0001h        ; IDE controller
PCI_CS_STORAGE_FLOPPY     = 0002h        ; floppy disk controller       
PCI_CS_STORAGE_IPI        = 0003h        ; IPI bus controller           
PCI_CS_STORAGE_RAID       = 0004h        ; RAID controller              
PCI_CS_STORAGE_ATA        = 0005h        ; ATA controller               
PCI_CS_STORAGE_OTHER      = 0080h        ; other mass storage controller

; PCI_CB_NETWORK:
PCI_CS_NET_ETHERNET       = 0000h        ; ethernet controller
PCI_CS_NET_TOKENRING      = 0001h        ; Token Ring controller
PCI_CS_NET_FDDI           = 0002h        ; FDDI controller
PCI_CS_NET_ATM            = 0003h        ; ATM controller
PCI_CS_NET_ISDN           = 0004h        ; ISDN controller
PCI_CS_NET_WORLDFIP       = 0005h        ; WorldFip controller
PCI_CS_NET_PICMG          = 0006h        ; PICMG
PCI_CS_NET_OTHER          = 0080h        ; other network controller

; PCI_CB_DISPLAY:
PCI_CS_VIDEO_VGA          = 0000h        ; VGA-compatible controller.  
PCI_CS_VIDEO_XGA          = 0001h        ; XGA controller
PCI_CS_VIDEO_3D           = 0002h        ; 3D controller
PCI_CS_VIDEO_OTHER        = 0080h        ; other display controller

; PCI_CB_MULTIMEDIA:
PCI_CS_MM_VIDEO           = 0000h        ; video device
PCI_CS_MM_AUDIO           = 0001h        ; audio device
PCI_CS_MM_TELEPHONY       = 0002h        ; computer telephony device
PCI_CS_MM_OTHER           = 0080h        ; other multimedia device

; PCI_CB_MEMORY:
PCI_CS_MEM_RAM            = 0000h        ; RAM
PCI_CS_MEM_FLASH          = 0001h        ; flash
PCI_CS_MEM_OTHER          = 0080h        ; other memory controller

; PCI_CB_BRIDGE:
PCI_CS_BRIDGE_HOST        = 0000h        ; host bridge
PCI_CS_BRIDGE_ISA         = 0001h        ; ISA bridge
PCI_CS_BRIDGE_EISA        = 0002h        ; EISA bridge
PCI_CS_BRIDGE_MCA         = 0003h        ; MCA bridge
PCI_CS_BRIDGE_PCI         = 0004h        ; PCI-to-PCI bridge
PCI_CS_BRIDGE_PCMCIA      = 0005h        ; PCMCIA bridge
PCI_CS_BRIDGE_NUBUS       = 0006h        ; NuBus bridge
PCI_CS_BRIDGE_CARDBUS     = 0007h        ; CardBus bridge
PCI_CS_BRIDGE_RACEWAY     = 0008h        ; RACEway bridge
PCI_CS_BRIDGE_STPCI       = 0009h        ; semi-transparent PCI-to-PCI bridge
PCI_CS_BRIDGE_IB          = 000Ah        ; InfiniBand-to-PCI host bridge
PCI_CS_BRIDGE_OTHER       = 0080h        ; other bridge device

; PCI_CB_COMM:
PCI_CS_COMM_serial        = 0000h        ; serial controller
PCI_CS_COMM_LPT           = 0001h        ; parallel port
PCI_CS_COMM_MULTI         = 0002h        ; multiport serial controller
PCI_CS_COMM_MODEM         = 0003h        ; modem
PCI_CS_COMM_GPIB          = 0004h        ; GPIB controller
PCI_CS_COMM_SMART         = 0005h        ; Smart Card
PCI_CS_COMM_OTHER         = 0080h        ; other communications device

; PCI_CB_BASE:
PCI_CS_BASE_PIC           = 0000h        ; PIC
PCI_CS_BASE_DMA           = 0001h        ; DMA controller
PCI_CS_BASE_TIMER         = 0002h        ; system timer.
PCI_CS_BASE_RTC           = 0003h        ; RTC controller
PCI_CS_BASE_HOTPLUG       = 0004h        ; generic PCI Hot-Plug controller
PCI_CS_BASE_OTHER         = 0080h        ; ither system peripheral

; PCI_CB_INPUT:
PCI_CS_INPUT_KEYBOARD     = 0000h        ; keyboard controller
PCI_CS_INPUT_PEN          = 0001h        ; digitizer (pen)
PCI_CS_INPUT_MOUSE        = 0002h        ; mouse controller
PCI_CS_INPUT_SCANNER      = 0003h        ; scanner controller
PCI_CS_INPUT_GAMEPORT     = 0004h        ; gameport controller
PCI_CS_INPUT_OTHER        = 0080h        ; other input controller

; PCI_CB_DOCKING:
PCI_CS_DOCK_GENERIC       = 0000h        ; generic docking station
PCI_CS_DOCK_OTHER         = 0080h        ; other type of docking station

; PCI_CB_PROCESSOR:
PCI_CS_CPU_386            = 0000h        ; 386
PCI_CS_CPU_486            = 0001h        ; 486
PCI_CS_CPU_PENTIUM        = 0002h        ; Pentium
PCI_CS_CPU_ALPHA          = 0010h        ; Alpha
PCI_CS_CPU_POWERPC        = 0020h        ; PowerPC
PCI_CS_CPU_MIPS           = 0030h        ; MIPS
PCI_CS_CPU_COPU           = 0040h        ; co-processor

; PCI_CB_SB:
PCI_CS_BUS_FIRE           = 0000h        ; IEEE 1394 (FireWire)
PCI_CS_BUS_ACCESS         = 0001h        ; ACCESS.bus
PCI_CS_BUS_SSA            = 0002h        ; SSA
PCI_CS_BUS_USB            = 0003h        ; Universal Serial Bus (USB)
PCI_CS_BUS_FIBRE          = 0004h        ; Fibre Channel
PCI_CS_BUS_SM             = 0005h        ; SMBus (System Management Bus)
PCI_CS_BUS_INFINI         = 0006h        ; InfiniBand
PCI_CS_BUS_IPMI           = 0007h        ; IPMI
PCI_CS_BUS_SERCOS         = 0008h        ; SERCOS
PCI_CS_BUS_CANBUS         = 0009h        ; CANbus

; PCI_CB_WIRELESS:
PCI_CS_WLESS_IRDA         = 0000h        ; iRDA compatible controller
PCI_CS_WLESS_CIR          = 0001h        ; Consumer IR controller
PCI_CS_WLESS_RF           = 0010h        ; RF controller
PCI_CS_WLESS_BLUETOOTH    = 0011h        ; Bluetooth
PCI_CS_WLESS_BROADBAND    = 0012h        ; Broadband
PCI_CS_WLESS_OTHER        = 0080h        ; other type of wireless controller

;cc #endif // QS_BASE_PCI
