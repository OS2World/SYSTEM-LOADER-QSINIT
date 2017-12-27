//
// QSINIT "extcmd" module
// Advanced system dialogs shared class
//
#include "qcl/qsextdlg.h"
#include "partmgr_ord.h"
#include "qcl/qslist.h"
#include "vioext.h"
#include "malloc.h"
#include "stdlib.h"
#include "qslog.h"
#include "qscon.h"
#include "qsmod.h"
#include "qsdm.h"
#include "vio.h"

#define FMT_STR_MODELIST   "9|>7|"
#define FMT_STR_HDDLIST    "9|>10"
#define FMT_STR_PTLIST     ">7|>10|<9|4|"

static vio_listref *alloc_menu_ref(u32t count, int submenu, int ctext) {
   vio_listref *rm = (vio_listref*)malloc_th(sizeof(vio_listref));
   rm->size  = sizeof(vio_listref);
   rm->items = count; 
   rm->text  = 0;
   rm->id    = (u32t*)calloc_th(count,4);
   rm->subm  = submenu ? (vio_listref**)calloc_th(count,sizeof(vio_listref*)) : 0;
   if (ctext) {
      rm->text  = (str_list*)malloc_th(sizeof(str_list)+sizeof(char*)*count);
      rm->text->count = count+1;
      memset(&rm->text->item, 0, sizeof(char*)*rm->text->count);
   }
   return rm;
}

static void free_menu_ref(vio_listref *rm, int freeitems) {
   u32t   ii;
   if (rm->size!=sizeof(vio_listref)) return;
   rm->size = 0;
   if (rm->id) free(rm->id);
   if (rm->subm) {
      for (ii=0; ii<rm->items; ii++)
         if (rm->subm[ii]) free_menu_ref(rm->subm[ii], freeitems);
      free(rm->subm);
   }
   if (rm->text) {
      if (freeitems)
         for (ii=0; ii<rm->text->count; ii++)
            if (rm->text->item[ii]) free(rm->text->item[ii]);
      free(rm->text);
   }
   memset(rm, 0, sizeof(vio_listref));
}

static char *print_info(vio_mode_info *mi) {
   return sprintf_dyn((mi->flags&VMF_GRAPHMODE)==0 ? "%ux%u|%ux%u|text":
      "%ux%u|%ux%u|%u x %u x %u", mi->mx, mi->my, mi->fontx, mi->fonty,
         mi->gmx, mi->gmy, mi->gmbits);
}

static u32t _exicc ext_selectmodedlg(EXI_DATA, const char *header, u32t flags,
                                      int conload)
{
   vio_mode_info *ml, *mp;
   dq_list       lst;
   u32t        resid = 0, mcount;
   if (conload)
      if (!mod_query(MODNAME_CONSOLE,MODQ_NOINCR)) cmd_execint("conload", 0);
   ml  = vio_modeinfo(0, 0, 0);
   mp  = ml;
   lst = NEW(dq_list);
   while (mp->size) {
      lst->add((u64t)mp->my<<48|(u64t)mp->mx<<32|(mp->mode_id&0xFFFF)<<16
         |(mp-ml));
      mp++;
   }
   lst->sort(0,1);
   // we have sorted by height/width/modeid list of modes here
   mcount = lst->count();
   if (mcount) {
      vio_listref *rm = alloc_menu_ref(mcount, 1, 1);
      u32t         ii = 0, pos = 0;
      int   seq_start = -1;

      rm->text->item[0] = sprintf_dyn(FMT_STR_MODELIST);

      while (ii<mcount) {
         u64t  va = lst->value(ii),
             va_n = ii<mcount-1?lst->value(ii+1):0;
         
         if (va>>32==va_n>>32) {
            if (seq_start<0) seq_start = ii;
            ii++;
            continue;
         }
         if (seq_start>=0) {
            vio_mode_info *mi = ml + (u16t)va;
            u32t       scount = ii-seq_start+1, si;
            vio_listref   *sm = alloc_menu_ref(scount, 0, 1);
            rm->id[pos]       = (u32t)va>>16;
            rm->subm[pos]     = sm;
            rm->text->item[++pos] = sprintf_dyn("%ux%u||multiple", mi->mx, mi->my);
            sm->text->item[0] = sprintf_dyn(FMT_STR_MODELIST);
            for (si=0; si<scount; si++) {
               va = lst->value(seq_start+si);
               sm->id[si] = (u32t)va>>16;
               sm->text->item[si+1] = print_info(ml + (u16t)va);
            }
            seq_start = -1;
         } else {
            rm->id[pos] = (u32t)va>>16;
            rm->text->item[++pos] = print_info(ml + (u16t)va);
         }
         ii++;
      }
      // fix number of items
      rm->items = pos;
      rm->text->count = pos+1;

      resid = vio_showlist(header, rm, flags);
      free_menu_ref(rm,1);
   }
   DELETE(lst);

   return resid;
}

/* ---------------------------------------------------------------------- */

typedef dsk_mapblock* (_std *fpdsk_getmap)(u32t disk);
typedef int (_std *fplvm_partinfo )(u32t disk, u32t index, lvm_partition_data *info);
typedef u8t (_std *fpdsk_ptquery64)(u32t disk, u32t index, u64t *start, u64t *size,
                                    char *filesys, u32t *flags);

static void att_pt(fplvm_partinfo fp_lpi, fpdsk_ptquery64 fp_ptq, u32t disk,
                   u32t sector, dsk_mapblock *mb, vio_listref *mref, u32t *rcnt)
{
    lvm_partition_data pd;
    char       fsname[17], letter[3];
    pd.VolName[0] = 0;
    fsname[0]     = 0;
    // ask PARTMGR
    if (fp_lpi) fp_lpi(disk, mb->Index, &pd);
    if (fp_ptq) fp_ptq(disk, mb->Index, 0, 0, fsname, 0);

    if (!mb->DriveLVM || mb->DriveLVM=='-') letter[0] = 0; else
       { letter[0] = mb->DriveLVM; letter[1] = ':'; letter[2] = 0; }

    mref->id[*rcnt] = mb->Index+1<<16|disk+1;
    *rcnt += 1;
    mref->text->item[*rcnt] = sprintf_dyn("%s.%u|%s|%s|%s|%s", dsk_disktostr(disk, 0),
       mb->Index, dsk_formatsize(sector, mb->Length, 0, 0), fsname, letter,
          pd.VolName[0]?pd.VolName:"-");
}

/** shows "boot manager" selection dialog.
    @param header        Dialog header
    @param flags         Dialog flags (see vio_showlist() flags).
    @param type          Dialog type (see BTDT_* below)
    @param [out] disk    Disk number (on success)
    @param [out] index   Partition index (on success, can be 0 for BTDT_HDD),
                         value of -1 means big floppy or mbr (for BTDT_HDD).
    @return 0 after success selection, E_SYS_UBREAK if Esc was pressed or
            one of disk error codes */
static qserr _exicc ext_bootmgrdlg(EXI_DATA, const char *header, u32t flags,
                                   u32t type, u32t *disk, long *index)
{
   fpdsk_getmap   fp_getmap = 0;
   fplvm_partinfo fp_lpinfo = 0;
   fpdsk_ptquery64   fp_ptq = 0;
   u32t             partmgr;
   qserr                res;
   u32t                hdds = hlp_diskcount(0);
   int                noemu = type&BTDT_NOEMU?1:0;
   // drop flag
   type &= ~BTDT_NOEMU;
   // and check args
   if (!disk || !index && type!=BTDT_HDD) return E_SYS_ZEROPTR;
   if (type>BTDT_DISKTREE) return E_SYS_INVPARM;
   if (!hdds) return E_SYS_NOTFOUND;
   /* query PARTMGR every time, because we mark this module as a system,
      so leaving PARTMGR loaded forever is not a good idea. It looks better
      as optional and unloadable */
   partmgr = mod_query(MODNAME_DMGR, 0);
   if (!partmgr) partmgr = mod_searchload(MODNAME_DMGR, 0, 0);
   if (partmgr) {
      fp_getmap = (fpdsk_getmap)mod_getfuncptr(partmgr, ORD_PARTMGR_dsk_getmap);
      fp_lpinfo = (fplvm_partinfo)mod_getfuncptr(partmgr, ORD_PARTMGR_lvm_partinfo);
      fp_ptq    = (fpdsk_ptquery64)mod_getfuncptr(partmgr, ORD_PARTMGR_dsk_ptquery64);
   }
   // no partmgr?
   if (!fp_getmap) res = E_SYS_UNSUPPORTED; else {
      dsk_mapblock **map = (dsk_mapblock**)calloc_thread(hdds, sizeof(void*));
      u32t     *sectorsz = (u32t*)calloc_thread(hdds, sizeof(u32t)),
               *diskmode = (u32t*)calloc_thread(hdds, sizeof(u32t)),
                   *pcnt = (u32t*)calloc_thread(hdds, sizeof(u32t));
      u32t     ii, rdcnt = 0, inbm = 0, all = 0, id = 0;
      int        hddmenu = type==BTDT_HDD || type==BTDT_DISKTREE;

      for (ii=0; ii<hdds; ii++) {
         diskmode[ii] = hlp_diskmode(ii, HDM_QUERY);
         if (!noemu || (diskmode[ii]&HDM_EMULATED)==0)
            if ((map[ii] = fp_getmap(ii))) {
               u32t idx = 0;
               rdcnt++;
               hlp_disksize(ii, sectorsz+ii, 0);
               do {
                  if (map[ii][idx].Type) {
                     all++; pcnt[ii]++;
                     if (map[ii][idx].Flags&DMAP_BMBOOT) inbm++;
                  }
               } while ((map[ii][idx++].Flags&DMAP_LEND)==0);
            }
      }
      if (!rdcnt || type==BTDT_LVM && !inbm) {
         vio_msgbox(header, type==BTDT_HDD?"No disks to show":"No partitions to show",
            MSG_OK|MSG_RED, 0);
      } else {
         vio_listref *mref = alloc_menu_ref(type==BTDT_ALL?all:(type==BTDT_LVM?inbm:rdcnt),
                                            type!=BTDT_HDD, 1);
         mref->text->item[0] = sprintf_dyn(hddmenu?FMT_STR_HDDLIST:FMT_STR_PTLIST);

         for (ii=0, rdcnt=0; ii<hdds; ii++) 
            if (map[ii]) {
               u32t idx = 0, rpcnt = 0;
               if (type==BTDT_HDD || type==BTDT_DISKTREE) {
                  mref->id[rdcnt++]       = ii+1;
                  mref->text->item[rdcnt] = sprintf_dyn("Disk %2u|%s", ii,
                     dsk_formatsize(sectorsz[ii], hlp_disksize64(ii,0), 0, 0));
         
                  if (type==BTDT_DISKTREE) {
                     vio_listref *sm = alloc_menu_ref(pcnt[ii], 0, 1);
                     mref->subm[ii]  = sm;
                     sm->text->item[0] = sprintf_dyn(FMT_STR_PTLIST);
                  }
               }
               do {
                  if (map[ii][idx].Type) {
                     if (type==BTDT_DISKTREE)
                        att_pt(fp_lpinfo, fp_ptq, ii, sectorsz[ii], map[ii]+idx,
                           mref->subm[ii], &rpcnt); else
                     if (type==BTDT_ALL || type==BTDT_LVM && (map[ii][idx].Flags&DMAP_BMBOOT))
                        att_pt(fp_lpinfo, fp_ptq, ii, sectorsz[ii], map[ii]+idx,
                           mref, &rdcnt);
                  }
               } while ((map[ii][idx++].Flags&DMAP_LEND)==0);
            }
         
         id = vio_showlist(header, mref, flags);
         if (id) {
            *disk = (id&0xFFFF)-1;
            if (index) *index = id>>16?(id>>16)-1:-1;
            // log_printf("id = %08X\n", id);
         }
         if (mref) free_menu_ref(mref, 1);
      }
      for (ii=0; ii<hdds; ii++) 
         if (map[ii]) free(map[ii]);
      free(map); free(diskmode); free(sectorsz); free(pcnt);
      res = id?0:E_SYS_UBREAK;
   }
   if (partmgr) mod_free(partmgr);
   return res;
}

static void *ext_methods_list[] = { ext_selectmodedlg, ext_bootmgrdlg };

static void _std ext_initialize(void *instance, void *data) {}
static void _std ext_finalize(void *instance, void *data) {}

void exi_register_extcmd(void) {
   if (sizeof(ext_methods_list)!=sizeof(_qs_extcmd))
      vio_msgbox("SYSTEM WARNING!", "qs_extcmd: Fix me!", MSG_LIGHTRED|MSG_OK, 0);
   exi_register("qs_extcmd", ext_methods_list, sizeof(ext_methods_list)/sizeof(void*),
      0, 0, 0, 0, 0);
}
