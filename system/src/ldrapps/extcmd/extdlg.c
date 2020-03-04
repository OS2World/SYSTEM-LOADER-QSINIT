//
// QSINIT "extcmd" module
// Advanced system dialogs shared class
// --------------------------------------
// all system modules here loaded dynamically because this module is
// never released and static linking will force the same for any used.
//
#include "qcl/qsextdlg.h"
#include "partmgr_ord.h"
#include "console_ord.h"
#include "qcl/qslist.h"
#include "qcl/qsmt.h"
#include "qspdata.h"
#include "console.h"
#include "vioext.h"
#include "malloc.h"
#include "stdlib.h"
#include "qslog.h"
#include "qsint.h"
#include "qscon.h"
#include "limits.h"
#include "qsmodext.h"
#include "qsdm.h"
#include "vio.h"

#define FMT_STR_GMLIST     ">|<"
#define FMT_STR_MODELIST   "9|>7|"
#define FMT_STR_HDDLIST    "9|>10"
#define FMT_STR_PTLIST1    ">|>10|<9|4|<"
#define FMT_STR_PTLIST2    "4|" FMT_STR_PTLIST1

/** menu/submenu allocation.
    @param count            # of items
    @param alloc_submenu    allocate submenu list (i.e. 0 menu submenu itself)
    @param ctext            allocate list for text items (1/0)
    @param opts             VLSF_* flags for submenu
    @param nkey             number of additional keys (addkey array)
    @return allocated list in thread-owned memory */
static vio_listref *alloc_menu_ref(u32t count, int alloc_submenu, int ctext, u32t opts, u32t nkey) {
   vio_listref *rm = (vio_listref*)malloc_th(sizeof(vio_listref)+nkey*sizeof(vio_listkey));
   rm->size    = sizeof(vio_listref);
   rm->items   = count;
   rm->text    = 0;
   rm->id      = (u32t*)calloc_th(count,4);
   rm->subm    = alloc_submenu ? (vio_listref**)calloc_th(count,sizeof(vio_listref*)) : 0;
   if (!alloc_submenu) {
      rm->type = opts;
      if ((opts&VLSF_LOCMASK)==VLSF_CENTOFSLINE || (opts&VLSF_LOCMASK)==VLSF_LINEOFS) {
         rm->posdx   = -1;
         rm->posdy   = 0;
      }
   }
   memset(&rm->akey, 0, sizeof(vio_listkey)*(nkey+1));

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

/** get single MT lib instance for this module.
    We should not care about release, because our module deny unload.
    This is, also, the reason to use shared class - else static linking
    will force MTLIB presence to be loaded. */
qs_mtlib get_mtlib(void) {
   static qs_mtlib mt = 0;
   if (!mt) {
      mt_swlock();
      mt = NEW_G(qs_mtlib);
      mt_swunlock();
   }
   return mt;
}

typedef con_mode_info* _std (*fpcon_gmlist)(void);

#define BITS5  0x80
#define BITS6  0xA0
#define BITS8  0xE0

#define GMODE_FILTER (SMDT_GRF8BIT|SMDT_GRF15BIT|SMDT_GRF16BIT|SMDT_GRF24BIT|SMDT_GRF32BIT)

/** shows video mode selection dialog.
    Use SMDT_TEXTADV to make sure, that CONSOLE module is loaded. SMDT_TEXT
    will show existing modes (VGA/UEFI text modes if no CONSOLE loaded).

    SMDT_TEXTNATIVE, SMDT_GRFNATIVE and SMDT_NATIVE use CONSOLE in any way
    and return list of native modes only (without emulated graphics console
    modes).

    "mode con gmlist" command is equal to SMDT_GRFNATIVE selection.

    @param header        Dialog header
    @param flags         Dialog flags (see vio_showlist() flags).
    @param type          Dialog type (one of SMDT_* below).

    @return mode_id or 0 if esc was presses */
static u32t _exicc ext_selectmodedlg(EXI_DATA, const char *header, u32t flags,
                                     u32t type)
{
   fpcon_gmlist fp_gmlist = 0;
   vio_mode_info      *ml, *mp;
   dq_list            lst;
   u32t             resid = 0, mcount;
   // bit 0 in type means "load CONSOLE"
   if (type&1) {
      for (mcount=0; mcount<2; mcount++) {
         u32t console = mod_query(MODNAME_CONSOLE, MODQ_NOINCR);
         if (console) {
            fp_gmlist = (fpcon_gmlist)mod_getfuncptr(console, ORD_CONSOLE_con_gmlist, 0);
            break;
         } else
            // let the shell will load it, using EXTCMD.INI rules.
            cmd_execint("conload", 0);
      }
      // SMDT_TEXTNATIVE without CONSOLE still can be listed
      if (!fp_gmlist)
         if ((type&0xF)==SMDT_TEXTNATIVE) type = SMDT_TEXT;
            else return 0;
   }
   lst = NEW(dq_list);
   // bit 1 & 2 - native mode list asked
   if (type&6) {
      con_mode_info *cl = fp_gmlist(), *cp;
      // only text?
      if ((type&4)==0) {
         for (mcount = 0, cp = cl; cp->size; cp++)
            if ((cp->flags&VMF_GRAPHMODE)==0) mcount++;
         // convert to list for text mode processing below
         ml = (vio_mode_info*)malloc_th(sizeof(vio_mode_info)*mcount + 4);
         mem_zero(ml);

         for (mcount = 0, cp = cl; cp->size; cp++)
            if ((cp->flags&VMF_GRAPHMODE)==0) {
               ml[mcount].size    = VIO_MI_FULL;
               ml[mcount].mx      = cp->mx;
               ml[mcount].my      = cp->my;
               ml[mcount].fontx   = 8;
               ml[mcount].fonty   = cp->my>25?8:16;
               ml[mcount].flags   = cp->flags;
               ml[mcount].mode_id = cp->mode_id;
               mcount++;
            }
         free(cl);
      } else {
         /* native modes list processing */
         cp  = cl;
         while (cp->size) {
            lst->add((u64t)cp->my<<48|(u64t)cp->mx<<32|(cp->mode_id&0xFFFF)<<16|(cp-cl));
            cp++;
         }
         lst->sort(0,1);

         mcount = lst->count();
         if (mcount) {
            static u32t fmt[] = { (BITS8|16)<<16 | (BITS8|8)<<8 | (BITS8|0),
                                  (BITS8|0)<<16 | (BITS8|8)<<8 | (BITS8|16),
                                  (BITS5|10)<<16 | (BITS5|5)<<8 | (BITS5|0),
                                  (BITS5|11)<<16 | (BITS6|5)<<8 | (BITS5|0),
                                  (BITS5|0)<<16 | (BITS5|5)<<8 | (BITS5|10),
                                  (BITS5|0)<<16 | (BITS6|5)<<8 | (BITS5|11) };
            char     *fmn[] = { "RGB888", "BGR888", "RGB555", "RGB565", "BGR555", "BGR565" };
            vio_listref *rm = alloc_menu_ref(mcount, 0, 1, 0, 0);
            u32t   cidx, ii, pos;

            rm->text->item[0] = sprintf_dyn(FMT_STR_GMLIST);
        
            for (ii=0, pos=0; ii<mcount; ii++) {
               const char *fmtstr = "";
               char       hgt_str[16];
               u64t            va = lst->value(ii);
               u32t            sv = 0;
               int          gmode;
               cp    = cl + (u16t)va;
               gmode = cp->flags&VMF_GRAPHMODE;

               if (gmode) {
                  // filter bits are present
                  if (type&GMODE_FILTER)
                     switch (cp->bits) {
                        case  8: if (type& SMDT_GRF8BIT) break; else continue;
                        case 15: if (type&SMDT_GRF15BIT) break; else continue;
                        case 16: if (type&SMDT_GRF16BIT) break; else continue;
                        case 24: if (type&SMDT_GRF24BIT) break; else continue;
                        case 32: if (type&SMDT_GRF32BIT) break; else continue;
                        default:
                          /* bit presence means "only asked" logic, so any
                             unknown just denied */
                          continue;
                     }
                  /* 32-bit modes will also be shown as RGB888, as well as
                     exotic ARGB 1555 as RGB 555 */
                  for (cidx=0; cidx<3; cidx++) {
                     u32t src = cidx==2?cp->rmask:(cidx?cp->gmask:cp->bmask),
                           vm = 0;
                     int   lp = bsr32(src),
                           rp = bsf32(src);
                     if (lp<0) { sv=0; break; }
                     sv |= (lp-rp<<5|rp)<<(cidx<<3);
                  }
                  if (sv) {
                     u32t *pos = memchrd(fmt, sv, sizeof(fmt)/4);
                     if (pos) fmtstr = fmn[pos-fmt];
                  }
               } else // bit means including of text modes
               if ((type&2)==0) continue;
               // cvt to string to make "x" char at the same pos over list
               sprintf(hgt_str, "%u", cp->my);

               rm->id[pos] = (u32t)va>>16;
               rm->text->item[++pos] = sprintf_dyn(gmode?(*fmtstr? 
                  "%u x %-4s|%2u (%s)":"%u x %-4s|%2u"): "%u x %-4s|text",
                     cp->mx, hgt_str, cp->bits, fmtstr);
            }
            key_clear();
            // fix number of items
            rm->items = pos;
            rm->text->count = pos+1;
            resid = vio_showlist(header, rm, flags, 0);
            free_menu_ref(rm,1);
         }
         DELETE(lst);
         free(cl);
         return resid;
      }
   } else
      ml  = vio_modeinfo(0, 0, 0);
   /* text mode list processing with graphics console support */
   mp  = ml;
   while (mp->size) {
      lst->add((u64t)mp->my<<48|(u64t)mp->mx<<32|(mp->mode_id&0xFFFF)<<16|(mp-ml));
      mp++;
   }
   lst->sort(0,1);
   // we have sorted by height/width/modeid list of modes here
   mcount = lst->count();
   if (mcount) {
      vio_listref *rm = alloc_menu_ref(mcount, 1, 1, 0, 0);
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
            vio_listref   *sm = alloc_menu_ref(scount, 0, 1, VLSF_LINEOFS, 0);
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
      key_clear();
      // fix number of items
      rm->items = pos;
      rm->text->count = pos+1;
      resid = vio_showlist(header, rm, flags, 0);
      free_menu_ref(rm,1);
   }
   DELETE(lst);
   free(ml);

   return resid;
}

/* ---------------------------------------------------------------------- */

typedef dsk_mapblock* (_std *fpdsk_getmap)(u32t disk);
typedef int (_std *fplvm_partinfo )(u32t disk, u32t index, lvm_partition_data *info);
typedef u8t (_std *fpdsk_ptquery64)(u32t disk, u32t index, u64t *start, u64t *size,
                                    char *filesys, u32t *flags);

static void att_pt(fplvm_partinfo fp_lpi, fpdsk_ptquery64 fp_ptq, u32t disk,
                   u32t sector, dsk_mapblock *mb, vio_listref *mref,
                   u32t *rcnt, int qstab)
{
    lvm_partition_data pd;
    char       fsname[17], letter[3], qsletter[7];
    pd.VolName[0] = 0;
    fsname[0]     = 0;
    // ask PARTMGR
    if (fp_lpi) fp_lpi(disk, mb->Index, &pd);
    if (fp_ptq) fp_ptq(disk, mb->Index, 0, 0, fsname, 0);

    if (!mb->DriveLVM || mb->DriveLVM=='-') letter[0] = 0; else
       { letter[0] = mb->DriveLVM; letter[1] = ':'; letter[2] = 0; }

    if (!qstab) qsletter[0] = 0; else
       if (mb->Flags&DMAP_MOUNTED) {
          char ltr = mb->DriveQS;
          // fix 0:-9: to A:-J:
          if (ltr>='0' && ltr<='9') ltr=ltr-'0'+'A';
          sprintf(qsletter, "%c:|", ltr);
       } else
          strcpy(qsletter,"|");

    mref->id[*rcnt] = mb->Index+1<<16|disk+1;
    *rcnt += 1;
    mref->text->item[*rcnt] = sprintf_dyn("%s%s.%u|%s|%s|%s|%s", qsletter,
       dsk_disktostr(disk, 0), mb->Index, dsk_formatsize(sector, mb->Length,
          0, 0), fsname, letter, pd.VolName[0]?pd.VolName:"-");
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
                                   u32t type, u32t letters, u32t *disk,
                                   long *index)
{
   fpdsk_getmap   fp_getmap = 0;
   fplvm_partinfo fp_lpinfo = 0;
   fpdsk_ptquery64   fp_ptq = 0;
   u32t             partmgr;
   qserr                res;
   u32t                hdds = hlp_diskcount(0),
                      fbits = type&BTDT_FILTER;
   int                noemu = type&BTDT_NOEMU?1:0,
                       finv = type&BTDT_INVERT?1:0,
                      qstab = type&BTDT_QSDRIVE?1:0,
                     qsfilt = type&(BTDT_QSM|BTDT_NOQSM),
                    ltrfilt = type&(BTDT_FLTLVM|BTDT_FLTQS);
   const char   *ptlist_hdr = qstab ? FMT_STR_PTLIST2 : FMT_STR_PTLIST1;
   // drop flags
   type &= ~(BTDT_NOEMU|BTDT_FILTER|BTDT_QSM|BTDT_NOQSM|BTDT_QSDRIVE|
             BTDT_FLTLVM|BTDT_FLTQS);
   // adjust "default" position
   if ((flags&MSG_POSMASK)==0) flags|=MSG_POSX(-5)|MSG_POSY(-5);
   // and check args
   if (!disk || !index && type!=BTDT_HDD) return E_SYS_ZEROPTR;
   if (fbits==BTDT_INVERT || type>BTDT_DISKTREE) return E_SYS_INVPARM;
   if (qsfilt==(BTDT_QSM|BTDT_NOQSM)) return E_SYS_INVPARM;
   if (ltrfilt==(BTDT_FLTLVM|BTDT_FLTQS)) return E_SYS_INVPARM;

   if (!hdds) return E_SYS_NOTFOUND;
   /* query PARTMGR every time, because we mark this module as a system,
      so leaving PARTMGR loaded forever is not a good idea. It looks better
      as optional and unloadable */
   partmgr = mod_query(MODNAME_DMGR, 0);
   if (!partmgr) partmgr = mod_searchload(MODNAME_DMGR, 0, 0);
   if (partmgr) {
      fp_getmap = (fpdsk_getmap)mod_getfuncptr(partmgr, ORD_PARTMGR_dsk_getmap, 0);
      fp_lpinfo = (fplvm_partinfo)mod_getfuncptr(partmgr, ORD_PARTMGR_lvm_partinfo, 0);
      fp_ptq    = (fpdsk_ptquery64)mod_getfuncptr(partmgr, ORD_PARTMGR_dsk_ptquery64, 0);
   }
   // no partmgr?
   if (!fp_getmap) res = E_SYS_UNSUPPORTED; else 
   // no ptquery64?
   if (!fp_ptq && fbits) res = E_SYS_UNSUPPORTED; else {
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
                  dsk_mapblock *mb = map[ii]+idx;

                  if (mb->Type) {
                     u8t inc = 1;

                     if (fbits) {
                        static const char *fsname[] = { "FAT", "FAT12", "FAT16",
                           "FAT32", "exFAT", "HPFS", "JFS", "NTFS", 0 };
                        static const u32t fsv[] = { BTDT_FFAT, BTDT_FFAT,
                           BTDT_FFAT, BTDT_FFAT32, BTDT_FexFAT, BTDT_FHPFS,
                           BTDT_FJFS, BTDT_FNTFS };
                        char fs[17];
                        u32t fi, mask;
                        fs[0] = 0;
                        fp_ptq(ii, mb->Index, 0, 0, fs, 0);

                        for (fi=0; fsname[fi]; fi++)
                           if (strcmp(fsname[fi],fs)==0) {
                              mask = fsv[fi];
                              break;
                           }
                        inc = fbits&mask?1:0;
                        if (fbits&BTDT_INVERT) inc = !inc;
                     }
                     // filter by QS mounting
                     if (inc && (qsfilt&BTDT_QSM))
                        if ((mb->Flags&DMAP_MOUNTED)==0) inc = 0;
                     if (inc && (qsfilt&BTDT_NOQSM))
                        if (mb->Flags&DMAP_MOUNTED) inc = 0;
                     // filter by drive letters
                     if (inc && (ltrfilt&BTDT_FLTQS))
                        if (mb->Flags&DMAP_MOUNTED)
                           if ((letters&1<<mb->DriveQS-'0')==0) inc = 0;
                     if (inc && (ltrfilt&BTDT_FLTLVM))
                        if (mb->Flags&DMAP_LVMDRIVE)
                           if ((letters&1<<mb->DriveLVM-'A')==0) inc = 0;
                     // use high bit as a flag storage
                     mb->Flags = mb->Flags&~0x80 | (inc?0x80:0);
                     
                     if (inc) {
                        all++; pcnt[ii]++;
                        if (mb->Flags&DMAP_BMBOOT) inbm++;
                     }
                  }
               } while ((map[ii][idx++].Flags&DMAP_LEND)==0);
            }
      }
      if (!rdcnt || type==BTDT_LVM && !inbm || type==BTDT_ALL && !all) {
         vio_msgbox(header, type==BTDT_HDD?"No disks to show":"No partitions to show",
            MSG_OK|MSG_RED, 0);
      } else {
         vio_listref *mref = alloc_menu_ref(type==BTDT_ALL?all:(type==BTDT_LVM?inbm:rdcnt),
                                            type!=BTDT_HDD, 1, 0, 0);
         mref->text->item[0] = sprintf_dyn(hddmenu?FMT_STR_HDDLIST:ptlist_hdr);

         for (ii=0, rdcnt=0; ii<hdds; ii++)
            if (map[ii]) {
               u32t idx = 0, rpcnt = 0;
               if (type==BTDT_HDD || type==BTDT_DISKTREE) {
                  mref->id[rdcnt++]       = ii+1;
                  mref->text->item[rdcnt] = sprintf_dyn("Disk %2u|%s", ii,
                     dsk_formatsize(sectorsz[ii], hlp_disksize64(ii,0), 0, 0));

                  if (type==BTDT_DISKTREE) {
                     vio_listref *sm = alloc_menu_ref(pcnt[ii], 0, 1, VLSF_RIGHT, 0);
                     mref->subm[rdcnt-1] = sm;
                     sm->text->item[0]   = sprintf_dyn(ptlist_hdr);
                  }
               }
               do {
                  if (map[ii][idx].Type && (map[ii][idx].Flags&0x80)) {
                     if (type==BTDT_DISKTREE)
                        att_pt(fp_lpinfo, fp_ptq, ii, sectorsz[ii], map[ii]+idx,
                           mref->subm[rdcnt-1], &rpcnt, qstab); else
                     if (type==BTDT_ALL || type==BTDT_LVM && (map[ii][idx].Flags&DMAP_BMBOOT))
                        att_pt(fp_lpinfo, fp_ptq, ii, sectorsz[ii], map[ii]+idx,
                           mref, &rdcnt, qstab);
                  }
               } while ((map[ii][idx++].Flags&DMAP_LEND)==0);
            }
         key_clear();
         id = vio_showlist(header, mref, flags, 0);
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

/* ---------------------------------------------------------------------- */

static qserr new_shell(void) {
   qserr  res = 0;
   char *cmdp = env_getvar("COMSPEC", 0);
   u32t   cmd = mod_searchload(cmdp?cmdp:"cmd.exe", 0, &res);
   /// try it on incorrect comspec
   if (!cmd && cmdp) cmd = mod_searchload("cmd.exe", 0, &res);
   if (cmdp) free(cmdp);
   // we have a module - launch it
   if (cmd) {
      qs_mtlib mt = get_mtlib();
      res = mt ? mt->execse(cmd,0,0,0,0,0,0) : E_MT_DISABLED;
      if (res) mod_free(cmd);
   }
   return res;
}

/// shows "session list".
static u32t _exicc ext_seslistdlg(EXI_DATA, const char *header, u32t flags,
                                   u32t opts, u16t updatekey, u32t startid)
{
   u32t    sesno = se_sesno(), devmask = FFFF, id = startid,
            self = opts&SLDO_NOSELF ? sesno : 0;
   qserr errcode = 0;
   // drop "current devices" flag in detached session
   if (sesno==SESN_DETACHED && (opts&SLDO_CDEV)) opts&=~SLDO_CDEV;
   // adjust "default" position
   if ((flags&MSG_POSMASK)==0) flags|=MSG_POSX(-5)|MSG_POSY(-5);

   if (opts&SLDO_CDEV) {
      se_stats *se = se_stat(sesno);
      if (se) {
         devmask = se->devmask;
         free(se);
      }
   }
   while (1) {
      typedef struct _sp_list { 
         process_information *self;
         struct _sp_list     *next; 
      } sp_list;

      process_information      *pd;
      sp_list               **pses = 0;
      u32t        ii, nses, max_sn = 0, idx, nproc, *nsproc = 0, *nth = 0, n_oth;
      se_stats          **sl, **sd = 0;
      vio_listref              *mm;
      u32t             showall_cmd,
                           no_pkey = opts&SLDO_PLIST ? 0 : 1,
                           new_ses = opts&SLDO_NEWSES ? 3 : 0;
      int                  listpos = -1;
      u32t               hint_opts = VLSF_RIGHT|VLSF_HINT, himask = 0;
      // custom hint color
      if (opts&SLDO_HINTCOL) hint_opts |= VLSF_COLOR|(opts&SLDO_HCOLMASK)<<12;
      // guarantee thread<->session id match 
      mt_swlock();
      sl    = se_enum(0);
      nproc = mod_processenum(&pd);
      mt_swunlock();
      // should never occur
      if (!sl || !pd) {
         if (sl) free(sl);
         if (pd) free(pd);
         errcode = E_SYS_SOFTFAULT;
         break;
      }
      // calc # of sessions on selected devices (FFFF mean session without device too)
      for (ii=0, nses=0, n_oth=0; sl[ii]; ii++)
         if ((sl[ii]->devmask & devmask) || devmask==FFFF) nses++; else n_oth++;
      showall_cmd = n_oth && (opts&SLDO_NOSHOWALL)==0 ? 1 : 0;
      if (nses) {
         sd = (se_stats**)malloc_th(nses*sizeof(void*));
         for (ii=0, nses=0; sl[ii]; ii++)
            if ((sl[ii]->devmask & devmask) || devmask==FFFF) {
               sd[nses++] = sl[ii];
               if (sl[ii]->sess_no>max_sn) max_sn = sl[ii]->sess_no;
            }
         pses   = (sp_list**)calloc_th(max_sn+1, sizeof(void*));
         nsproc = (u32t*)calloc_th(max_sn+1, sizeof(u32t));
         nth    = (u32t*)calloc_th(max_sn+1, sizeof(u32t));
         /* create process list for every existing session.
            Start from the last to the first to fill it in ascending order */
         for (ii=nproc; ii; ii--) {
            process_information *pi = pd+(ii-1);
            for (idx=0; idx<pi->threads; idx++) {
               thread_information *ti = pi->ti + idx;
               if (ti->sid!=SESN_DETACHED) {
                  // use 64 bit time field as a list storage
                  sp_list *link = (sp_list*)&ti->time;
                  nth[ti->sid]++;
                  if (!pses[ti->sid]) {
                     pses[ti->sid] = link;
                     link->next    = 0;
                  } else
                  if (pses[ti->sid]->self==pi) continue; else {
                     link->next    = pses[ti->sid];
                     pses[ti->sid] = link;
                  }
                  link->self = pi;
                  nsproc[ti->sid]++;
               }
            }
         }
      }
      // +1 for header, -1 for self, +1 for "new session"
      mm = alloc_menu_ref((nses?nses-1:1)+(self?0:1)+showall_cmd, 1, 1, 0, 
                          2+(updatekey?1:0)-no_pkey+new_ses);
      mm->text->item[0] = strdup("<");
      ii = 0;
      if (!no_pkey) {
         mm->akey[ii].key     = 0x0F09;  // Tab
         mm->akey[ii].mask    = 0xFFFF;
         mm->akey[ii++].id_or = TLDR_TAB;
         himask |= TLDR_TAB;
      }
      // override Enter to return id (else it will switch hint on/off)
      mm->akey[ii].key        = 0x000D;
      mm->akey[ii].mask       = 0x00FF;
      mm->akey[ii++].id_or    = 0x40000000;
      himask |= 0x40000000;
      if (updatekey) {
         mm->akey[ii].key     = updatekey;
         mm->akey[ii].mask    = 0xFFFF;
         mm->akey[ii++].id_or = 0x20000000;
         himask |= 0x20000000;
      }
      // calc lowest used bit - 1 for service id values below
      himask = (1 << bsf32(himask)) - 1;
      // F3|n keys for new session
      if (new_ses) {
         mm->akey[ii].key     = 0x3D00;  // F3
         mm->akey[ii].mask    = 0xFF00;
         mm->akey[ii++].id_or = himask;
         mm->akey[ii].key     = 0x316E;  // n
         mm->akey[ii].mask    = 0xFFFF;
         mm->akey[ii++].id_or = himask;
         mm->akey[ii].key     = 0x314E;  // N
         mm->akey[ii].mask    = 0xFFFF;
         mm->akey[ii].id_or   = himask;
      }

      for (ii=0, idx=0; ii<nses; ii++)
         if (sd[ii]->sess_no!=self) {
            sp_list     *lp;
            char     *title = sd[ii]->title;
            u32t        sid = sd[ii]->sess_no, ll;
            vio_listref *si = alloc_menu_ref(3+nsproc[sid], 0, 1, hint_opts, 0);
            mm->subm[idx]   = si;
            mm->id[idx++]   = sid;
            mm->text->item[idx] = sprintf_dyn("%s", title?title:"no title");
            si->text->item[0]   = strdup("<");
            si->text->item[1]   = sprintf_dyn("Session %u", sid);
            si->text->item[2]   = sprintf_dyn("%u thread%s in %u process%s",
               nth[sid], nth[sid]==1?"":"s", nsproc[sid], nsproc[sid]==1?"":"es");
            si->text->item[3]   = strdup("-");
            // try to preserve list position
            if (sid==id || listpos<0 && id && sid>id) listpos = idx-1;

            lp = pses[sid];
            for (ll=0; ll<nsproc[sid] && lp; ll++) {
               process_information *pi = lp->self;
               char *line = sprintf_dyn("%3u %s", pi->pid, pi->mi->mod_path);
               // too noisy
               // if (sd[ii]->focus==pi->pid) line = strcat_dyn(line, " (Ctrl-C focus)");
               si->text->item[4+ll] = line;
               lp = lp->next;
            }
         }

      ii = mm->text->count - (nses?1:2);
      if (showall_cmd) {
         mm->id[ii-1] = himask-1;
         mm->text->item[ii] = strdup("All devices");
      }
      // still insert this string to populate empty list on secondary device
      if (!nses) {
         ii++;
         mm->id[ii-1]  = himask;
         mm->text->item[ii] = strdup("New session");
      }

      id = vio_showlist(header, mm, flags, listpos<0?0:listpos);
      free_menu_ref(mm, 1);
      if (nsproc) free(nsproc);
      if (pses) free(pses);
      if (nth) free(nth);
      if (sd) free(sd);
      free(pd);
      free(sl);
      /* drop "session fore" flag else any session start will switch into the
         existing task list instead, just after "update" key processing */
      flags&=~MSG_SESFORE;
      // convert Enter press to common id
      if (id&0x40000000) id&=~0x40000000;

      if (id==himask-1) {
         devmask = FFFF;
         id = 0;
         continue;
      } else
      if (id==himask) {
         errcode = new_shell();
         id = 0;
      } else
      if (id&0x20000000) { // "update list" pseudo-key pressed
         id&=~0x20000000;
         continue;
      } else
      // Tab pressed on "New session" && "All devices" - return neutral ses 1
      if ((id&TLDR_TAB) && (id&himask)>=himask-1) id = TLDR_TAB|1;

      break;
   }
   if (errcode) {
      char *msg = cmd_shellerrmsg(EMSG_QS, errcode);
      if (!msg) msg = sprintf_dyn("Internal error %X", errcode);
      vio_msgbox("Session list", msg, MSG_LIGHTRED|MSG_OK, 0);
      free(msg);
      id = 0;
   }
   return id;
}

static u32t tree_walk(process_information *pd, process_information **pos2pi,
                      ptr_list str, u32t pos, u32t level, int last)
{
   static const char *box = "ÀÃÄ³Â";

   char *line = 0;
   u32t    ll;

   if (level) {
      char hdr[4];
      // copy deep parent state from the previous line
      if (level>1) {
         char *pl = (char*)str->value(str->max());
         line = (char*)malloc_th(level);
         for (ll=0; ll<level-1; ll++)
            line[ll] = pl[ll]==box[1]||pl[ll]==box[3] ? box[3] : ' ';
         line[level-1] = 0;
      }
      hdr[0] = box[last?0:1];
      hdr[1] = box[pd->ncld?4:2];
      hdr[2] = ' ';
      hdr[3] = 0;
      line   = strcat_dyn(line, hdr);
   }
   line = strcat_dyn(line, pd->mi?pd->mi->name:"?");
   str->add(line);
   pos2pi[pos++] = pd;

   for (ll=0; ll<pd->ncld; ll++)
      pos = tree_walk(pd->cll[ll], pos2pi, str, pos, level+1, ll==pd->ncld-1);
   return pos;
}

static int _std pkill_cb(struct _vio_list *ref, u16t key, u32t id) {
    char *msg = sprintf_dyn("Kill process id %u tree?", id);
    u32t mres = vio_msgbox("Process list", msg, MSG_YESNO|MSG_RED, 0);
    free(msg);

    if (mres==MRES_YES) {
       qs_mtlib mt = get_mtlib();
       qserr   err = mt ? mt->stop(id, 1, EXIT_FAILURE) : E_MT_DISABLED;
       if (err) {
          msg = cmd_shellerrmsg(EMSG_QS, err);
          if (!msg) msg = sprintf_dyn("Internal error %X", err);
          vio_msgbox("Task list", msg, MSG_LIGHTRED|MSG_OK, 0);
          free(msg);
       } else
          return INT_MAX;
    }
    return 0;
}

static u32t _exicc ext_tasklistdlg(EXI_DATA, const char *header, u32t flags,
                                   u32t opts, u16t updatekey, u32t startpid)
{
   qserr errcode = 0;
   mt_pid     id = startpid;
   // adjust "default" position
   if ((flags&MSG_POSMASK)==0) flags|=MSG_POSX(-5)|MSG_POSY(-5);

   while (1) {
      process_information      *pd, **pos2pi;
      ptr_list                 str = 0;
      vio_listref              *mm;
      int                  listpos = -1, loop = 0;
      u32t             ii, key_tab = opts&TLDO_PLIST ? 1 : 0,
                           key_del = opts&TLDO_PDEL ? 1 : 0, upd_or = 0,
                           new_ses = opts&TLDO_NEWSES ? 3 : 0, nses_id,
                             e_del = opts&TLDO_EDEL ? 1 : 0,
                             nproc = mod_processenum(&pd);
      if (e_del && key_del) key_del = 0;
      // should never occur
      if (!pd) {
         errcode = E_SYS_SOFTFAULT;
         break;
      }
      pos2pi = (process_information**)malloc_th(sizeof(void*)*nproc);
      str    = NEW(ptr_list);
      // pid 1 in 1st pos assumed here
      tree_walk(pd, pos2pi, str, 0, 0, 0);

      mm = alloc_menu_ref(nproc, 1, 1, 0, (updatekey?1:0)+key_tab+key_del+e_del+new_ses);
      mm->text->item[0] = strdup("<|>|<");
      ii = 0;

      if (key_tab) {
         mm->akey[ii].key     = 0x0F09;  // Tab
         mm->akey[ii].mask    = 0xFFFF;
         mm->akey[ii++].id_or = TLDR_TAB;
         upd_or |= TLDR_TAB;
      }
      if (key_del || e_del) {
         mm->akey[ii].key     = 0x5300;  // Del
         mm->akey[ii].mask    = 0xFF00;
         mm->akey[ii].id_or   = TLDR_DEL;
         upd_or |= TLDR_DEL;
         if (e_del) mm->akey[ii].cb = pkill_cb;
         ii++;
      }
      if (updatekey) {
         mm->akey[ii].key     = updatekey;
         mm->akey[ii].mask    = 0xFFFF;
         // next free bit below used Tab/Del
         upd_or = 1 << (upd_or ? bsf32(upd_or) - 1 : 31);
         mm->akey[ii++].id_or = upd_or;
      }
      // lowest used bit - 1
      nses_id = (1 << bsf32(upd_or)) - 1;
      // F3|n keys for new session
      if (new_ses) {
         mm->akey[ii].key     = 0x3D00;  // F3
         mm->akey[ii].mask    = 0xFF00;
         mm->akey[ii++].id_or = nses_id;
         mm->akey[ii].key     = 0x316E;  // n
         mm->akey[ii].mask    = 0xFFFF;
         mm->akey[ii++].id_or = nses_id;
         mm->akey[ii].key     = 0x314E;  // N
         mm->akey[ii].mask    = 0xFFFF;
         mm->akey[ii].id_or   = nses_id;
      }

      for (ii=0; ii<nproc; ii++) {
          process_information *pi = pos2pi[ii];
          mm->id[ii] = pi->pid;
          mm->text->item[ii+1] = sprintf_dyn("%s|%u|%s", str->value(ii), pi->pid,
             pi->flags&PFLM_SYSTEM?"sys":(pi->ti[0].state==THRD_FINISHED?"exit":
                (pi->ti[0].sid==0?"det":"")));
         // try to preserve list position
         if (pi->pid==id) listpos = ii;
      }
      str->freeitems(0, str->max());
      DELETE(str);

      id = vio_showlist(header, mm, flags, listpos<0?0:listpos);
      free_menu_ref(mm, 1);
      /* drop "session fore" flag else any session start will switch into the
         existing task list instead, just after "update" key processing */
      flags&=~MSG_SESFORE;

      if (id==nses_id) {
         errcode = new_shell();
         id = 0;
      } else
      if (id&upd_or) { // "update list" pseudo-key pressed
         id  &=~upd_or;
         loop = 1;
      } else
      if (e_del && (id&TLDR_DEL)) { // process "del" internally
         id  &=~TLDR_DEL;
         loop = 1;
         // search for parent in the process data list
         for (ii=0; ii<nproc; ii++) {
            process_information *pi = pos2pi[ii];
            if (pi->pid==id) {
               id = pi->parent ? pi->parent->pid : 0;
               break;
            }
         }
      }
      free(pos2pi);
      free(pd);
      if (loop) continue; else break;
   }
   if (errcode) {
      char *msg = cmd_shellerrmsg(EMSG_QS, errcode);
      if (!msg) msg = sprintf_dyn("Internal error %X", errcode);
      vio_msgbox("Task list", msg, MSG_LIGHTRED|MSG_OK, 0);
      free(msg);
      id = 0;
   }

   return id;
}

static void *ext_methods_list[] = { ext_selectmodedlg, ext_bootmgrdlg,
   ext_seslistdlg, ext_tasklistdlg };

static void _std ext_initialize(void *instance, void *data) {}
static void _std ext_finalize(void *instance, void *data) {}

void exi_register_extcmd(void) {
   if (sizeof(ext_methods_list)!=sizeof(_qs_extcmd))
      vio_msgbox("SYSTEM WARNING!", "qs_extcmd: Fix me!", MSG_LIGHTRED|MSG_OK, 0);
   exi_register("qs_extcmd", ext_methods_list, sizeof(ext_methods_list)/sizeof(void*),
      0, 0, 0, 0, 0);
}
