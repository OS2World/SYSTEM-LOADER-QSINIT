//
// QSINIT "partmgr" module
// OS/2 LVM DLAT sector
//
#ifndef QSINIT_LVMINFO
#define QSINIT_LVMINFO

#ifdef __cplusplus
extern "C" {
#endif

#define DLA_TABLE_SIGNATURE1  0x424D5202
#define DLA_TABLE_SIGNATURE2  0x44464D50

#define LVM_NAME_SIZE         20

typedef struct {
   u32t      Volume_Serial;                  // The serial number of the volume.
   u32t      Partition_Serial;               // The serial number of this partition.
   u32t      Partition_Size;                 // The size of the partition, in sectors.
   u32t      Partition_Start;                // The starting sector of the partition.
   u8t       On_Boot_Manager_Menu;           // Volume/partition is on the Boot Manager Menu.
   u8t       Installable;                    // Installable volume.
   char      Drive_Letter;                   // The drive letter assigned to the partition.
   u8t       Reserved;
   char      Volume_Name[LVM_NAME_SIZE];     // The name assigned to the volume by the user.
   char      Partition_Name[LVM_NAME_SIZE];  // The name assigned to the partition.
} DLA_Entry;

typedef struct {
   u32t      DLA_Signature1;                 // Signature #1
   u32t      DLA_Signature2;                 // Signature #2
   u32t      DLA_CRC;                        /* The 32 bit CRC for this sector. Calculated assuming that
                                                this field and all unused space in the sector is 0. */
   u32t      Disk_Serial;                    // The serial number assigned to this disk.
   u32t      Boot_Disk_Serial;               /* The serial number of the disk used to boot the system.
                                                This is for conflict resolution when multiple volumes
                                                want the same drive letter. Since LVM.EXE will not let
                                                this situation happen, the only way to get this situation
                                                is for the disk to have been altered by something other 
                                                than LVM.EXE, or if a disk drive has been moved from one
                                                machine to another.  If the drive has been moved, then it
                                                should have a different Boot_Disk_Serial_Number. Thus,
                                                we can tell which disk drive is the "foreign" drive and
                                                therefore reject its claim for the drive letter in
                                                question. If we find that all of the claimaints have
                                                the same Boot_Disk_Serial, then we must assign drive
                                                letters on a first come, first serve basis. */
   u32t      Install_Flags;                  // Used by the Install program.
   u32t      Cylinders;
   u32t      Heads_Per_Cylinder;
   u32t      Sectors_Per_Track;
   char      Disk_Name[LVM_NAME_SIZE];       // The name assigned to the disk containing this sector.
   u8t       Reboot;                         // Used to keep track of reboots initiated by install.
   u8t       Reserved[3];                    // Alignment.
   DLA_Entry DLA_Array[4];                   // Partition table entries.
} DLA_Table_Sector;

/** delete partition's record from DLAT
    @param [in]  disk     disk number
    @param [in]  index    partition index
    @return 0 on success or LVME_* error code. */
int  _std lvm_delptrec(u32t disk, u32t index);

#define LVM_INITCRC  FFFF

/// CRC for LVM DLAT sector
u32t _std lvm_crc32(u32t crc, void *Buffer, u32t BufferSize);

/** wipe DLAT sector.
    Sector will be not wiped actually, but written with removed signatures.
    @param disk     disk number
    @param quadidx  dlat index (0 for primary, 1..x - extended)
    @return 0 on success or LVME_* error code. */
int  _std lvm_wipedlat(u32t disk, u32t quadidx);

#ifdef __cplusplus
}
#endif

#endif // QSINIT_LVMINFO
