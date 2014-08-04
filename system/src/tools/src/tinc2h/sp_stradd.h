#ifndef SP_STRADD_MACROSES
#define SP_STRADD_MACROSES

#include "sp_str.h"

// ext can be "ext" or ""
inline spstr strChangeFileExt(const spstr &src,const spstr &newext) {
  spstr dmp(src);
  int ps=dmp.crpos('.');
  if (dmp.crpos('/')>ps||dmp.crpos('\\')>ps) ps=-1;
  if (!newext) {
    if (ps>=0) dmp.del(ps,dmp.length()-ps);
    return dmp;
  }
  if (ps<0) ps=dmp.length();
  if (ps<dmp.length()) dmp.del(ps+1,65536);
  return dmp+=newext;
}

inline spstr strRemovePath(const spstr &src) {
  spstr dmp(src);
  int ps1=dmp.crpos('/'),
      ps2=dmp.crpos('\\');
  if (ps2>ps1) ps1=ps2;
  return ps1>=0?dmp(ps1+1,dmp.length()-ps1-1):dmp;
}

// get index of word, here position "ps" located
inline l strFindWordForPos(const spstr &src,int ps,const char *sep) {
  l wp=0,pos;
  do { pos=src.wordpos(++wp,sep); } while (pos>=0&&pos<ps);
  return wp;
}

inline Direction strStringToDirection(const spstr &src) {
  char First =TOUPPER(src()[0]),
       Second=TOUPPER(src()[1]);
  switch (First) {
    case 'E':return EAST;
    case 'W':return WEST;
    case 'C':return STOP;
    case 'N':return Second=='W'?NW:(Second=='E'?NE:NORTH);
    case 'S':return Second=='W'?SW:(Second=='E'?SE:SOUTH);
  }
  return STOP;
}


#endif // SP_STRADD_MACROSES
