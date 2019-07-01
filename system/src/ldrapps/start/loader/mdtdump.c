//
// QSINIT "start" module
// dump module table
//
#include "qsutil.h"
#include "qsmodint.h"
#include "qsint.h"
#include "qsinit.h"
#include "qslog.h"
#include "qslxfmt.h"
#include "qsxcpt.h"
#include "syslocal.h"
#include "qspdata.h"

extern module * _std mod_list, * _std mod_ilist; // loaded and loading module lists
extern mt_proc_cb   _std           mt_exechooks;

void _std log_mdtdump(printf_function pfn) {
   int ii=2, obj;
   if (pfn==0) pfn = log_printf;
   pfn("== MDT contents ==\n");
   pfn("* Loaded list:\n");
   // use QSINIT binary internal mutex to serialize access to the module lists
   if (in_mtmode) mt_muxcapture(mod_secondary->ldr_mutex);

   while (ii--) {
      module *md=ii?mod_list:mod_ilist, *impm;
      if (!md) pfn("   empty!\n");

      while (md) {
         char      st[256];
         u16t  flatcs = get_flatcs();
         int      len;
         pfn("\n");
         pfn("%s, %d objects, base addr: 0x%08X, usage %d\n", md->name,
            md->objects, md->baseaddr, md->usage);
         pfn("  module path: %s\n", md->mod_path);
         pfn("  objects:\n");
         for (obj=0;obj<md->objects;obj++) {
            u16t objsel = md->obj[obj].sel;
            u32t     fl = md->obj[obj].flags, jj;
            static u32t objflags[]={OBJREAD, OBJWRITE, OBJEXEC, OBJSHARED, OBJPRELOAD, OBJALIAS16,
                                    OBJBIGDEF, OBJCONFORM, OBJIOPL, 0};
            static char  *objstr[]={"READ", "WRITE", "EXEC", "SHARED", "PRELOAD", "ALIAS",
                                    "BIG", "CONFORM", "IOPL", 0};

            len=sprintf(st,"  %2d. 0x%08x, size %6d",obj+1,md->obj[obj].address,md->obj[obj].size);
            // do not print FLAT CS/DS selectors, only custom (16-bit) ones
            if (objsel && (objsel<flatcs || objsel>flatcs+8))
               len+=sprintf(st+len,", sel %04x", objsel);
            len+=sprintf(st+len,", flags %08x (",fl);

            for (jj=0;objflags[jj];jj++)
               if (fl&objflags[jj]) len+=sprintf(st+len,"%s ",objstr[jj]);
            if (st[len-1]==' ') len--;

            strcpy(st+len,")\n");
            pfn(st);
         }
         obj=0; len=0;
         while ((impm=md->impmod[obj++])!=0) {
            if (obj==1) len=sprintf(st,"  uses modules: ");
            len+=sprintf(st+len,"%s ",impm->name);
         }
         if (len) { pfn("\n"); pfn("%s\n",st); }
         if (md->exports) {
            pfn("\n");
            pfn("  exports:\n");
            for (obj=0; obj<md->exports; obj++) {
               mod_export *ee=md->exps+obj;
               if ((obj&3)==0) len=sprintf(st,"  ");
               if (ee->forward) len+=sprintf(st+len,"%4d.<FORWARD> ",ee->ordinal); else
               if (ee->is16) len+=sprintf(st+len,"%4d.%04X:%04X ",ee->ordinal,ee->sel,
                  ee->address);
               else len+=sprintf(st+len,"%4d.%08X  ",ee->ordinal,ee->address);

               if ((obj&3)==3||obj+1==md->exports) pfn("%s\n",st);
            }
         }

         pfn("\n");
         md = md->next;
      }

      if (ii) pfn("* Loading list:\n");
   }
   if (in_mtmode) mt_muxrelease(mod_secondary->ldr_mutex);

   pfn("==================\n");
}

static u32t* sv_mt = 0;

// print memory control table contents (debug)
void _std log_memtable(printf_function pfn, void* memtable, void* memftable,
                       u8t *memheapres, u32t *memsignlst, u32t memblocks,
                       process_context **memowner)
{
   auto u32t*         mt = sv_mt = memtable;
   auto free_block **mft = memftable;
   auto u8t         *mhr = memheapres;
   u32t          ii, cnt;
   char        sigstr[8];
   sigstr[4] = 0;

   if (pfn==0) pfn = log_printf;

   mt_swlock();
   pfn("<=====QS memory dump=====>\n");
   pfn("Total : %d kb\n", memblocks<<6);
   pfn("        addr        sign   pid         size\n");
   for (ii=0,cnt=0; ii<memblocks; ii++,cnt++) {
      free_block *fb=(free_block*)((u32t)mt+(ii<<16));
      if (mt[ii]&&mt[ii]<FFFF) {
         char     buffer[80], pid[8];
         module      *mi = mod_by_eip((u32t)fb, 0, 0, 0);
         int         len;
         *(u32t*)&sigstr = memsignlst[ii];

         if (memowner[ii]) snprintf(pid, 8, " %5u", memowner[ii]->pid);
            else strcpy(pid, "      ");

         len = snprintf(buffer, 64, "%5d. %08X - %c %-4s %s  %7d kb (%d)", cnt, fb,
            mhr[ii>>3]&1<<(ii&7) ? 'R' : ' ', sigstr, pid, Round64k(mt[ii])>>10,
               mt[ii]);
         while (len<58) len = strlen(strcat(buffer, " "));
         if (mi) strcat(buffer, "// ");
         pfn("%s%s\n", buffer, mi?mi->name:"");
      } else
      if (!mt[ii]&&ii&&mt[ii-1]) {
         if (fb->sign!=FREE_SIGN) pfn("free header destroyd!!!\n");
            else pfn("%5d. %08X - <free>         %7d kb\n",cnt,fb,fb->size<<6);
      }
   }
   pfn("%5d. %08X - end\n",cnt,(u32t)mt+(memblocks<<16));

   pfn("Free table:\n");
   for (ii=0; ii<memblocks; ii++)
      if (mft[ii]) {
         free_block *fb=mft[ii];
         pfn("%d kb (%d):",ii<<6,ii);
         while (fb) {
            pfn(" %08X",fb);
            if (fb->size!=ii) pfn("(%d!)",fb->size);
            if (fb->sign!=FREE_SIGN) pfn("(S:%08X!)",fb->sign);
            fb=fb->next;
         }
         pfn("\n");
      }
   mt_swunlock();
}

void _std mempanic(u32t type, void *addr, u32t info, char here, u32t caller) {
   u32t   object, offset;
   u32t   px = 3, py = 3, hdt = 2;
   module            *mi;
   free_block        *fb = (free_block*)addr;  // just alias
   static char       msg[77];
   static const char *em[] = { "Memory table data damaged",
      "Free block size error", "Trying realloc read-only block",
      "Wrong address in call", "Free block signature damaged",
      "Broken free block header", "Unknown error type" };
   if (type>MEMHLT_FBHEADER+1) type = MEMHLT_FBHEADER+1;

   switch (type) {
      case MEMHLT_MEMTABLE : hdt+=1; break;
      case MEMHLT_FBSIZEERR: hdt+=2; break;
      case MEMHLT_FBSIGN   : hdt+=2; break;
      case MEMHLT_FBHEADER : hdt+=2; break;
   }
   mt_swlock();
   trap_screen_prepare();
   draw_border(1, 2, 78, hdt+5, 0x4F);
   vio_setpos(py, px); vio_strout(" \xFE SYSTEM HEAP FATAL ERROR \xFE");
   py+=2;
   vio_setpos(py++, px); vio_strout(em[type]);
   vio_strout(". Occurs in ");
   switch (here) {
      case 'a': vio_strout("alloc_worker");   break;
      case 'r': vio_strout("hlp_memrealloc"); break;
      case 'f': vio_strout("hlp_memfree");    break;
      case 'i': vio_strout("hlp_memreserve"); break;
      case 'm': vio_strout("hlp_memavail");   break;
      case 'b': vio_strout("hlp_memgetsize"); break;
      default:
         snprintf(msg, 77, "strange location (%c)", here);
         vio_strout(msg);
   }
   vio_setpos(py++, px);
   snprintf(msg, 77, "Error address - %08X", addr);
   vio_strout(msg);

   vio_setpos(py++, px);
   mi = mod_by_eip(caller, &object, &offset, 0);
   if (mi) snprintf(msg, 77, "Caller - %08X: \"%s\" %u:%08X", caller, mi->name,
      object+1, offset); else
         snprintf(msg, 77, "Caller unknown - %08X", caller);
   vio_strout(msg);

   vio_setpos(py++, px);

   switch (type) {
      case MEMHLT_MEMTABLE :
         if (sv_mt) snprintf(msg, 77, "Table mismatch with expected value. Found %08X at pos %u",
            sv_mt[info], info);
         else
            snprintf(msg, 77, "Table mismatch with expected value at pos %u", info);
         break;
      case MEMHLT_FBSIZEERR:
         snprintf(msg, 77, "Size is %ukb instead of %ukb", fb->size<<6, info<<6);
         break;
      case MEMHLT_FBSIGN   :
         snprintf(msg, 77, "Value is %08X instead of %08X", fb->sign, FREE_SIGN);
         break;
      case MEMHLT_FBHEADER :
         snprintf(msg, 77, "%08X<-%08X->%08X", fb->prev, fb, fb->next);
         break;
      default:
         msg[0] = 0;
   }
   if (msg[0]) vio_strout(msg);

   if (type==MEMHLT_FBSIZEERR || type==MEMHLT_FBSIGN || type==MEMHLT_FBHEADER) {
      vio_setpos(py++, px);
      snprintf(msg, 77, "%08X: %16b", addr, addr);
      vio_strout(msg);
   }
   reipl(QERR_MCBERROR);
}

void _std log_dumppctx(process_context* pq, printf_function pfn) {
   char     fmtstr[32];
   u32t         ii;
   mt_prcdata  *pd = (mt_prcdata*)pq->rtbuf[RTBUF_PROCDAT];
   if (pfn==0) pfn = log_printf;

   pfn(" PID %4d : %s\n", pq->pid, pq->self->name);
   if (pq->self)
      pfn("   module : %08X, path : %s\n", pq->self, pq->self->mod_path);
   if (pq->parent)
      pfn("   parent : %08X, path : %s\n", pq->parent, pq->parent->mod_path);
   pfn("    flags : %08X\n", pq->flags);
   pfn("  env.ptr : %08X\n", pq->envptr);

   // buffers can grow in next revisions - so print it in this way
   ii = sizeof(pq->rtbuf)/4;
   snprintf(fmtstr, 24, "    rtbuf : %%%dlb\n", ii/2);
   pfn(fmtstr, pq->rtbuf);
   memcpy(fmtstr, "          ", 10);
   pfn(fmtstr, pq->rtbuf+ii/2);

   ii = sizeof(pq->userbuf)/4;
   snprintf(fmtstr, 24, "  userbuf : %%%dlb\n", ii/2);
   pfn(fmtstr, pq->userbuf);
   memcpy(fmtstr, "          ", 10);
   pfn(fmtstr, pq->userbuf+ii/2);

   if (!pd) pfn("  procinfo ptr is NULL!\n"); else {
      char *cdv = pd->piCurDir[pd->piCurDrive];
      pfn("  cur.dir : %c:\\%s\n", pd->piCurDrive+'A', cdv?cdv+3:"");
   }
}

/** dump process tree to log.
    This dump replaced by MTLIB when it active */
void _std START_EXPORT(mod_dumptree)(printf_function pfn) {
   process_context* pq = mod_context();
   if (pfn==0) pfn = log_printf;
   pfn("== Process Tree ==\n");
   while (pq) {
      mt_prcdata *pd = (mt_prcdata*)pq->rtbuf[RTBUF_PROCDAT];
      log_dumppctx(pq,pfn);
      if (pd) pfn("  procinfo: %08X, tid 1: %08X\n", pd, pd->piList[0]);
      pq = pq->pctx;
      pfn("------------------\n");
   }
   if (mt_exechooks.mtcb_cfpt)
      pfn("FPU owner pid %u\n", mt_exechooks.mtcb_cfpt->tiPID);
   else
      pfn("FPU is not used\n");
   pfn("==================\n");
}

static u32t mod_enum_int(module_information **pmodl, u32t moresize) {
   module_information *rc, *rp;
   u32t   mcnt = 0, ii,
          slen = 0;
   module  *md;
   char    *cb;

   if (in_mtmode) mt_muxcapture(mod_secondary->ldr_mutex);
   md = mod_list;
   while (md) {
      mcnt++;
      slen += strlen(md->mod_path)+1 + strlen(md->name)+1;
      md = md->next;
   }
   if (!pmodl) return mcnt;
   rc = (module_information*)((u8t*)malloc_local(sizeof(module_information)*mcnt +
                                                 slen + moresize) + moresize);
   md = mod_list;
   cb = (char*)(rc + mcnt);
   ii = 0;
   rp = rc;
   while (md && ii++<mcnt) {
      rp->handle    = (u32t)md;
      rp->flags     = md->flags;
      rp->start_ptr = md->start_ptr;
      rp->stack_ptr = md->stack_ptr;
      rp->usage     = md->usage;
      rp->objects   = md->objects;
      rp->baseaddr  = (u32t)md->baseaddr;
      rp->mod_path  = cb;
      strcpy(cb, md->mod_path); cb+=strlen(cb)+1;
      rp->name      = cb;
      strcpy(cb, md->name); cb+=strlen(cb)+1;
      memcpy(&rp->imports, &md->impmod, sizeof(void*)*(MAX_IMPMOD+1));

      md = md->next;
      rp++;
   }
   rp   = rc;
   // who knows :)
   mcnt = ii;
   // re-link imports
   for (ii=0; ii<mcnt; ii++) {
      u32t idx = 0, ll;
      while (rp->imports[idx] && idx<MAX_IMPMOD) {
         for (ll=0; ll<mcnt; ll++)
            if ((u32t)rp->imports[idx]==rc[ll].handle) {
               rp->imports[idx] = rc+ll;
               break;
            }
         idx++;
      }
      rp++;
   }
   if (in_mtmode) mt_muxrelease(mod_secondary->ldr_mutex);
   *pmodl = rc;
   return mcnt;
}

/** enum all modules in system.
    @param [out] pmodl   List of modules, must be releases via free().
    @return number of modules in returning list */
u32t _std mod_enum(module_information **pmodl) {
   return mod_enum_int(pmodl,0);
}

qserr _std mod_objectinfo(u32t mod, u32t object, u32t *addr, u32t *size,
                          u32t *flags, u16t *sel)
{
   qserr  res = E_MOD_HANDLE;
   if (mod) {
      module *md = (module*)mod;
      if (in_mtmode) mt_muxcapture(mod_secondary->ldr_mutex);
      if (md->sign==MOD_SIGN)
         if (object>=md->objects) res = E_SYS_INVPARM; else {
            mod_object *mo = md->obj + object;
            if (addr ) *addr  = (u32t)mo->address;
            if (size ) *size  = mo->size;
            if (flags) *flags = mo->flags;
            if (sel  ) *sel   = mo->sel;
            res = 0;
         }
      if (in_mtmode) mt_muxrelease(mod_secondary->ldr_mutex);
   }
   return res;
}

#define pd2loc(x) \
   (*ppdl+((mt_prcdata**)memchrd((u32t*)pdl, (u32t)(x), pcnt) - pdl))

u32t _std mod_processenum(process_information **ppdl) {
   u32t  *pl, pcnt;
   mt_swlock();
   pl   = mod_pidlist();
   pcnt = memchrd(pl, 0, mem_blocksize(pl)>>2) - pl;
   if (ppdl) {
      module_information  *ml = 0;
      process_information *rp, **rpcs;
      u32t          ii, tilen = 0, cdlen = 0, cllen = 0, mcnt,
                         cpid = mod_getpid();
      char              *cdir;
      thread_information *ctp;
      mt_prcdata        **pdl = (mt_prcdata**)malloc_local(pcnt*sizeof(void*));

      /* we should never receive PFLM_CHAINEXIT flag here, because
         mod_pidctx() returns "visible" process and ignores exiting one
         during chaining */
      for (ii=0; ii<pcnt; ii++) {
         process_context *pq = mod_pidctx(pl[ii]);
         mt_prcdata      *pd = (mt_prcdata*)(mt_prcdata*)pq->rtbuf[RTBUF_PROCDAT],
                         *pc = pd->piFirstChild;
         char           *cdv = pd->piCurDir[pd->piCurDrive];
         u32t       childcnt = 0;
         // dec MT lock after mod_pidctx() because we locked over entire call
         mt_swunlock();
         while (pc) { childcnt++; pc = pc->piNext; }
         pdl[ii] = pd;
         cdlen  += (cdv?strlen(cdv):3)+1;
         tilen  += pd->piThreads * sizeof(thread_information);
         cllen  += (childcnt+1) * sizeof(void*);
         // replace pid with childcnt to use it below
         pl[ii]  = childcnt;
      }
      ii    = tilen+cdlen+cllen+sizeof(process_information)*pcnt;
      mcnt  = mod_enum_int(&ml, ii);
      // log_it(2,"mod_enum_int(%08X, %u) = %u done\n", &ml, ii, mcnt);
      // returning data
      rp    = (process_information*)((u8t*)ml - ii);
      ctp   = (thread_information*)((u8t*)rp + sizeof(process_information)*pcnt);
      rpcs  = (process_information**)((u8t*)ctp + tilen);
      cdir  = (char*)((u8t*)rpcs + cllen);
      *ppdl = rp;

      for (ii=0; ii<pcnt; ii++) {
         mt_prcdata *pd = pdl[ii],
                    *pc = pd->piFirstChild;
         char      *cdv = pd->piCurDir[pd->piCurDrive];
         u32t   pathlen = (cdv?strlen(cdv):3)+1, ti, tidx;

         rp->pid     = pd->piPID;
         rp->threads = pd->piThreads;
         rp->flags   = pd->piMiscFlags;
         rp->ti      = ctp;
         rp->ncld    = pl[ii];
         rp->cll     = rpcs;
         if (cdv) memcpy(cdir, cdv, pathlen); else {
            cdir[0] = pd->piCurDrive+'A';
            cdir[1] = ':';
            cdir[2] = '\\';
            cdir[3] = 0;
         }
         rp->curdir  = cdir;
         cdir += pathlen;

         ti = 0;
         while (pc) { 
            rp->cll[ti++] = pd2loc(pc);
            pc = pc->piNext;
         }
         rp->cll[ti++] = 0;
         rpcs += ti;

         if (pd->piParent) rp->parent = pd2loc(pd->piParent); else
            rp->parent = 0;

         // module information pointer
         rp->mi = 0;
         for (ti=0; ti<mcnt; ti++)
            if (ml[ti].handle==(u32t)pd->piModule) {
               rp->mi = ml + ti;
               break;
            }
         // collect threads
         for (ti=0; ti<pd->piListAlloc; ti++) {
            mt_thrdata *td = pd->piList[ti];
            if (td) {
               ctp->tid         = td->tiTID;
               ctp->fibers      = td->tiFibers;
               ctp->activefiber = td->tiFiberIndex;
               ctp->state       = td->tiState;
               ctp->sid         = td->tiSession;
               ctp->flags       = td->tiMiscFlags;
               ctp->time        = td->tiTime;
               // fix wrong state value in non-MT mode
               if (!in_mtmode && rp->pid!=cpid) ctp->state = THRD_WAITING;

               ctp++;
            }
         }
         rp++;
      }
      free(pdl);
      free(pl);
   }
   mt_swunlock();
   return pcnt;
}

