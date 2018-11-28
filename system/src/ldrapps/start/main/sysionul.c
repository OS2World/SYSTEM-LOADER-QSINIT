//
// QSINIT "start" module
// "null" file system (stub for mounted unknown fs types)
// -------------------------------------
//
#include "qserr.h"
#include "qsint.h"
#include "qslog.h"
#include "qsxcpt.h"
#include "qcl/sys/qsvolapi.h"
#include "sysio.h"

#define NLFS_SIGN        0x53464C4E   ///< NLFS string

typedef struct {
   u32t           sign;
   int             vol;     ///< -1 or 0..9
   qs_sysvolume   self;     ///< ptr to self instance
} volume_data;

#define instance_ret(inst,err)             \
   volume_data *inst = (volume_data*)data; \
   if (inst->sign!=NLFS_SIGN) return err;

static qserr _exicc nulfs_initialize(EXI_DATA, u8t vol, u32t flags, void *bootsec) {
   vol_data *vdta = _extvol+vol;
   instance_ret(vd, E_SYS_INVOBJECT);

   vdta->badclus = 0;
   vdta->serial  = 0;
   vdta->clsize  = 1;
   vdta->clfree  = 0;
   vdta->cltotal = 0;
   vdta->label[0] = 0;
   vdta->fsname[0] = 0;
   vd->vol = vol;
   return E_DSK_UNKFS;
}

/// unmount volume
static qserr _exicc nulfs_finalize(EXI_DATA) {
   instance_ret(vd, E_SYS_INVOBJECT);

   if (vd->vol<0) return E_DSK_NOTMOUNTED; else {
      vd->vol = -1;
   }
   return 0;
}

static int _exicc nulfs_drive(EXI_DATA) {
   instance_ret(vd, -1);
   return vd->vol;
}

static qserr _exicc nulfs_open(EXI_DATA, const char *name, u32t mode,
                             io_handle_int *pfh, u32t *action)
{
   if (mode>SFOM_CREATE_ALWAYS) return E_SYS_INVPARM;
   if (!pfh || !name || !action) return E_SYS_ZEROPTR;
   if (strlen(name)>QS_MAXPATH) return E_SYS_LONGNAME;

   return E_DSK_UNKFS;
}

static qserr _exicc nulfs_read(EXI_DATA, io_handle_int file, u64t pos,
                             void *buffer, u32t *size)
{
   return E_SYS_INVPARM;
}

static qserr _exicc nulfs_write(EXI_DATA, io_handle_int file, u64t pos,
                              void *buffer, u32t *size)
{
   return E_SYS_INVPARM;
}

static qserr _exicc nulfs_flush(EXI_DATA, io_handle_int file) {
   return E_SYS_INVPARM;
}

static qserr _exicc nulfs_close(EXI_DATA, io_handle_int file) {
   return E_SYS_INVPARM;
}

static qserr _exicc nulfs_size(EXI_DATA, io_handle_int file, u64t *size) {
   return E_SYS_INVPARM;
}

static qserr _exicc nulfs_setsize(EXI_DATA, io_handle_int file, u64t newsize) {
   return E_SYS_INVPARM;
}

static qserr _exicc nulfs_pathinfo(EXI_DATA, const char *path, io_handle_info *info) {
   return E_DSK_UNKFS;
}

static qserr _exicc nulfs_setinfo(EXI_DATA, const char *path, io_handle_info *info, u32t flags) {
   return E_DSK_UNKFS;
}

static qserr _exicc nulfs_setexattr(EXI_DATA, const char *path, const char *aname, void *buffer, u32t size) {
   return E_SYS_UNSUPPORTED;
}

static qserr _exicc nulfs_getexattr(EXI_DATA, const char *path, const char *aname, void *buffer, u32t *size) {
   return E_SYS_UNSUPPORTED;
}

static str_list* _exicc nulfs_lstexattr(EXI_DATA, const char *path) {
   return 0;
}

// warning! it acts as rmdir too!!
static qserr _exicc nulfs_remove(EXI_DATA, const char *path) {
   return E_DSK_UNKFS;
}

static qserr _exicc nulfs_renmove(EXI_DATA, const char *oldpath, const char *newpath) {
   return E_DSK_UNKFS;
}

static qserr _exicc nulfs_makepath(EXI_DATA, const char *path) {
   return E_DSK_UNKFS;
}

static qserr _exicc nulfs_diropen(EXI_DATA, const char *path, dir_handle_int *pdh) {
   return E_DSK_UNKFS;
}

static qserr _exicc nulfs_dirnext(EXI_DATA, dir_handle_int dh, io_direntry_info *de) {
   return E_SYS_INVPARM;
}

static qserr _exicc nulfs_dirclose(EXI_DATA, dir_handle_int dh) {
   return E_SYS_INVPARM;
}

static qserr _exicc nulfs_volinfo(EXI_DATA, disk_volume_data *info, int fast) {
   instance_ret(vd, E_SYS_INVOBJECT);

   if (!info) return E_SYS_ZEROPTR;
   memset(info, 0, sizeof(disk_volume_data));

   if (vd->vol>=0) {
      vol_data  *vdta = _extvol+vd->vol;
      // mounted?
      if (vdta->flags&VDTA_ON) {
         info->StartSector  = vdta->start;
         info->TotalSectors = vdta->length;
         info->Disk         = vdta->disk;
         info->SectorSize   = vdta->sectorsize;
         info->DataSector   = 0;
         info->FSizeLim     = 0;
         info->FsVer        = FST_NOTMOUNTED;
         info->ClBad        = vdta->badclus;
         info->SerialNum    = vdta->serial;
         info->ClSize       = vdta->clsize;
         info->ClAvail      = vdta->clfree;
         info->ClTotal      = vdta->cltotal;
         strcpy(info->Label,vdta->label);
         strcpy(info->FsName,vdta->fsname);
         return 0;
      }
   }
   return E_DSK_NOTMOUNTED;
}

static qserr _exicc nulfs_setlabel(EXI_DATA, const char *label) {
   return E_DSK_UNKFS;
}

static u32t _exicc nulfs_avail(EXI_DATA) { return 0; }

static qserr _exicc nulfs_finfo(EXI_DATA, io_handle_int fh, io_direntry_info *info) {
   return E_SYS_UNSUPPORTED;
}

static void _std nulfs_init(void *instance, void *userdata) {
   volume_data *vd = (volume_data*)userdata;
   vd->sign        = NLFS_SIGN;
   vd->vol         = -1;
   vd->self        = (qs_sysvolume)instance;
}

static void _std nulfs_done(void *instance, void *userdata) {
   volume_data *vd = (volume_data*)userdata;
   if (vd->sign==NLFS_SIGN)
      if (vd->vol>=0) log_printf("Volume %d fini???\n", vd->vol);
   memset(vd, 0, sizeof(volume_data));
}

static void *m_list[] = { nulfs_initialize, nulfs_finalize, nulfs_avail,
   nulfs_volinfo, nulfs_setlabel, nulfs_drive, nulfs_open, nulfs_read,
   nulfs_write, nulfs_flush, nulfs_close, nulfs_size, nulfs_setsize,
   nulfs_finfo, nulfs_setexattr, nulfs_getexattr, nulfs_lstexattr,
   nulfs_remove, nulfs_renmove, nulfs_makepath, nulfs_remove, nulfs_pathinfo,
   nulfs_setinfo, nulfs_diropen, nulfs_dirnext, nulfs_dirclose };

extern u32t nulfs_classid;

void register_nulfs(void) {
   if (sizeof(_qs_sysvolume)!=sizeof(m_list)) _throwmsg_("nulfs: size mismatch");
   // register private(unnamed) class
   nulfs_classid = exi_register("qs_sys_nulfs", m_list, sizeof(m_list)/sizeof(void*),
      sizeof(volume_data), EXIC_PRIVATE, nulfs_init, nulfs_done, 0);
   if (!nulfs_classid) _throwmsg_("nulfs - reg.error");
}
