//
// QSINIT "start" module
// dump module table
//
#include "stdlib.h"
#include "qsutil.h"
#define MODULE_INTERNAL
#include "qsmod.h"
#include "qsint.h"
#include "qsinit.h"
#include "qslog.h"
#include "qslxfmt.h"

extern module * _std mod_list, * _std mod_ilist; // loaded and loading module lists

void _std log_mdtdump(void) {
   int ii=2,obj;
   log_it(2,"== MDT contents ==\n");
   log_it(2,"* Loaded list:\n");
   while (ii--) {
      module *md=ii?mod_list:mod_ilist, *impm;
      if (!md) log_it(2,"   empty!\n");

      while (md) {
         char      st[256];
         u16t  flatcs = get_flatcs();
         int      len;
         log_it(2,"\n");
         log_it(2,"%s, %d objects, base addr: 0x%08X, usage %d\n", md->name,
            md->objects,md->baseaddr,md->usage);
         log_it(2,"  objects:\n");
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
            log_it(2,st);
         }
         obj=0; len=0;
         while ((impm=md->impmod[obj++])!=0) {
            if (obj==1) len=sprintf(st,"  uses modules: ");
            len+=sprintf(st+len,"%s ",impm->name);
         }
         if (len) { log_it(2,"\n"); log_it(2,"%s\n",st); }
         if (md->exports) {
            log_it(2,"\n"); 
            log_it(2,"  exports:\n");
            for (obj=0;obj<md->exports;obj++) {
               mod_export *ee=md->exps+obj;
               if ((obj&3)==0) len=sprintf(st,"  ");
               if (ee->forward) len+=sprintf(st+len,"%4d.<FORWARD> ",ee->ordinal); else
               if (ee->is16) len+=sprintf(st+len,"%4d.%04X:%04X ",ee->ordinal,ee->sel,
                  ee->address);
               else len+=sprintf(st+len,"%4d.%08X  ",ee->ordinal,ee->address);

               if ((obj&3)==3||obj+1==md->exports) log_it(2,"%s\n",st);
            }
         }

         log_it(2,"\n");
         md=md->next;
      }


      if (ii) log_it(2,"* Loading list:\n");
   }
   log_it(2,"==================\n");
}

// print memory control table contents (debug)
void _std log_memtable(void* memtable, void* memftable, u8t *memheapres, u32t *memsignlst, u32t memblocks) {
   auto u32t*         mt =   memtable;
   auto free_block **mft =  memftable;
   auto u8t         *mhr = memheapres;
   u32t          ii, cnt;
   char        sigstr[8];
   sigstr[4] = 0;
   log_it(2,"<=====QS memory dump=====>\n");
   log_it(2,"Total : %d kb\n",memblocks<<6);
   for (ii=0,cnt=0;ii<memblocks;ii++,cnt++) {
      free_block *fb=(free_block*)((u32t)mt+(ii<<16));
      if (mt[ii]&&mt[ii]<FFFF) {
         *(u32t*)&sigstr = memsignlst[ii];
         log_it(2,"%5d. %08X - %c %-4s  %7d kb (%d)\n", cnt, fb, 
            mhr[ii>>3]&1<<(ii&7) ? 'R' : ' ', sigstr, Round64k(mt[ii])>>10, mt[ii]);
      } else
      if (!mt[ii]&&ii&&mt[ii-1]) {
         if (fb->sign!=FREE_SIGN) log_it(2,"free header destroyd!!!\n");
            else log_it(2,"%5d. %08X - <free>  %7d kb\n",cnt,fb,fb->size<<6);
      }
   }
   log_it(2,"%5d. %08X - end\n",cnt,(u32t)mt+(memblocks<<16));

   log_it(2,"Free table:\n");
   for (ii=0;ii<memblocks;ii++)
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
}

/// dump process tree to log
void _std mod_dumptree(void) {
   process_context* pq = mod_context();
   log_it(2,"== Process Tree ==\n");
   while (pq) {
      char fmtstr[32];
      log_it(2," PID %4d : %s\n", pq->pid, pq->self->name);
      if (pq->self)
         log_it(2,"   module : %08X, path : %s\n", pq->self, pq->self->mod_path);
      if (pq->parent)
         log_it(2,"   parent : %08X, path : %s\n", pq->parent, pq->parent->mod_path);
      log_it(2,"    flags : %08X\n", pq->flags);
      log_it(2,"  env.ptr : %08X\n", pq->envptr);

      // buffers can grow in next revisions - so print it in this way
      snprintf(fmtstr, 24, "    rtbuf : %%%dlb\n", sizeof(pq->rtbuf)/4);
      log_it(2, fmtstr, pq->rtbuf);
      snprintf(fmtstr, 24, "  userbuf : %%%dlb\n", sizeof(pq->userbuf)/4);
      log_it(2, fmtstr, pq->userbuf);
      
      pq = pq->pctx;
      if (pq) log_it(2,"------------------\n");
   }
   log_it(2,"==================\n");
}
