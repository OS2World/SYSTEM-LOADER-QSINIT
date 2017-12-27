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
#include "syslocal.h"
#include "qspdata.h"

extern module * _std mod_list, * _std mod_ilist; // loaded and loading module lists
extern mt_proc_cb   _std           mt_exechooks;

void log_mdtdump_int(printf_function pfn) {
   int ii=2, obj;
   pfn("== MDT contents ==\n");
   pfn("* Loaded list:\n");
   // use QSINIT binary internal mutex to serialize access to module lists
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
            for (obj=0;obj<md->exports;obj++) {
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

void _std log_mdtdump(void) {
   log_mdtdump_int(log_printf);
}

static u32t* sv_mt = 0;

// print memory control table contents (debug)
void _std log_memtable(void* memtable, void* memftable, u8t *memheapres,
                       u32t *memsignlst, u32t memblocks, process_context **memowner)
{
   auto u32t*         mt = sv_mt = memtable;
   auto free_block **mft = memftable;
   auto u8t         *mhr = memheapres;
   u32t          ii, cnt;
   char        sigstr[8];
   sigstr[4] = 0;

   mt_swlock();
   log_it(2,"<=====QS memory dump=====>\n");
   log_it(2,"Total : %d kb\n", memblocks<<6);
   log_it(2,"        addr        sign   pid         size\n");
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
         log_it(2,"%s%s\n", buffer, mi?mi->name:"");
      } else
      if (!mt[ii]&&ii&&mt[ii-1]) {
         if (fb->sign!=FREE_SIGN) log_it(2,"free header destroyd!!!\n");
            else log_it(2,"%5d. %08X - <free>         %7d kb\n",cnt,fb,fb->size<<6);
      }
   }
   log_it(2,"%5d. %08X - end\n",cnt,(u32t)mt+(memblocks<<16));

   log_it(2,"Free table:\n");
   for (ii=0; ii<memblocks; ii++)
      if (mft[ii]) {
         free_block *fb=mft[ii];
         log_it(2,"%d kb (%d):",ii<<6,ii);
         while (fb) {
            log_it(2," %08X",fb);
            if (fb->size!=ii) log_it(2,"(%d!)",fb->size);
            if (fb->sign!=FREE_SIGN) log_it(2,"(S:%08X!)",fb->sign);
            fb=fb->next;
         }
         log_it(2,"\n");
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

void _std log_dumppctx(process_context* pq) {
   char     fmtstr[32];
   u32t         ii;
   mt_prcdata  *pd = (mt_prcdata*)pq->rtbuf[RTBUF_PROCDAT];

   log_it(2," PID %4d : %s\n", pq->pid, pq->self->name);
   if (pq->self)
      log_it(2,"   module : %08X, path : %s\n", pq->self, pq->self->mod_path);
   if (pq->parent)
      log_it(2,"   parent : %08X, path : %s\n", pq->parent, pq->parent->mod_path);
   log_it(2,"    flags : %08X\n", pq->flags);
   log_it(2,"  env.ptr : %08X\n", pq->envptr);

   // buffers can grow in next revisions - so print it in this way
   ii = sizeof(pq->rtbuf)/4;
   snprintf(fmtstr, 24, "    rtbuf : %%%dlb\n", ii/2);
   log_it(2, fmtstr, pq->rtbuf);
   memcpy(fmtstr, "          ", 10);
   log_it(2, fmtstr, pq->rtbuf+ii/2);

   ii = sizeof(pq->userbuf)/4;
   snprintf(fmtstr, 24, "  userbuf : %%%dlb\n", ii/2);
   log_it(2, fmtstr, pq->userbuf);
   memcpy(fmtstr, "          ", 10);
   log_it(2, fmtstr, pq->userbuf+ii/2);

   if (!pd) log_it(2,"  procinfo ptr is NULL!\n"); else {
      char *cdv = pd->piCurDir[pd->piCurDrive];
      log_it(2,"  cur.dir : %c:\\%s\n", pd->piCurDrive+'A', cdv?cdv+3:"");
   }
}

/** dump process tree to log.
    This dump replaced by MTLIB when it active */
void _std START_EXPORT(mod_dumptree)(void) {
   process_context* pq = mod_context();
   log_it(2,"== Process Tree ==\n");
   while (pq) {
      log_dumppctx(pq);
      pq = pq->pctx;
      log_it(2,"------------------\n");
   }
   if (mt_exechooks.mtcb_cfpt)
      log_it(2,"FPU owner pid %u\n", mt_exechooks.mtcb_cfpt->tiPID);
   else
      log_it(2,"FPU is not used\n");
   log_it(2,"==================\n");
}

/** enum all modules in system.
    @param [out] pmodl   List of modules, must be releases via free().
    @return number of modules in returning list */
u32t _std mod_enum(module_information **pmodl) {
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
   rc = (module_information*)malloc_local(sizeof(module_information)*mcnt + slen);
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
      md->tmp       = rp;
      memcpy(&rp->imports, &md->impmod, sizeof(void*)*(MAX_IMPMOD+1));

      md = md->next;
      rp++;
   }
   rp = rc;
   mcnt = ii;
   for (ii=0; ii<mcnt; ii++) {
      u32t idx = 0;
      while (rp->imports[idx]) rp->imports[idx] = (module_information*)
         (((module*)rp->imports[idx])->tmp);
   }
   if (in_mtmode) mt_muxrelease(mod_secondary->ldr_mutex);
   *pmodl = rc;
   return mcnt;
}
