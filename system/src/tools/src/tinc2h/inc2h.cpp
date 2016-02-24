#include <time.h>
#include <ctype.h>
#include <stdio.h>
#include "sp_stradd.h"
#include "classes.hpp"

TStrings src, dst, repops, repc, types;

const char *OP_DELIM = " \t+-*/()";

spstr replaceops(spstr str, d line, const char *fname) {
   l wrds=str.words(OP_DELIM), ii;
   for (ii=1;ii<=wrds;ii++) {
      spstr wrdi = str.word(ii,OP_DELIM).upper();
      l idx=repops.IndexOf(wrdi);

      if (idx>=0) str.replaceword(ii, OP_DELIM, repc[idx]()); else
      if (isdigit(wrdi[0])) {
         char suffix = toupper(wrdi.lastchar()), *eptr;
         int    base = suffix=='H'?16:(suffix=='B'?2:10);
         if (base!=10) wrdi.dellast();
         d     value = strtoul(wrdi(), &eptr, base);

         if (eptr-wrdi() < wrdi.length())
            printf("WARNING: integer error, line %d, file \"%s\"\n", line, fname);
         wrdi.sprintf(base!=10?"0x%8.8X":"%d", value);
         str.replaceword(ii, OP_DELIM, wrdi());
      }
   }
   for (ii=1;ii<=wrds;ii++)
      if (str.word(ii,OP_DELIM).upper()=="SIZE"&&ii<wrds) {
          spstr type(str.word(ii+1,OP_DELIM)), tmp;
          if (types.IndexOfName(type)>=6) type.insert("struct ",0);

          tmp = str.word(ii,OP_DELIM).lower(); tmp+= "of";
          str.replaceword(ii,OP_DELIM,tmp());
          tmp.sprintf("(%s)", type());
          str.replaceword(ii+1,OP_DELIM,tmp());
      }
   return str;
}

spstr &trimc(spstr &str) {
	return str.replace("*/","  ").replace("/*","  ").trimright();
}

int main(int argc,char *argv[]) {
  if (argc<2) {
#ifdef SP_OS2
    printf("\t\x1B[1;32mQSINIT\x1B[1;37m tools.\nusage:\tinc2h <.inc> [<.h>] [/t]\x1B[0;37m\n"
#else
    printf("\tQSINIT tools.\nusage:\tinc2h <.inc> [<.h>] [/t]\n"
#endif
           "\t/tX - use types: t=Toolkit, q=QSInit, x=for 64bit code\n\t/pX - add #pragma pack(x)\n\t/rY - make *struct with Y prefix\n"
           "\t/k  - add *_t typedef for *_s struct types\n");
    return 1;
  }
  if (!src.LoadFromFile(argv[1])) {
    printf("There is no file \"%s\"\n",argv[1]);
    return 2;
  }

  l ll, in_macro=0, unckcnt, tktype=0, ppack=0, make_t=0;
  spstr recptr;
  while (argc>2&&(argv[argc-1][0]=='-'||argv[argc-1][0]=='/')) {
    if (toupper(argv[argc-1][1])=='T') {
       char typeltr = argv[--argc][2];
       switch (toupper(typeltr)) {
          case 0:
          case 'T': tktype=1; break;
          case 'X': tktype=2; break;
          case 'Q': tktype=3; break;
          default:
             printf("Invalid type letter (%c)\n",typeltr);
             return 7;
       }
    } else
    if (toupper(argv[argc-1][1])=='K') { make_t=1; argc--; } else
    if (toupper(argv[argc-1][1])=='R') recptr=spstr(argv[--argc]+2); else
    if (toupper(argv[argc-1][1])=='P') {
      ppack=atoi(argv[--argc]+2);
      if (!ppack) ppack=1;
    } else argc--;
  }
  for (ll=0;ll<=src.Max();ll++) src[ll].expandtabs().trimright();

  spstr recname, nexttype, tmp;
  time_t tt=time(0);
  tmp.sprintf("// Generated by inc2h at %s",ctime(&tt));
  dst << tmp;
  if (ppack) dst<<spstr().sprintf("#pragma pack(%d)",ppack)<<"";

  types<<"DB"<<"DW"<<"DD"<<"DF"<<"DQ"<<"DT";
  repops<<"AND"<<"OR"<<"XOR"<<"NOT"<<"MOD"<<"SHL"<<"SHR";
  repc<<"&"<<"|"<<"^"<<"~"<<"%"<<"<<"<<">>";
  ll=0;
  while (ll<=src.Max()) {
    spstr str(src[ll++]),cm,op;

    if (str[0]==';'&&!in_macro) {
      if (str[1]=='c'&&str[2]=='c') dst<<str.del(0,4); else
      if (str[1]=='c'&&str[2]=='r') { dst<<str.del(0,4); ll++; } else {
        trimc(str);
        if (str[1]=='c'&&str[2]=='t') nexttype=str.del(0,4); else
        if (src[ll][0]==';'&&src[ll][1]!='c') {
          dst << str.del(0).insert("/*",0);
          while (src[ll][0]==';'&&src[ll][1]!='c'&&ll<=src.Max())
            dst << spstr("  ")+trimc(src[ll++]).del(0);
          dst[dst.Max()]+="*/";
        } else
          dst << str.del(0).insert("//",0);
      }
      continue;
    }
    trimc(str);

    l cmpos=str.cpos(';'),dstpos=dst.Max();
    if (cmpos>0)
      if (str[cmpos-1]=='\'') cmpos=0;
    if (cmpos>0) cm=str(cmpos,65535);
    str.del(cmpos,65535).trim();

    op=str.word(1).upper();
    if (!in_macro&&op=="INCLUDE") {
      tmp.sprintf("#include \"%s\"",str.word(2)());
      dst << tmp;
    } else
    if (op=="PUBLIC") continue; else
    if (op=="IRP") in_macro++; else
    if (op=="ENDM") {
      if (--in_macro<0) {
        printf("MACRO/ENDM mismatch: line %d, file \"%s\"\n",ll,argv[1]);
        return 4;
      }
    } else {
      spstr type(str.word(2).upper());
      if (type=="MACRO") {
        in_macro++;
      } else
      if (type=="EQU"||type=="=") {
        if (!in_macro) {
          int ps=str.wordpos(3);

          dst<<str.word(1).insert("#define ",0);
          while (dst[dst.Max()].length()<40) dst[dst.Max()]+=" ";

          if (isdigit(str[ps])) {
            char suffix=toupper(str.lastchar()), *eptr;
            int base=suffix=='H'?16:(suffix=='B'?2:10), ps=str.wordpos(3);
            if (base!=10) str.dellast();
            d value=strtoul(str()+ps,&eptr,base);
            if (eptr-(str()+ps)<str.length()-ps) {
              printf("WARNING: integer error, line %d, file \"%s\"\n",ll,argv[1]);
            }
            dst[dst.Max()]+=spstr().sprintf(base!=10?"0x%8.8X":"%d",value);
          } else
            dst[dst.Max()]+=replaceops(str(ps,65535), ll, argv[1]);
        }
      } else
      if (!in_macro&&type=="STRUC") {
        if (!recname) {
          recname=str.word(1);
          types<<op;
          unckcnt=0;
          tmp.sprintf("struct %s {", recname());
          dst << tmp;
        } else {
          printf("Nested STRUCT: line %d, file \"%s\"\n",ll,argv[1]);
          return 5;
        }
      } else
      if (!in_macro&&type=="ENDS") {
        if (recname==str.word(1)) {
          dst<<"};"<<spstr();
          int add_t=make_t&&TOUPPER(recname.lastchar())=='S'&&recname[recname.length()-2]=='_';
          if (add_t||recptr.length()) {
            spstr td;
            td.sprintf("typedef struct %s\t", recname());
            if (add_t) td+=recname.dellast()+="t";
            if (recptr.length()) {
              if (add_t) td+=",";
              td += "*";
              td += recptr+recname;
            }
            td+=";";
            dst<<td<<spstr();
          }
          recname.clear();
        }
      } else
      if (type=="SEGMENT") continue; else
      if (!in_macro&&recname.length()&&(str.words()>2||types.IsMember(op)&&str.words()>1)) {
        static const char *recsize[6] = {"DB","DW","DD","DF","DQ","DT"};
        static int         sizeval[6] = {1,2,4,6,8,10};
        static const char *typenam[4][6]= {
           {"unsigned char","unsigned short","unsigned long",0,"unsigned long long",0}, 
           {"UCHAR"        ,"USHORT"        ,"ULONG"        ,0,"ULONGLONG"         ,0},
           {"unsigned char","unsigned short","unsigned int" ,0,"unsigned long"     ,0},
           {"u8t"          ,"u16t"          ,"u32t"         ,0,"u64t"              ,0}};
        int    size=0, ps, sizeidx=0;

        if (types.IsMember(op)) {
          str=spstr().sprintf("unnamed_field_%3.3d  ",++unckcnt)+str;
          type=op;
        }
        if (str.cpos(',')>0) {
          printf("Unsupported ',' in struct: line %d, file \"%s\"\n",ll,argv[1]);
          return 6;
        }

        for (sizeidx=0;sizeidx<6;sizeidx++)
          if (type==recsize[sizeidx]) { size=sizeval[sizeidx]; break; }
        ps=str.wordpos(3);
        if (ps<=0) {
          printf("WARNING: record field skipped, line %d, file \"%s\"\n",ll,argv[1]);
        } else {
          spstr duptext;
          l dup=str.pos("dup",ps);
          if (dup<0) dup=str.pos("DUP",ps);
          if (dup>0&&str[dup-1]==' ') {
            duptext=str(ps,dup-ps).trim();
            if (isdigit(duptext[0])) {
              char suffix=toupper(duptext.upper().lastchar()),*eptr;
              int base=suffix=='H'?16:(suffix=='B'?2:10);
              if (base!=10) duptext.dellast();
              char lch=duptext.lastchar();
              if (!isdigit(lch)&&(base!=16||lch<='A'&&lch>='F')) dup=0; else {
                dup=strtoul(duptext(),&eptr,base);
                if (eptr-duptext()<duptext.length()) {
                  printf("WARNING: field size error, line %d, file \"%s\"\n",ll,argv[1]);
                }
              }
            } else dup=0;
          } else dup=0;

          if (size&&typenam[tktype][sizeidx]) {
            dst<<(!nexttype?spstr(typenam[tktype][sizeidx]):nexttype)+spstr(" ");
          } else
          if (size) {
            dst<<(!nexttype?spstr(typenam[tktype][1]):nexttype)+spstr(" ");
            size>>=1;
            if (dup) dup*=size; else
            if (duptext.length()) {
              tmp = duptext;
              duptext.sprintf("%d * (%s)", size, duptext());
            } else 
              dup = size;
          } else {
            if (nexttype.length()) dst<<nexttype+spstr(" "); else
              dst<<spstr(str.word(3+(dup?2:0))[0]=='<'?"struct ":"")+str.word(2)+spstr(" ");
          }

          spstr rc(str.word(1).replace("$","_"));
          if (dup) rc+=spstr().sprintf("[%d]",dup); else
          if (duptext.length()) {
            rc+="[";
            rc+=replaceops(duptext, ll, argv[1]);
            rc+="]";
          }
          rc+=";";

          while (dst[dst.Max()].length()+rc.length()<40) dst[dst.Max()]+=" ";
          dst[dst.Max()]+=rc;
        }
      }
    }

    if (cmpos>0&&!in_macro) {
      if (dstpos==dst.Max()) dst<<"";
      while (dst[dst.Max()].length()<cmpos) dst[dst.Max()]+=" ";
      if (dst[dst.Max()].lastchar()!=' ') dst[dst.Max()]+=" ";
      dst[dst.Max()]+=spstr("//")+cm.del(0);
    }

    nexttype.clear();
  }
  if (ppack) dst<<""<<spstr().sprintf("#pragma pack()",ppack)<<"";

  spstr dstname(argc>2?argv[2]:strChangeFileExt(argv[1],"h")());
  if (!dst.SaveToFile(dstname())) {
    printf("Unable to save file \"%s\"\n",dstname());
    return 3;
  }
  return 0;
}
