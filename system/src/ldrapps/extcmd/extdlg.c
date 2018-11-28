//
// QSINIT "extcmd" module
// Advanced system dialogs shared class
//
#include "qcl/qsextdlg.h"
#include "partmgr_ord.h"
#include "qcl/qslist.h"
#include "qcl/qsmt.h"
#include "vioext.h"
#include "malloc.h"
#include "stdlib.h"
#include "qslog.h"
#include "qsint.h"
#include "qscon.h"
#include "qsmodext.h"
#include "qsdm.h"
#include "vio.h"

#define FMT_STR_MODELIST   "9|>7|"
#define FMT_STR_HDDLIST    "9|>10"
#define FMT_STR_PTLIST     ">|>10|<9|4|<"

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
      // fix number of items
      rm->items = pos;
      rm->text->count = pos+1;

      resid = vio_showlist(header, rm, flags, 0);
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
   // adjust "default" position
   if ((flags&MSG_POSMASK)==0) flags|=MSG_POSX(-5)|MSG_POSY(-5);
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
                                            type!=BTDT_HDD, 1, 0, 0);
         mref->text->item[0] = sprintf_dyn(hddmenu?FMT_STR_HDDLIST:FMT_STR_PTLIST);

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
                     sm->text->item[0]   = sprintf_dyn(FMT_STR_PTLIST);
                  }
               }
               do {
                  if (map[ii][idx].Type) {
                     if (type==BTDT_DISKTREE)
                        att_pt(fp_lpinfo, fp_ptq, ii, sectorsz[ii], map[ii]+idx,
                           mref->subm[rdcnt-1], &rpcnt); else
                     if (type==BTDT_ALL || type==BTDT_LVM && (map[ii][idx].Flags&DMAP_BMBOOT))
                        att_pt(fp_lpinfo, fp_ptq, ii, sectorsz[ii], map[ii]+idx,
                           mref, &rdcnt);
                  }
               } while ((map[ii][idx++].Flags&DMAP_LEND)==0);
            }
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

/// shows "task list".
static u32t _exicc ext_tasklistdlg(EXI_DATA, const char *header, u32t flags,
                                   u32t opts, u16t updatekey)
{
   u32t    sesno = se_sesno(), devmask = FFFF, id = 0,
            self = opts&TLDT_NOSELF ? sesno : 0;
   qserr errcode = 0;
   // drop "current devices" flag in detached session
   if (sesno==SESN_DETACHED && (opts&TLDT_CDEV)) opts&=~TLDT_CDEV;
   // adjust "default" position
   if ((flags&MSG_POSMASK)==0) flags|=MSG_POSX(-5)|MSG_POSY(-5);

   if (opts&TLDT_CDEV) {
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
                         no_delkey = opts&TLDT_NODEL ? 1 : 0;
      int                  listpos = -1;
      u32t               hint_opts = VLSF_RIGHT|VLSF_HINT;
      // custom hint color
      if (opts&TLDT_HINTCOL) hint_opts |= VLSF_COLOR|(opts&TLDT_HCOLMASK)<<12;
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
      // calc # of sessions on selected devices (FFFF means session without device too)
      for (ii=0, nses=0, n_oth=0; sl[ii]; ii++)
         if ((sl[ii]->devmask & devmask) || devmask==FFFF) nses++; else n_oth++;
      showall_cmd = n_oth && (opts&TLDT_NOSHOWALL)==0 ? 1 : 0;
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
      mm = alloc_menu_ref((nses?nses:1)+(self?0:1)+showall_cmd, 1, 1, 0, 
                          2+(updatekey?1:0)-no_delkey);
      mm->text->item[0] = strdup("<");
      ii = 0;
      if (!no_delkey) {
         mm->akey[ii].key     = 0x5300;
         mm->akey[ii].mask    = 0xFF00;
         mm->akey[ii++].id_or = 0x80000000;
      }
      // override Enter to return id (else it will switch hint on/off)
      mm->akey[ii].key        = 0x000D;
      mm->akey[ii].mask       = 0x00FF;
      mm->akey[ii++].id_or    = 0x40000000;
      if (updatekey) {
         mm->akey[ii].key     = updatekey;
         mm->akey[ii].mask    = 0xFFFF;
         mm->akey[ii].id_or   = 0x20000000;
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

      ii = mm->text->count - 2;
      if (showall_cmd) {
         mm->id[ii-1] = FFFF-2;
         mm->text->item[ii] = strdup("All devices");
      }
      ii++;
      mm->id[ii-1]  = FFFF-1;
      mm->text->item[ii] = strdup("New session");

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

      if (id==FFFF-2) {
         devmask = FFFF;
         id = 0;
         continue;
      } else
      if (id==FFFF-1) {
         char *cmdp = env_getvar("COMSPEC");
         u32t   cmd = mod_searchload(cmdp?cmdp:"cmd.exe", 0, &errcode);
         /// try it on incorrect comspec
         if (!cmd && cmdp) cmd = mod_searchload("cmd.exe", 0, &errcode);
         if (cmdp) free(cmdp);
         // we have a module - launch it
         if (cmd) {
            qs_mtlib mt = NEW(qs_mtlib);
            errcode = mt ? mt->execse(cmd,0,0,0,0,0,0) : E_MT_DISABLED;
            if (errcode) mod_free(cmd);
         }
         id = 0;
      } else
      if (id&0x20000000) { // "update list" pseudo-key pressed
         id&=~0x20000000;
         continue;
      } else
      if (id&0x40000000) id&=~0x40000000;

      break;
   }
   if (errcode) {
      char *msg = cmd_shellerrmsg(EMSG_QS, errcode);
      if (!msg) msg = sprintf_dyn("Internal error %X", errcode);
      vio_msgbox("Task list", msg, MSG_LIGHTRED|MSG_OK, 0);
      free(msg);
   }
   return id;
}

static void *ext_methods_list[] = { ext_selectmodedlg, ext_bootmgrdlg, ext_tasklistdlg };

static void _std ext_initialize(void *instance, void *data) {}
static void _std ext_finalize(void *instance, void *data) {}

void exi_register_extcmd(void) {
   if (sizeof(ext_methods_list)!=sizeof(_qs_extcmd))
      vio_msgbox("SYSTEM WARNING!", "qs_extcmd: Fix me!", MSG_LIGHTRED|MSG_OK, 0);
   exi_register("qs_extcmd", ext_methods_list, sizeof(ext_methods_list)/sizeof(void*),
      0, 0, 0, 0, 0);
}
