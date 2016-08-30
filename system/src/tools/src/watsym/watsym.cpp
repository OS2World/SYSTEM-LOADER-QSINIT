//
// QSINIT
// watcom map -> sym file converter
//
#define SORT_BY_OBJECTS_ON
#include <stdio.h>
#include <stdarg.h>
#include "classes.hpp"
#include "../tinc2h/sp_stradd.h"
#include "mapsym.h"

TStrings src,  segs;
Strings<u64>  names;
TPtrList     wdlist;
TList        wslist;
TUList       seglen;

int         verbose = 0;

static const char *err_unexp = "Unexpected end of source file!\n",
                  *err_badfm = "Unrecognized format in line %u!\n",
                  *err_unseg = "Unknown segment in line %u!\n",
                  *err_wrerr = "Target file write error!\n";


static void error(const char *fmt, ...) {
   va_list arglist;
   va_start(arglist,fmt);
   vprintf(fmt,arglist);
   va_end(arglist);
   exit(4);
}

u32 seg_indexlen(u32 segno, int &is32, u32 &segp, u32 &count) {
   int   start = -1, end = -1, ii;
   u32  outlen = 0;
   is32  = 0;
   count = 0;
   for (ii=0; ii<=names.Max(); ii++) {
      u64 av = names.Objects(ii);
      if (av>>32 == segno) {
         if (start<0) start = ii;
         end = ii;
         if ((u32)av>=0x10000) is32 = 1;
         outlen += names[ii].length() + 1;
      } else
      if (end>=0) break;
   }
   if (start<0) return 0;
   segp  = start;
   count = end-start+1;

   u32 reslen = outlen + (is32?4:2)*count + 2*count;

   if (verbose) {
      if (segno) printf("seg %04X: ", segno); else
         printf("abs syms: ");
      printf("%u-bit ofs, name index size %u\n", is32?32:16, reslen);
   }
   // names + offsets + index table
   return reslen;
}

void seg_indexbuild(void *mem, int is32, u16 segpos, u32 namepos, u32 count) {
   u16 *index = (u16*)mem;
   u8   *nptr = (u8*)(index + count);

   for (u32 ii=0; ii<count; ii++) {
      u32 ofs = names.Objects(namepos+ii);
      *index++ = segpos + (nptr - (u8*)mem);
      if (is32) { *(u32*)nptr = ofs; nptr+=4; } else
         { *(u16*)nptr = ofs; nptr+=2; }

      spstr fn(names[namepos+ii]);
      *nptr++ = fn.length();
      memcpy(nptr, fn(), fn.length());
      nptr += fn.length();
   }
}


void *seg_build(u32 segno, u32 &size, u32 fileofs) {
   int       is32;
   u32       segp, count,
          namelen = seg_indexlen(segno, is32, segp, count);
   size = 0;
   if (!namelen) return 0;

   spstr  segname = segs[segs.IndexOfObject(segno)];
   u32     fixlen = SYM_SEGDEF_FIXSIZE + segname.length() + 1;
   size = Round16(fixlen + namelen);

   sym_segdef *sd = (sym_segdef*)calloc(1,size);
   sd->num_syms   = count;
   sd->sym_tab_ofs= fixlen;
   sd->load_addr  = segno;
   sd->sym_type   = is32?SYM_FLAG_32BIT:0;
   sd->curr_inst  = 0xFF; // who knows - what is this ;)
   sd->name_len   = segname.length();
   sd->next_ptr   = fileofs + size >> 4;
   memcpy(&sd->name, segname(), segname.length());

   if (verbose) printf("         '%s', %04X bytes, file pos %08X\n",
      segname(), size, fileofs);

   seg_indexbuild((u8*)sd+fixlen, is32, fixlen, segp, count);
   return sd;
}

int main(int argc,char *argv[]) {
   if (argc<2) {
#ifdef SP_OS2
      printf("\t\x1B[1;32mQSINIT\x1B[1;37m tools.\nusage:\twatsym <.map> [<.sym>] [/s] [/b]\x1B[0;37m\n"
#else
      printf("\tQSINIT tools.\nusage:\twatsym <.map> [<.sym>] [/s] [/b]\n"
#endif
             "\t/s - include static symbols (if present in source .map)\n"
             "\t/b - add segment border symbols (as wat2map.cmd do, but only\n"
             "\t     if absent another on the same addr)\n"
             "\t/v - be verbose\n");
      return 1;
   }
   if (!src.LoadFromFile(argv[1])) {
      printf("There is no file \"%s\"\n",argv[1]);
      return 2;
   }

   l ll, statics = 0,
         borders = 0, arg_m = argc;
   spstr recptr;
   while (arg_m>2 && (argv[arg_m-1][0]=='-' || argv[arg_m-1][0]=='/')) {
      char cc = toupper(argv[arg_m-1][1]);
      if (cc=='S') { statics = 1; arg_m--; } else
      if (cc=='V') { verbose = 1; arg_m--; } else
      if (cc=='B') { borders = 1; arg_m--; } else {
         printf("Unknown option \"%s\"\n", argv[arg_m-1]);
         return 3;
      }
   }
   u32  startseg = 0;
   spstr modname;
   ll = 0;
   while (ll<=src.Max()) {
      if (src[ll].trim()=="Segment                Class          Group          Address         Size") {
         ll+=3;
         while (ll<=src.Max() && src[ll].length()) {
            spstr segn = src[ll].word(1),
                  addr = src[ll].word(4),
                 segsz = src[ll].word(5);
            if (addr.words(":")!=2) error(err_badfm,ll+1);

            u32 segv = strtoul(addr(), 0, 16),
                sego = strtoul(addr()+addr.cpos(':')+1, 0, 16),
                segl = strtoul(segsz(), 0, 16);
            l   lpos = segs.IndexOfObject(segv);
            /* use group name for multiple segments in one object
               (except AUTO one) */
            if (lpos<0) {
               segs.AddObject(segn(),segv);
               seglen.Add(sego+segl);
            } else {
               spstr group(src[ll].word(3));
               if (group!="AUTO") segs[lpos] = group;
               // trying to collect segment length
               if (seglen[lpos]<sego+segl) seglen[lpos] = sego+segl;
            }
            ll++;
         }
         if (ll>src.Max()) error(err_unexp);
         continue;
      }
      if (src[ll].trim()=="Address        Symbol") {
         ll+=3;
         while (ll<=src.Max() && src[ll].length()) {
            spstr ln(src[ll],15),
                addr(src[ll],0,14);

            if (ln.pos("::operator")<0 && ln.pos(" operator ")<0 &&
               (statics || addr[13]!='s'))
            {
               if (ln.countchar('(')>1) {
                  l op1 = ln.cpos('('),
                    op2 = ln.cpos('(', op1+1),
                    cl1 = ln.cpos(')', op1+1),
                    cll = ln.crpos(')');
                  // this is type name, remove parens
                  if (op2>cl1 && cl1>0) {
                     ln[op1] = ' ';
                     ln[cl1] = ' ';
                  }
               }
               l opn;
               if (ln.countchar('(')) { opn = ln.cpos('('); ln = ln.left(opn).trim(); }
               if (ln.countchar('[')) { opn = ln.cpos('['); ln = ln.left(opn).trim(); }
               ln.replace("::","__");
               ln.replace("~","d");
               ln = ln.word(ln.words());
               if (ln[0]=='_') ln.del(0,1);


               u32 seg = strtoul(addr(), 0, 16),
                   ofs = strtoul(addr()+addr.cpos(':')+1, 0, 16);
               if (seg && segs.IndexOfObject(seg)<0) error(err_unseg, ll+1);

               //printf("%04X:%08X %s\n", seg, ofs, ln());

               names.AddObject(ln, (u64)seg<<32 | ofs);
            }
            ll++;
         }
         if (ll>src.Max()) error(err_unexp);
         continue;
      }
      if (src[ll](0,21)=="Entry point address: ") {
         startseg = strtoul(src[ll]()+21, 0, 16);
         if (!startseg) error("Bad entry point format");
         if (segs.IndexOfObject(startseg)<0) error(err_unseg, ll+1);
      } else
      if (src[ll](0,18)=="Executable Image: ") {
         spstr ename(src[ll],18);
         modname = ename.word(1,".");
      }
      ll++;
   }
   // adding segment borders
   if (borders) {
      for (ll=0; ll<segs.Count(); ll++) {
         u32 segno = segs.Objects(ll);
         u64   sv1 = (u64)segno<<32,
               sv2 = sv1 + seglen[ll];
         spstr  cn;
         int    lp = names.IndexOfObject(sv1);
         if (lp<0) names.AddObject(cn.sprintf("SEG%04X_%s_START", segno,
                                   segs[ll]()), sv1);
         lp = names.IndexOfObject(sv2);
         if (lp<0) names.AddObject(cn.sprintf("SEG%04X_%s_END", segno,
                                   segs[ll]()), sv2);
      }

   }

   names.SortByObjects();
   if (!modname) error("Unable to locate module name");

   u32 maxlen = 0;
   for (ll=0; ll<names.Count(); ll++)
      if (names[ll].length()>maxlen) maxlen = names[ll].length();

   int          is32 = 0;
   u32   sze, si_pos, si_count,
            abs_nlen = seg_indexlen(0, is32, si_pos, si_count),
         purehdrsize = SYM_HEADER_FIXSIZE + modname.length() + 1,
          headersize = Round16(purehdrsize + abs_nlen),
               f_ofs = headersize;
   sym_header   *hdr = (sym_header*)calloc(1,headersize);
   hdr->entry_seg    = startseg;
   hdr->abs_tab_ofs  = si_count?purehdrsize:0;
   hdr->max_sym_len  = maxlen;
   hdr->name_len     = modname.length();
   hdr->abs_type     = is32?SYM_FLAG_32BIT:0;
   hdr->abs_sym_count= si_count;
   hdr->seg_ptr      = f_ofs >> 4;
   memcpy(&hdr->name, modname(), modname.length());

   if (si_count)
      seg_indexbuild((u8*)hdr+purehdrsize, is32, purehdrsize, si_pos, si_count);

   for (ll=0; ll<segs.Count(); ll++) {
      u32   seg_sz;
      void  *seg_m = seg_build(segs.Objects(ll), seg_sz, f_ofs);
      // non-empty?
      if (seg_m) {
         wdlist.Add(seg_m);
         wslist.Add(seg_sz);
         f_ofs += seg_sz;
      }
   }
   hdr->num_segs     = wdlist.Count();
   hdr->next_ptr     = f_ofs >> 4;
   // zero next in last record
   if (wdlist.Count()) ((sym_segdef *)wdlist[wdlist.Max()])->next_ptr = 0;
   // say something
   if (verbose) {
      printf("module '%s', %u segs, %u abs\n", modname(), wdlist.Count(),
         hdr->abs_sym_count);
      if (wdlist.Count() != segs.Count())
         printf("%u segs eliminated (empty)\n", segs.Count()-wdlist.Count());
   }

   sym_endmap   ehdr = { 0, SYM_VERSION_MINOR, SYM_VERSION_MAJOR };
   spstr dstname(arg_m>2?argv[2]:strChangeFileExt(argv[1],"sym")());

   FILE *dst = fopen(dstname(), "wb");
   if (!dst) error("Unable to create target file \"%s\"\n", dstname());
   // header
   if (fwrite(hdr, 1, headersize, dst) != headersize) error(err_wrerr);
   // segments
   for (ll=0; ll<wdlist.Count(); ll++)
      if (fwrite(wdlist[ll], 1, wslist[ll], dst) != wslist[ll]) error(err_wrerr);
   // eof
   if (fwrite(&ehdr, 1, sizeof(hdr), dst) != sizeof(hdr)) error(err_wrerr);
   // done
   fclose(dst);
   // leave all garbage for the system - it is smart enough for this ;)
   return 0;
}
