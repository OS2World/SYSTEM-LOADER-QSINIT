#ifndef DISKIO_API
#define DISKIO_API

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__WATCOMC__)||defined(_MSC_VER)
typedef unsigned __int64   uq;
typedef __int64            sq;
#else
typedef unsigned long long uq;
typedef long long          sq;
#endif
typedef unsigned long      ul;

/** return number of installed hard disks in system.
    @param  [out]  floppies - ptr to number of floppy disks, can be 0.
    @result number of hard disks */
ul dsk_count(ul *floppies,ul *vdisks);

#define DSK_FLOPPY    0x080    ///< add to disk number to index floppy disks
#define DSK_VIRTUAL   0x100    ///< add to disk number to index QSINIT memory disks
#define DSK_DISKMASK  0x07F    ///< disk index mask

typedef struct {
   ul        Cylinders;
   ul            Heads;
   ul      SectOnTrack;
   ul       SectorSize;
   uq     TotalSectors;
} dsk_geo_data;

/** return disk size.
    @param [in]   disk      Disk number.
    @param [out]  sectsize  Size of disk sector
    @param [out]  geo       Disk data (optional, can be 0, can return zeroed data)
    @result number of sectors on disk */
ul dsk_size(ul disk, ul *sectsize, dsk_geo_data *geo);

/** return 64-bit disk size.
    @param [in]   disk      Disk number.
    @param [out]  sectsize  Size of disk sector
    @result number of sectors on disk */
uq dsk_size64(ul disk, ul *sectsize);

/** read physical disk.
    @param  disk      Disk number.
    @param  sector    Start sector
    @param  count     Number of sectors to read
    @param  data      Buffer for data
    @result number of sectors was actually readed */
ul dsk_read(ul disk, uq sector, ul count, void *data);

/** write to physical disk.
    Be careful with QSEL_VIRTUAL flag! 0x100 value mean boot partition, 0x101 - 
    virtual disk with QSINIT apps!

    @param  disk      Disk number.
    @param  sector    Start sector
    @param  count     Number of secvtors to write
    @param  data      Buffer with data
    @result number of sectors was actually written */
ul dsk_write(ul disk, uq sector, ul count, void *data);

#ifdef __cplusplus
}
#endif
#endif // DISKIO_API
