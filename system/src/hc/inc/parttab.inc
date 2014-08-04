;
; Partition table & boot records (OS/2 bpb defined in ioint13.inc)
;

;cc #ifndef QS_BASE_PARTTABLE
;cc #define QS_BASE_PARTTABLE

MBR_Record      struc
PTE_Active      db      ?                 ;/< 80h for active
PTE_HStart      db      ?                 ;/< chs of FFh
PTE_CSStart     dw      ?                 ;
PTE_Type        db      ?                 ;
PTE_HEnd        db      ?                 ;
PTE_CSEnd       dw      ?                 ;
PTE_LBAStart    dd      ?                 ;/< lba start
PTE_LBASize     dd      ?                 ;/< lba len or -1
MBR_Record      ends

Disk_MBR        struc
MBR_Code        db      440 dup(?)        ;/< boot code
MBR_DiskID      dd      ?                 ;/< windows disk ID
MBR_Reserved    dw      ?                 ;/< unused
;cr struct MBR_Record          MBR_Table[4];
MBR_Table       db 4 * size MBR_Record dup (?)   ;
MBR_Signature   dw      ?                 ;/< AA55h
Disk_MBR        ends

Disk_GPT        struc
GPT_Sign        dq      ?                 ;/< signature
GPT_Revision    dd      ?                 ;
GPT_HdrSize     dd      ?                 ;/< header size (92)
GPT_HdrCRC      dd      ?                 ;/< CRC32 of header
GPT_Reserved1   dd      ?                 ;
GPT_Hdr1Pos     dq      ?                 ;/< pos of this header
GPT_Hdr2Pos     dq      ?                 ;/< pos of backup header
GPT_UserFirst   dq      ?                 ;/< first usable sector
GPT_UserLast    dq      ?                 ;/< last usable sector
GPT_GUID        db      16 dup(?)         ;
GPT_PtInfo      dq      ?                 ;/< first sector with partitions
GPT_PtCout      dd      ?                 ;/< number of entries
GPT_PtEntrySize dd      ?                 ;/< size of pt record (128)
GPT_PtCRC       dd      ?                 ;/< CRC32 of partition entries
Disk_GPT        ends

GPT_Record      struc
PTG_TypeGUID    db      16 dup(?)         ;/< partition type GUID
PTG_GUID        db      16 dup(?)         ;/< partition GUID
PTG_FirstSec    dq      ?                 ;/< first sector
PTG_LastSec     dq      ?                 ;/< last sector
PTG_Attrs       dq      ?                 ;/< attributes
PTG_Name        dw      36 dup(?)         ;/< readable name
GPT_Record      ends

;cc #define GPT_SIGNMAIN 0x5452415020494645LL
GPT_SIGNDDLO      = 20494645h             ;
GPT_SIGNDDHI      = 54524150h             ;
GPT_RECSIZE       = 128                   ;< Common partition record size

GPTATTR_SYSTEM    = 0                     ;< system partition
GPTATTR_IGNORE    = 1                     ;<
GPTATTR_BIOSBOOT  = 2                     ;< Legacy BIOS bootable
GPTATTR_MS_RO     = 60                    ;< MS Basic data: read-only
GPTATTR_MS_HIDDEN = 62                    ;< MS Basic data: hidden
GPTATTR_MS_NOMNT  = 63                    ;< MS Basic data: do not automount

PTE_00_FREE      = 000h                   ;/< Free partition
PTE_01_FAT12     = 001h                   ;/< FAT12, CHS (all CHS -> offset < 4Gb)
PTE_02_XENIX     = 002h                   ;/< Xenix
PTE_03_XENIX     = 003h                   ;/< Xenix
PTE_04_FAT16     = 004h                   ;/< FAT16, CHS
PTE_05_EXTENDED  = 005h                   ;/< extended, CHS
PTE_06_FAT16     = 006h                   ;/< FAT16 Big, CHS
PTE_07_MSFS      = 007h                   ;/< Microsoft: HPFS, NTFS, exFAT
PTE_0A_BOOTMGR   = 00Ah                   ;/< OS/2 Boot Manager
PTE_0B_FAT32     = 00Bh                   ;/< FAT32, CHS
PTE_0C_FAT32     = 00Ch                   ;/< FAT32, LBA
PTE_0E_FAT16     = 00Eh                   ;/< FAT16 Big, LBA
PTE_0F_EXTENDED  = 00Fh                   ;/< extended, LBA
PTE_11_FAT12     = 011h                   ;/< FAT12, CHS, Hidden (0x01)
PTE_12_FAT       = 012h                   ;/< Service FAT partition
PTE_14_FAT16     = 014h                   ;/< FAT16, CHS, Hidden (0x04)
PTE_15_EXTENDED  = 015h                   ;/< extended, CHS, Hidden (0x05)
PTE_16_FAT16     = 016h                   ;/< FAT16 Big, CHS, Hidden (0x06)
PTE_17_MSFS      = 017h                   ;/< Microsoft Hidden: HPFS, NTFS, exFAT (0x07)
PTE_1B_FAT32     = 01Bh                   ;/< FAT32, CHS, Hidden (0x0B)
PTE_1C_FAT32     = 01Ch                   ;/< FAT32, LBA, Hidden (0x0C)
PTE_1E_FAT16     = 01Eh                   ;/< FAT16 Big, LBA, Hidden (0x0E)
PTE_1F_EXTENDED  = 01Fh                   ;/< extended, LBA, Hidden (0x0F)
PTE_27_MSFS      = 027h                   ;/< Microsoft Hidden: HPFS, NTFS, exFAT (0x07)
PTE_2A_ATHEOS    = 02Ah                   ;/< AtheOS
PTE_2B_ATHEOS    = 02Bh                   ;/< AtheOS
PTE_35_JFS       = 035h                   ;/< JFS
PTE_42_WINDYN    = 042h                   ;/< Windows Dynamic
PTE_63_UNIX      = 063h                   ;/< Unix
PTE_64_NOVELL    = 064h                   ;/< Novell
PTE_65_NOVELL    = 065h                   ;/< Novell
PTE_66_NOVELL    = 066h                   ;/< Novell
PTE_67_NOVELL    = 067h                   ;/< Novell
PTE_68_NOVELL    = 068h                   ;/< Novell
PTE_69_NOVELL    = 069h                   ;/< Novell
PTE_7F_EXP       = 07Fh                   ;/< Experimental
PTE_80_MINIX     = 080h                   ;/< Minix
PTE_81_MINIX     = 081h                   ;/< Minix
PTE_82_LINUX     = 082h                   ;/< Linux
PTE_83_LINUX     = 083h                   ;/< Linux
PTE_84_FAT16     = 084h                   ;/< FAT16, CHS, Hidden (0x04)
PTE_85_EXTENDED  = 085h                   ;/< Linux extended (0x05)
PTE_86_FAT16     = 086h                   ;/< WinNT stripe FAT16 Big (0x06)
PTE_87_NTFS      = 087h                   ;/< WinNT stripe NTFS
PTE_8B_FAT32     = 08Bh                   ;/< WinNT stripe FAT32
PTE_8C_FAT32     = 08Ch                   ;/< WinNT stripe FAT32, LBA (0x0C)
PTE_8D_FAT12     = 08Dh                   ;/< FAT12, CHS, Hidden (0x01)
PTE_8E_LINUX     = 08Eh                   ;/< Linux LVM
PTE_90_FAT16     = 090h                   ;/< FAT16, CHS, Hidden (0x04)
PTE_91_EXTENDED  = 091h                   ;/< extended, CHS, Hidden (0x05)
PTE_92_FAT16     = 092h                   ;/< FAT16 Big, CHS, Hidden (0x06)
PTE_93_LINUX     = 093h                   ;/< Hidden Linux
PTE_97_FAT32     = 097h                   ;/< FAT32, CHS, Hidden (0x0B)
PTE_98_FAT32     = 098h                   ;/< FAT32, LBA, Hidden (0x0C)
PTE_9A_FAT16     = 09Ah                   ;/< FAT16 Big, LBA, Hidden (0x0E)
PTE_9B_EXTENDED  = 09Bh                   ;/< extended, LBA, Hidden (0x0F)
PTE_A5_BSD       = 0A5h                   ;/< BSD  
PTE_A6_BSD       = 0A6h                   ;/< BSD  
PTE_A8_MACOS     = 0A8h                   ;/< MacOS
PTE_A9_BSD       = 0A9h                   ;/< BSD  
PTE_AB_MACOS     = 0ABh                   ;/< MacOS
PTE_B1_QNX       = 0B1h                   ;/< QNX  
PTE_B2_QNX       = 0B1h                   ;/< QNX  
PTE_B3_QNX       = 0B1h                   ;/< QNX  
PTE_DB_FAT16     = 0DBh                   ;/< Dell FAT
PTE_DE_FAT16     = 0DEh                   ;/< Dell FAT
PTE_EB_BEOS      = 0EBh                   ;/< BeOS
PTE_EE_UEFI      = 0EEh                   ;/< UEFI
PTE_EF_UEFI      = 0EFh                   ;/< UEFI
PTE_F8_VMWARE    = 0F8h                   ;/< VMware
PTE_FC_VMWARE    = 0FCh                   ;/< VMware
PTE_FF_XENIX     = 0FFh                   ;/< Xenix

;/< --------------------------------------------------------------------------

Common_BPB      struc
BPB_BytePerSect dw      ?                 ;/< physical sector size      
BPB_SecPerClus  db      ?                 ;/< sectors per cluster
BPB_ResSectors  dw      ?                 ;/< number of reserved sectors
BPB_FATCopies   db      ?                 ;/< number of FAT copies
BPB_RootEntries dw      ?                 ;/< number of root dir entries
BPB_TotalSec    dw      ?                 ;/< total number of sectors
BPB_MediaByte   db      ?                 ;/< media descriptor byte
BPB_SecPerFAT   dw      ?                 ;/< number of sectors in one FAT copy
BPB_SecPerTrack dw      ?                 ;/< sectors per track
BPB_Heads       dw      ?                 ;/< number of heads
BPB_HiddenSec   dd      ?                 ;/< number of hidden sectors
BPB_TotalSecBig dd      ?                 ;/< total sectors (if BPB_TotalSectors = 0)
Common_BPB      ends

Extended_BPB    struc
EBPB_PhysDisk   db      ?                 ;/< physical drive number 
EBPB_Dirty      db      ?                 ;/< dirty flag (bit 1)
EBPB_Sign       db      ?                 ;/< 0x29 (can be 0x28)
EBPB_VolSerial  dd      ?                 ;/< volume serial number
EBPB_VolLabel   db      11 dup (?)        ;/< volume label
EBPB_FSType     db      8 dup (?)         ;/< volume label
Extended_BPB    ends

ExtF32_BPB      struc
FBPB_SecPerFAT  dd      ?                 ;/< number of sectors in one FAT copy
FBPB_ActiveCopy dw      ?                 ;/< active copy of FAT (bit 0..3)
FBPB_Version    dw      ?                 ;/< version (should be 0)
FBPB_RootClus   dd      ?                 ;/< first cluster of root directory
FBPB_FSInfo     dw      ?                 ;/< logical sector with FSInfo
FBPB_BootCopy   dw      ?                 ;/< logical sector of 3 boot sectors copy
FBPB_Reserved   db      12 dup (?)        ;
ExtF32_BPB      ends

Boot_Record     struc
BR_Jump         db      3 dup (?)         ;/< 
BR_OEM          db      8 dup (?)         ;
BR_BPB          Common_BPB    <?>         ;
BR_EBPB         Extended_BPB  <?>         ;
Boot_Record     ends

Boot_RecordF32  struc
BR_Jump         db      3 dup (?)         ;/< 
BR_OEM          db      8 dup (?)         ;
BR_BPB          Common_BPB    <?>         ;
BR_F32_BPB      ExtF32_BPB    <?>         ;
BR_F32_EBPB     Extended_BPB  <?>         ;
Boot_RecordF32  ends

;cc #endif // QS_BASE_PARTTABLE
