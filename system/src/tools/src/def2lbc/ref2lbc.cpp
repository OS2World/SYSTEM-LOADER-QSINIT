/* 
  ref file converter...
*/

#include "classes.hpp"
#include <io.h>

#define IMPORT_PREFIX  "__import__"

typedef Strings<ptr> TTreeList;
typedef TTreeList   *PTreeList;

void ErrorExit(spstr error) {
   printf("%s\n",error());
   exit(77);
}

set alnum,digit;

TTreeList *ParseVarList(pchar&pos);

Bool ParseVar(pchar&pos,spstr&var,spstr&value,PTreeList&lst) {
   char*start=pos;
   lst=NULL;
   var=value=spstr();
   if (alnum[(b)(*pos)]||*pos=='-'||*pos=='\"') {
      l len=0;
      if (*pos=='\"') {
         char *end=strchr(pos+1,'\"');
         if (!end) {
            ErrorExit("Unclosed string at the end of file!");
            return false;
         } else len=end-pos+1;
      } else len=strcspn(pos," ={}");
      var=spstr(pos,len);
      pos+=len;
      while (*pos==' ') pos++;
      if ((digit[(b)(*start)]||*start=='-')&&*pos!='=') return true;
      if (*pos=='=') {
         pos++; while (*pos==' ') pos++;
         if (*pos!='{') {
            if (*pos=='\"') {
               char *end=strchr(pos+1,'\"');
               if (!end) {
                  ErrorExit("Unclosed string at the end of file!");
                  return false;
               } else len=end-pos+1;
            } else len=strcspn(pos," ={}");
            value=spstr(pos,len);
            pos+=len;
            while (*pos==' ') pos++;
            return true;
         }
      } else
      if (*pos=='{') {
         ErrorExit(spstr().sprintf("\"{\" after var name in string: %s",spstr(start,pos-start+10)()));
         return false;
      } else
      if (*pos=='}') return true;
   } else
   if (*pos!='{') {
      ErrorExit(spstr().sprintf("Unknown character: %#x in string: %s",*pos,spstr(pos-10,20)()));
      return false;
   }
   if (*pos=='{') {
      pos++;
      lst=ParseVarList(pos);
      if (*pos=='}') pos++;
      while (*pos==' ') pos++;
   }
   return true;
}

TTreeList *ParseVarList(pchar&pos) {
   TTreeList*lst=new TTreeList;
   while (*pos==' ') pos++;
   while (*pos&&*pos!='}') {
      spstr var,value;
      PTreeList clst=NULL;
      if (ParseVar(pos,var,value,clst))
         lst->AddObject(!value?var:var+spstr(" = ")+value,(ptr)clst);
   }
   return lst;
}

void SaveTree(TTreeList&lst,TStrings&result,spstr&border);

void SaveVar(TTreeList&lst,d Entry,TStrings&result,spstr&border,Bool Flat=false) {
   TTreeList*obj=(TTreeList*)lst.Objects(Entry);
   if (obj) {
      if (obj->Count()>0) {
         result<<border+(!lst[Entry]?spstr("{"):lst[Entry]+spstr(" = { "));
         l pos=result.Count();
         border+="   ";
         SaveTree(*obj,result,border);
         border.del(0,3);
         if (pos==result.Count()) result[result.Max()]+="} "; else
            result<<border+spstr("} ");
      } else 
         result<<border+lst[Entry]+spstr(" = { } ");
   } else
   if (!Flat) result<<border+lst[Entry]; else 
      result[result.Max()]+=lst[Entry]+spstr(" ");
}

void SaveTree(TTreeList&lst,TStrings&result,spstr&border) {
   if (lst.Count()>0) {
      Bool Flat=lst.Count()<3||lst.Value((l)0).length()==0&&lst.Objects((l)0)==0;
      for (d ll=0;ll<lst.Count();ll++) SaveVar(lst,ll,result,border,Flat);
   }
}

void FreeTree(TTreeList*lst) {
   for (l ll=0;ll<lst->Count();ll++) {
      TTreeList*obj=(TTreeList*)lst->Objects(ll);
      if (obj) {
         FreeTree(obj);
         delete obj;
         lst->Objects(ll)=NULL;
      }
   }
}

// поиск Key, соотв. условию
// Key = {
//    SubKey = SubValue
//
l FindVarPos(TTreeList&lst,const spstr Key,const char*SubKey=NULL,const char*SubValue=NULL,l fpos=0) {
   if (fpos>lst.Max()) return -1;
   do {
      fpos=lst.IndexOfName(Key,fpos);
      if (fpos>=0) {
         if (!SubKey) return fpos; else {
            TTreeList*cl=(TTreeList*)lst.Objects(fpos);
            if (!SubValue) {
               if (cl->IndexOfName(SubKey)>=0) return fpos;
            } else
               if (cl->Value(SubKey).trim().upper()==spstr(SubValue).upper()) return fpos;
         }
      } else return -1;
      fpos++;
   } while (true);
}

Bool SetSimpleValue(TTreeList&lst,const spstr name,const spstr value) {
   l ps=lst.IndexOfName(name);
   if (ps>=0) lst[ps]=name+spstr(" = ")+value;
   return ps>=0;
}

Bool IsValueOn(TTreeList&lst,const char *name) {
   spstr value = lst.Value(name).trim().upper().unquote();
   return value=="YES" || value.Int()!=0;
}

TTreeList *ReadTreeFile(const spstr &name,Bool UnComment) {
   FILE *svh=fopen(name(),"rb");
   if (svh) {
      spstr  save;
      d       fsz;
      char    *sv=save.LockPtr((fsz=fsize(svh))+1);
      fread(sv,1,fsz,svh);
      save.UnlockPtr(fsz);
      fclose(svh);
      if (UnComment) {
         TStrings st;
         st.SetText(save());
         for (l ii=0;ii<st.Count();ii++) {
            int ps=st[ii].cpos('#');
            if (ps>=0) st[ii].del(ps,65536);
            ps=st[ii].cpos('\xA4');
            if (ps>=0) st[ii].del(ps,65536);
         }
         save=st.GetTextToStr(" ");
      } else {
         save.replacechar(10,' ');
         save.replacechar(13,' ');
      }
      save.replacechar(26,' ');
      save.replacechar(9,' ');
      sv=save.LockPtr(1);
      TTreeList *rc=ParseVarList(sv);
      save.UnlockPtr();
      return rc;
   } else
      return 0;
}


class TraceFile {
   spstr       fname;
   char         *mem;
   d     alloc, used;
   Bool      written;
   void addspace(d more) {
      if (more>alloc-used) {
         mem = (char*)realloc(mem, alloc+=more*2);
         if (!mem) ErrorExit("Out of memory!");
      }
   }
public:
   TraceFile(const spstr &name) : fname(name) { mem=0; alloc=0; used=0; written=0; }
   Bool WriteFile() {
      if (!written && used) {
         FILE *ff = fopen(fname(),"wb");
         if (!ff) return False;
         if (fwrite(mem,1,used,ff)==used) written = True;
         fclose(ff);
         if (!written) unlink(fname());
      }
      return written;
   }
   ~TraceFile() { 
      if (used) { WriteFile(); free(mem); mem=0; used=0; alloc=0; }
   }
   void Push(const void *data, d len) { 
      if (len) {
         addspace(len);
         memcpy(mem+used, data, len);
         used+=len;
      }
   }
   void Push(const char *str) {
      int len = strlen(str);
      if (len>255) len=255;
      Push(&len,1);
      if (len) Push(str,len);
   }
   void Push(const spstr &str) { Push(str()); }
   void Push(w value) { Push(&value,2); }
};

void about(void) {
   printf(" ref2lbc - QS ref file conversion:\n\n"
          " * write watcom linker batch file: \n"
          "       def2lbc lbc <in_ref> <out_lbc>\n\n"
          " * write asm jumping code and def for implib: \n"
          "       def2lbc asm <in_ref> <out_asm> <out_def>\n\n"
          " * write pure def: \n"
          "       def2lbc def <in_ref> <out_def>\n\n"
          " * write trace file: \n"
          "       def2lbc trc <in_ref> <out_trc>\n\n"
          " * write header with ordinals: \n"
          "       def2lbc ord <in_ref> <out_h>\n\n");
   exit(1);
}

int main(int argc, char *argv[]) {
   if (argc<4) about();

   alnum.Include('0','9');
   alnum.Include('A','Z');
   alnum.Include('a','z');
   alnum.Include('a','z');
   alnum.Include('_');
   digit.Include('0','9');

   TStrings defs, lbc, mdef;
   spstr op(argv[1]);
   int mode = 0;

   if (op.upper()!="ASM" && op!="LBC" && op!="DEF" && op!="TRC" && op!="ORD") {
      printf("Invalid mode requsted: \"%s\", exiting...\n",op());
      return 1;
   } else
      mode = op[0]=='A'?1:0;
   // additional check
   if (argc<4+mode) about();

   TTreeList *refs = ReadTreeFile(argv[2],True);

   if (!refs) {
      printf("Unable to open \"%s\", exiting...\n",argv[2]);
      return 2;
   }

   spstr modname = refs->Value("name").trim().upper().unquote(),
         modtype = refs->Value("type").trim().upper().unquote(),
         errstr;
   int err=true;
   do {
      if (modtype!="EXE" && modtype!="DLL") {
         errstr.sprintf("Error! Invalid module type: \"%s\"",modtype);
      }
      l pos = FindVarPos(*refs,"exports"), ii;
      if (pos<0) {
         errstr="Missing EXPORTS section!";
         break;
      }
      // exports tree
      TTreeList *exl=(TTreeList*)refs->Objects(pos);

      if (op[0]=='A') {
         spstr wrk;
         wrk.sprintf("%s %s", modtype=="EXE"?"NAME":"LIBRARY", modname());
         mdef<<wrk;
         mdef<<""<<"EXPORTS";

         lbc.Add("; generated file, do not modify");
         lbc.Add("\t\t.486p\n");

         d segcount = 0;
         spstr ipfx(IMPORT_PREFIX);

         char *pbuf = (char*)malloc(16384);

         for (ii=0; ii<exl->Count(); ii++)
            if ((*exl)[ii].trim().length() && exl->Objects(ii)) {
               TTreeList *edat = (TTreeList*)exl->Objects(ii);
               Bool  is_offset = IsValueOn(*edat, "offset");

               spstr func((*exl)[ii].trim()), mfunc;
               mfunc = ipfx + func;

               if (is_offset) {
                  wrk.sprintf("  %-50s @%d   NONAME", func(), edat->IntValue("index"));
                  mdef<<wrk;
               } else {
                  sprintf(pbuf,"IMPSEG%04X\tsegment byte public use32 'CODE'",segcount);
                  lbc<<pbuf;
                  sprintf(pbuf,"\t\tpublic\t%s",func());
                  lbc<<pbuf;
                  sprintf(pbuf,"\t\textrn\t%s:near",mfunc());
                  lbc<<pbuf;
                  sprintf(pbuf,"%s:",func());
                  lbc<<pbuf;
                  sprintf(pbuf,"\t\tjmp\t%s",mfunc());
                  lbc<<pbuf;
                  sprintf(pbuf,"IMPSEG%04X\tends\n",segcount);
                  lbc<<pbuf;
                  
                  segcount++;
                  wrk.sprintf("  %-50s @%d   NONAME", mfunc(), edat->IntValue("index"));
                  mdef<<wrk;
               }
            }
         lbc.Add("\t\tend");
         free(pbuf);

         if (!mdef.SaveToFile(argv[4])) {
            printf("Unable to save \"%s\", exiting...\n",argv[4]);
            return 3;
         }
      } else 
      if (op[0]=='L') {
         spstr wrk;
         for (ii=0; ii<exl->Count(); ii++)
            if ((*exl)[ii].trim().length() && exl->Objects(ii)) {
               TTreeList *edat = (TTreeList*)exl->Objects(ii);
               spstr expname = edat->Value("expname").trim().unquote();
               if (!expname) expname = (*exl)[ii].trim();
               wrk.sprintf("++%s.%s.%d",expname(), modname(), edat->IntValue("index"));
               lbc<<wrk;
            }
      } else 
      if (op[0]=='D') {
         spstr wrk;
         wrk.sprintf("%s %s", modtype=="EXE"?"NAME":"LIBRARY", modname());
         lbc<<wrk;
         lbc<<""<<"EXPORTS";
         for (ii=0; ii<exl->Count(); ii++)
            if ((*exl)[ii].trim().length() && exl->Objects(ii)) {
               TTreeList *edat = (TTreeList*)exl->Objects(ii);
               wrk.sprintf("  %-30s @%d   NONAME", (*exl)[ii].trim()(), 
                  edat->IntValue("index"));
               lbc<<wrk;
            }
      } else 
      if (op[0]=='O') {
         spstr wrk, defn;
         modname.upper();
         defn.sprintf("%s_ORDINAL_GEN_H", modname());
         lbc<<spstr("#ifndef ")+defn;
         lbc<<spstr("#define ")+defn<<"";
         for (ii=0; ii<exl->Count(); ii++)
            if ((*exl)[ii].trim().length() && exl->Objects(ii)) {
               TTreeList *edat = (TTreeList*)exl->Objects(ii);
               wrk.sprintf("#define ORD_%s%-50s (%d)", modname(), 
                  (*exl)[ii].trim()(), edat->IntValue("index"));
               lbc<<wrk;
            }
         lbc<<""<<spstr("#endif // ")+defn;
      } else 
      if (op[0]=='T') {
         Strings<TList*> groups;

         for (ii=0; ii<exl->Count(); ii++)
            if ((*exl)[ii].trim().length() && exl->Objects(ii)) {
               TTreeList *edat = (TTreeList*)exl->Objects(ii);
               Bool  is_offset = IsValueOn(*edat, "offset");
               int       index = edat->IntValue("index");
               spstr     group = edat->Value("group").trim().unquote(),
                        format = edat->Value("format").trim().unquote();
               int         idx = groups.IndexOfName(group);

               if (index<=0 || is_offset || !group || !format) continue;
               if (idx<0) {
                  TList *memb = new TList;
                  idx = groups.AddObject(group,memb);
                  memb->Add(ii);
               } else
                  groups.Objects(idx)->Add(ii);
            }
         TraceFile trf(argv[3]);
         trf.Push("QSTRACE");
         trf.Push(modname);
         trf.Push(groups.Count());
         for (ii=0; ii<groups.Count(); ii++) trf.Push(groups[ii]);

         for (ii=0; ii<groups.Count(); ii++) {
            TList *el = groups.Objects(ii);
            trf.Push(el->Count());
            for (l idx=0; idx<el->Count(); idx++) {
               TTreeList *edat = (TTreeList*)exl->Objects((*el)[idx]);
               Bool  is_offset = IsValueOn(*edat, "offset");
               d         index = edat->DwordValue("index");
               spstr    format = edat->Value("format").trim().unquote(),
                          name = (*exl)[(*el)[idx]];
               if (name[0]=='_') name.del(0);
               trf.Push(index);
               trf.Push(name);
               trf.Push(format);
            }
         }

         // delete TLists
         FreeItems(groups);
         if (!trf.WriteFile()) {
            printf("Unable to save \"%s\", exiting...\n",argv[3]);
            return 3;
         }
         return 0;
      }

      err=false;
   } while (false);

   if (err) {
      printf("%s in file \"%s\"",argv[1],errstr());
      return 2;

   }

   if (!lbc.SaveToFile(argv[3])) {
      printf("Unable to save \"%s\", exiting...\n",argv[3]);
      return 3;
   }
   return 0;
}
