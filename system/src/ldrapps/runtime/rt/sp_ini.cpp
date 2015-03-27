#include "sp_ini.h"
#include "minmax.h"
#ifndef NO_INIFILESRW
#ifdef USE_SPFILEIO_FOR_STRINGS
#include "sp_fileio.h"
#endif
#endif
#include <stdlib.h>

TINIFile::TINIFile(const TStrings *from) {
  NoWrite=true;
  Changes=false;
  FName.clear();
  if (from) {
    File=*from;
    for (l jj,ii=0;ii<File.Count();ii++) { // collect sections
      if (File[ii].cpos('[')>=0) {
        File[ii].trim();
        if (File[ii][0]=='['&&(jj=File[ii].cpos(']'))>0)
          Sections.AddObject(spstr(File[ii],1,jj-1).upper(),ii);
      }
    }
  }
}

#ifndef NO_INIFILESRW
TINIFile::TINIFile(const char *FileName, Bool NoFileUpdate) {
  NoWrite=NoFileUpdate;
  Changes=false;
  FName=FileName;
  if (!File.LoadFromFile(FileName)&&!NoWrite) {
#ifdef USE_SPFILEIO_FOR_STRINGS
    d OutFile=fOpen(FileName,"wbdws+");
    if (OutFile) { fWrite(OutFile,(void*)("\xD\xA"),2); fClose(OutFile); }
#else
    File.SaveToFile(FileName);
#endif
    Changes=true;
  }
  for (l ii=0;ii<File.Count();ii++) { // collect sections
    l jj;
    if (File[ii].cpos('[')>=0) {
      File[ii].trim();
      if (File[ii][0]=='['&&(jj=File[ii].cpos(']'))>0)
        Sections.AddObject(spstr(File[ii],1,jj-1).upper(),ii);
    }
  }
}
#endif

TINIFile::~TINIFile() {
#ifndef NO_INIFILESRW
  if (!NoWrite&&Changes) File.SaveToFile(FName.c());
#endif
}

TStrings TINIFile::ReadSections() { return Sections; }

void TINIFile::ReadSections(TStrings &names) { names = Sections; }

void TINIFile::ReadSectionOrgNames(TStrings &names) {
  names.Clear();
  for (l ll=0,jj,ps;ll<Sections.Count();ll++) {
    ps = Sections.Objects(ll);
    jj = File[ps].cpos(']',1);
    names.AddObject(spstr(File[ps],1,jj-1),ps);
  }
}

TStrings TINIFile::ReadSectionOrgNames() {
  TStrings result;
  ReadSectionOrgNames(result);
  return result;
}

l TINIFile::ValueIndex(const spstr &Section,const spstr &Key) {
  l ii=SectionIndex(Section);
  if (ii>=0) {
    l jj=File.IndexOfName(Key,Sections.Objects(ii));
    if (jj>=0&&(Sections.Count()<=ii+1||jj<Sections.Objects(ii+1)))
      return jj; else return -1;
  } else return -1;
}

void TINIFile::WriteString(const spstr &Section,const spstr &Key,const spstr &Value) {
  l idx=ValueIndex(Section,Key);
  if (idx<0) {
    l si=SectionIndex(Section),spos;
    if (si<0) {
      spos=File.Add('['+spstr(Section)+']');
      si=Sections.AddObject(Section,spos);
      Sections[si].upper();
    } else {
      spos=Sections.Objects(si)+1;
      while ((File[spos][0]==';'||File[spos].pos("=")>=0)&&File[spos][0]!='['&&
         spos<=File.Max()) spos++;
      spos--;
    }
    File.Insert(spos+1,Key+'='+Value);
    for (l jj=si+1;jj<Sections.Count();jj++) Sections.Objects(jj)++;
  } else File[idx]=Key+'='+Value;
  Changes=true;
}

float TINIFile::ReadFloat(const spstr &Section,const spstr &Key,float Default) {
  l vi=ValueIndex(Section,Key);
  if (vi<0) return Default; else {
    spstr SS=ReadString(Section,Key);
    float  FF=Default;
    SS.trim();
    sscanf(SS.c(),"%f",&FF);
    return FF;
  }
}

TList TINIFile::ReadArray(const spstr &Section,const spstr &Key,const TList &Default) {
  spstr ss=ReadString(Section,Key,"");
  if (!ss) return Default; else {
    TList result;
    l wcnt=ss.words(" ,;-");
    result.IncCount(wcnt);
    for (l ii=1;ii<=wcnt;ii++) result[ii]=ss.word_Dword(ii," ,;-");
    return result;
  }
}

void TINIFile::WriteArray(const spstr &Section,const spstr &Key,TList &Value) {
  spstr ss;
  for (l ii=0;ii<Value.Count();ii++) ss+=spstr().sprintf(" %2.2x",Value[ii]);
  WriteString(Section,Key,ss);
}

void TINIFile::WriteArray(const spstr &Section,const spstr &Key,ptr Buf,d Size) {
  spstr ss;
  b* buf=(b*)Buf;
  for (d ii=0;ii<Size;ii++) ss+=spstr().sprintf(" %2.2x",*buf++);
  WriteString(Section,Key,ss);
}

void TINIFile::ReadSection(const spstr &Section,TStrings &text) {
  l si=SectionIndex(Section);
  text.Clear();
  if (si>=0) {
    l cnt=CountSection(Section);
    for (l jj=Sections.Objects(si)+1,kk=0;kk<cnt-1;kk++,jj++) text.Add(File[jj]);
  }
}

TStrings TINIFile::ReadSection(const spstr &Section) {
  TStrings result;
  ReadSection(Section,result);
  return result;
}

void TINIFile::WriteSection(const spstr &Section,const TStrings &Text,Bool Replace) {
  l si=SectionIndex(Section);
  if (si>=0&&Replace) {
    l cnt=CountSection(Section)-1,ii=0,
     tcnt=Text.Count(),delta=0,
     spos=Sections.Objects(si)+1,
      mve=min(cnt,tcnt);

    while (ii<mve) { File[spos+ii]=Text.Str.Value()[ii]; ii++; }
    tcnt-=mve; cnt-=mve;

    if (tcnt) {
      File.Insert(spos+ii,spstr(),delta=tcnt);
      for (int jj=ii;jj<tcnt+ii;jj++) File[spos+jj]=Text.Str.Value()[jj];
    } else {
      if (cnt) File.Delete(spos+ii,cnt);
      delta=-cnt;
    }
    if (delta!=0)
      for (++si;si<Sections.Count();si++) Sections.Objects(si)+=delta;
  } else {
    l spos=File.Add('['+Section+']');
    si=Sections.AddObject(Section,spos);
    Sections[si].upper();
    File.AddStrings(File.Count(),Text);
  }
}

TStrings TINIFile::ReadSectionKeys(const spstr &Section) {
  TStrings result;
  ReadSectionKeys(Section,result);
  return result;
}

void TINIFile::ReadSectionKeys(const spstr &Section,TStrings &keys) {
  l si=SectionIndex(Section);
  keys.Clear();
  if (si>=0) {
    l cnt=CountSection(Section);
    for (l jj=Sections.Objects(si)+1,kk=0;kk<cnt-1;kk++,jj++)
      if (File[jj][0]!=';'&&File[jj].pos("=")>=0) keys.Add(File.Name(jj));
  }
}

TStrings TINIFile::ReadSectionValues(const spstr &Section) {
  TStrings result;
  ReadSectionValues(Section,result);
  return result;
}

void TINIFile::ReadSectionValues(const spstr &Section,TStrings &values) {
  l si=SectionIndex(Section);
  values.Clear();
  if (si>=0) {
    l cnt=CountSection(Section);
    for (l jj=Sections.Objects(si)+1,kk=0;kk<cnt-1;kk++,jj++)
      if (File[jj][0]!=';'&&File[jj].pos("=")>=0) values.Add(File.Value(jj));
  }
}

void TINIFile::EraseSection(const spstr &Section) {
  l si=SectionIndex(Section);
  if (si>=0) {
    l count=CountSection(Section);
    File.Delete(Sections.Objects(si),count);
    for (l jj=si+1;jj<Sections.Count();jj++) Sections.Objects(jj)-=count;
    Sections.Delete(si);
  }
  Changes=true;
}

void TINIFile::DeleteKey(const spstr &Section,const spstr &Key) {
  l si=SectionIndex(Section),ii=ValueIndex(Section,Key);
  if (si>=0&&ii>=0) {
    File.Delete(ii);
    for (l jj=si+1;jj<Sections.Count();jj++) Sections.Objects(jj)--;
  }
  Changes=true;
}

void TINIFile::WriteFloat(const spstr &Section,const spstr &Key,float Value) {
  char buf[64];
  sprintf(buf,"%f",Value);
  WriteString(Section,Key,buf);
}

l TINIFile::SectionIndex(const spstr &Section) {
  spstr Tmp(Section);
  return Sections.IndexOf(Tmp.upper());
}

l TINIFile::CountSection(const spstr &Section) {
  l si=SectionIndex(Section);
  return si<0?0:((si>=Sections.Count()-1?File.Count()-1:Sections.Objects(si+1)-1)-
    Sections.Objects(si)+1);
}

spstr TINIFile::ReadString(const spstr &Section,const spstr &Key,const spstr Default) {
  l ii=ValueIndex(Section,Key);
  return ii>=0?File.Value(ii):Default;
}

l TINIFile::ReadInteger(const spstr &Section,const spstr &Key,l Default) {
  l ii=ValueIndex(Section,Key);
  return ii>=0?File.Value(ii).Int():Default;
}

void TINIFile::WriteInteger(const spstr &Section,const spstr &Key,l Value) {
  char buf[128];
  ITOA(Value,buf,10);
  WriteString(Section,Key,spstr(buf));
}

spstr TINIFile::ReadStr(const char *Section,const char *Key,const char *Default) {
  return ReadString(Section,Key,Default);
}

l TINIFile::ReadInt(const char *Section,const char *Key,l Default) {
  return ReadInteger(Section,Key,Default);
}
