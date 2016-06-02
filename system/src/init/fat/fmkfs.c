//
// QSINIT
// own f_mkfs - smaller, less checks, FAT12/16 only - for virtual disk
//

#include "qsint.h"

// ram-disk memory is zero-filled, so some init ops skipped to save code size
#define MEM_ZEROED

#define N_ROOTDIR	128		/* Number of root dir entries for FAT12/16 */
#define N_FATS		1		/* Number of FAT copies (1 or 2) */

FRESULT f_mkfs2(void) {
	static const WORD vst[] = { 1024,   512,  256,  128,   64,    32,   16,    8,    4,    2,   0};
	static const WORD cst[] = {32768, 16384, 8192, 4096, 2048, 16384, 8192, 4096, 2048, 1024, 512};
	BYTE fmt, md, sys, *tbl, pdrv, part, vol = DISK_LDR;
	DWORD n_clst, vs, n, wsect;
	UINT i, au;
	DWORD b_vol, b_fat, b_dir, b_data;        /* LBA */
	DWORD n_vol, n_rsv, n_fat, n_dir, *lpdt;  /* Size */
	FATFS *fs;
	DSTATUS stat;
	/* Check mounted drive and clear work area */
	//if (vol >= _VOLUMES) return FR_INVALID_DRIVE;
	fs = FatFs[vol];
	if (!fs) return FR_NOT_ENABLED;
	fs->fs_type = 0;
	pdrv = LD2PD(vol);	/* Physical drive */
	part = LD2PT(vol);	/* Partition (0:auto detect, 1-4:get from partition table)*/

	/* Get disk statics */
	stat = disk_initialize(pdrv);
	if (stat & STA_NOINIT) return FR_NOT_READY;
	if (stat & STA_PROTECT) return FR_WRITE_PROTECTED;
#if _MAX_SS != _MIN_SS		/* Get disk sector size */
	if (disk_ioctl(pdrv, GET_SECTOR_SIZE, &SS(fs)) != RES_OK || SS(fs) > _MAX_SS)
		return FR_DISK_ERR;
#endif
	/* Create a partition in this function */
	if (disk_ioctl(pdrv, GET_SECTOR_COUNT, &n_vol) != RES_OK || n_vol < 128)
		return FR_DISK_ERR;
	b_vol = 0;					/* Volume start sector */
	n_vol -= b_vol;				/* Volume size */

	/* AU auto selection */
	vs = n_vol / (2000 / (SS(fs) / 512));
	for (i = 0; vs < vst[i]; i++) ;
	au = cst[i];

	au /= SS(fs);				/* Number of sectors per cluster */
	if (au == 0) au = 1;
	if (au > 128) au = 128;

	/* Pre-compute number of clusters and FAT sub-type */
	n_clst = n_vol / au;
	fmt = FS_FAT12;
	if (n_clst >= MIN_FAT16) fmt = FS_FAT16;
	if (n_clst >= MIN_FAT32) return FR_MKFS_ABORTED;

	/* Determine offset and size of FAT structure */
	n_fat = (fmt == FS_FAT12) ? (n_clst * 3 + 1) / 2 + 3 : (n_clst * 2) + 4;
	n_fat = (n_fat + SS(fs) - 1) / SS(fs);
	n_rsv = 1;
	n_dir = (DWORD)N_ROOTDIR * SZ_DIRE / SS(fs);

	b_fat = b_vol + n_rsv;				/* FAT area start sector */
	b_dir = b_fat + n_fat * N_FATS;		/* Directory area start sector */
	b_data = b_dir + n_dir;				/* Data area start sector */
	if (n_vol < b_data + au - b_vol) return FR_MKFS_ABORTED;	/* Too small volume */

	/* Determine number of clusters and final check of validity of the FAT sub-type */
	n_clst = (n_vol - n_rsv - n_fat * N_FATS - n_dir) / au;
	if (fmt == FS_FAT16 && n_clst < MIN_FAT16) return FR_MKFS_ABORTED;

	md = 0xF8;

	/* Create BPB in the VBR */
	tbl = fs->win;							/* Clear sector */
	mem_set(tbl, 0, SS(fs));

	mem_cpy(tbl, "\xEB\xFE\x90" "MSDOS5.0", 11);/* Boot jump code, OEM name */
	i = SS(fs);								/* Sector size */
	ST_WORD(tbl+BPB_BytsPerSec, i);
	tbl[BPB_SecPerClus] = (BYTE)au;			/* Sectors per cluster */
	ST_WORD(tbl+BPB_RsvdSecCnt, n_rsv);		/* Reserved sectors */
	tbl[BPB_NumFATs] = N_FATS;				/* Number of FATs */
	ST_WORD(tbl+BPB_RootEntCnt, N_ROOTDIR);	/* Number of rootdir entries */
	if (n_vol < 0x10000) {					/* Number of total sectors */
		ST_WORD(tbl+BPB_TotSec16, n_vol);
	} else {
		ST_DWORD(tbl+BPB_TotSec32, n_vol);
	}
	tbl[BPB_Media] = md;					/* Media descriptor */
	ST_WORD(tbl+BPB_SecPerTrk, 63);			/* Number of sectors per track */
	ST_WORD(tbl+BPB_NumHeads, 255);			/* Number of heads */
	ST_DWORD(tbl+BPB_HiddSec, b_vol);		/* Hidden sectors */
	n = get_fattime();						/* Use current time as VSN */
	ST_DWORD(tbl+BS_VolID, n);				/* VSN */
	ST_WORD(tbl+BPB_FATSz16, n_fat);		/* Number of sectors per FAT */
	tbl[BS_DrvNum] = 0x80;					/* Drive number */
	tbl[BS_BootSig] = 0x29;					/* Extended boot signature */

	lpdt = (DWORD*)(tbl+BS_FilSysType);
	*lpdt++ = 0x20544146;                   /* Volume label, FAT signature */
	*lpdt   = 0x20202020;

	ST_WORD(tbl+BS_55AA, 0xAA55);			/* Signature (Offset is fixed here regardless of sector size) */
	if (disk_write(pdrv, tbl, b_vol, 1) != RES_OK)	/* Write it to the VBR sector */
		return FR_DISK_ERR;
	// Initialize FAT area
	wsect = b_fat;
	for (i = 0; i < N_FATS; i++) {		/* Initialize each FAT copy */
		mem_set(tbl, 0, SS(fs));			/* 1st sector of the FAT  */
		n = md;								/* Media descriptor byte */
		n |= (fmt == FS_FAT12) ? 0x00FFFF00 : 0xFFFFFF00;
		ST_DWORD(tbl+0, n);					/* Reserve cluster #0-1 (FAT12/16) */

		if (disk_write(pdrv, tbl, wsect++, 1) != RES_OK)
			return FR_DISK_ERR;
#ifndef MEM_ZEROED
		mem_set(tbl, 0, SS(fs));			/* Fill following FAT entries with zero */
		for (n = 1; n < n_fat; n++) {
			if (disk_write(pdrv, tbl, wsect++, 1) != RES_OK)
				return FR_DISK_ERR;
		}
#endif
	}
#ifndef MEM_ZEROED
	/* Initialize root directory */
	i = n_dir;
	do {
		if (disk_write(pdrv, tbl, wsect++, 1) != RES_OK)
			return FR_DISK_ERR;
	} while (--i);
#endif
	return FR_OK;
}
