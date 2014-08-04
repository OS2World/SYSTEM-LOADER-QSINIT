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
         char st[256];
         int  len;
         log_it(2,"\n");
         log_it(2,"%s, %d objects, base addr: 0x%08X, usage %d\n", md->name,
            md->objects,md->baseaddr,md->usage);
         log_it(2,"  objects:\n");
         for (obj=0;obj<md->objects;obj++) {
            u32t fl=md->obj[obj].flags, jj;
            static u32t objflags[]={OBJREAD, OBJWRITE, OBJEXEC, OBJSHARED, OBJPRELOAD, OBJALIAS16,
                                    OBJBIGDEF, OBJCONFORM, OBJIOPL, 0};
            static char  *objstr[]={"READ", "WRITE", "EXEC", "SHARED", "PRELOAD", "ALIAS",
                                    "BIG", "CONFORM", "IOPL", 0};

            len=sprintf(st,"  %2d. 0x%08x, size %6d",obj+1,md->obj[obj].address,md->obj[obj].size);
            if (md->obj[obj].sel && md->obj[obj].sel>SEL32DATA)
               len+=sprintf(st+len,", sel %04x",md->obj[obj].sel);
            len+=sprintf(st+len,", flags %08x (",fl);

            for (jj=0;objflags[jj];jj++)
               if (fl&objflags[jj]) len+=sprintf(st+len,"%s ",objstr[jj]);
            if (st[len-1]==' ') len--;

            strcpy(st+len,")\n");
            log_it(2,st);
         }
         obj=0; len=0;
         while ((impm=md->impmod[obj++])!=0) {
            if (obj==1) len=sprintf(st,"  used modules: ");
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
void _std log_memtable(void* memtable, void* memftable, u32t memblocks) {
   auto u32t*         mt= memtable;
   auto free_block **mft=memftable;
   u32t ii,cnt;
   log_it(2,"<=====QS memory dump=====>\n");
   log_it(2,"Total : %d kb\n",memblocks<<6);
   for (ii=0,cnt=0;ii<memblocks;ii++,cnt++) {
      free_block *fb=(free_block*)((u32t)mt+(ii<<16));
      if (mt[ii]&&mt[ii]<FFFF) {
         log_it(2,"%5d. %08X - %d kb (%d)\n",cnt,fb,Round64k(mt[ii])>>10,mt[ii]);
      } else
      if (!mt[ii]&&ii&&mt[ii-1]) {
         if (fb->sign!=FREE_SIGN) log_it(2,"free header destroyd!!!\n");
            else log_it(2,"%5d. %08X - free %d kb\n",cnt,fb,fb->size<<6);
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
