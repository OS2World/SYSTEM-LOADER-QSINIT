//
//  Simple List and StringList implementation
//
//  (c) dixie/SP 1997-2013. Freeware.
//
#ifndef SP_CLASSES
#define SP_CLASSES

#include "sp_defs.h"
#include "sp_str.h"
#include <string.h>
#include <stdlib.h>
#ifndef SRT
#define USE_SPFILEIO_FOR_STRINGS
#endif
#if defined(SPMALLOC_GETTEXT)||defined(__IBMCPP_RN_ON__)&&defined(HRT)
#include "sp_mem.h"
#endif
#ifndef NO_HUGELIST
#include "hugelist.hpp"
#endif

#define ADDON_ALLOC_COUNT 1

template <class L>
class List {
protected:
  L* Values;
  l MaxIndex;
  l AllocOn;
  l IndexOfCache;
  void AdjustAllocOn(l Value);
  l CheckValid(l idx) {
#ifndef NO_SP_ASSERTS
    assert(idx>=0&&idx<=MaxIndex);
#endif
    return range(idx,0,MaxIndex);
  }
  l CheckValidEx(l idx) { return range(idx,0,MaxIndex+1); }
  void CheckCapacity(l New) { if (New>AllocOn) AdjustAllocOn((New+ADDON_ALLOC_COUNT)*2); }
  void CPY(const List<L> &FromList) {
    AllocOn=0; Values=NULL; MaxIndex=-1;
    CheckCapacity(FromList.Max()+1);
    MaxIndex=FromList.Max();
    for (l ii=0;ii<=MaxIndex;ii++) Values[ii]=FromList.Values[ii];
    IndexOfCache=FromList.IndexOfCache;
  }
public:
  List() { Init(); }
  List(const List<L> &FromList) { CPY(FromList); }
  ~List() { Clear(); }
#ifdef _DEBUG
  L& operator[](l idx) { return Values[CheckValid(idx)]; }
#else
  L& operator[](l idx) { return Values[idx]; }
#endif
  l Max() const { return MaxIndex; }
  l Count() const { return MaxIndex+1; }
  List<L>& operator=(const List<L> &F) { Clear(); CPY(F); return *this; }
  const L* Value() const { return Values; }
  void Compact() { AdjustAllocOn(MaxIndex+11); } // compact memory usage
//  void Pack();                    // remove all 0 elements and Compact()
  void Clear();
  void Exchange(l Idx1,l Idx2);
  void Insert(l Pos,L Value,l Repeat=1);
  void Insert(l Pos,const List<L> &FromList);
  void Delete(l Pos,l Count=1,Bool CleanUp=false);
  Bool Equal(const List<L>&) const;
  l Add(const L Value);
  void IncCount(l IncValue=1) {
    CheckCapacity(MaxIndex+1+IncValue);
    MaxIndex+=IncValue;
  }
  void SetCount(l Count) {
    CheckCapacity(Count);
    MaxIndex=Count-1;
    if (IndexOfCache>MaxIndex) IndexOfCache=-1;
  }
  l IndexOf(const L &Value,l StartFrom=0);

  /// please, don't use this - this only for Strings implementation
  l& GetIndexOfCache() { return IndexOfCache; }

  void Init() {
    AllocOn=0; Values=NULL; MaxIndex=-1; IndexOfCache=-1;
  } // constructor
  friend List<L>& operator<< (List<L>&LL,const L Value) {
    LL.Add(Value);
    return LL;
  }
#if defined(__IBMCPP_RN_ON__) && defined(HRT)
  void* operator new(size_t size) { return spmalloc(size); }
  void* operator new[](size_t size) { return spmalloc(size); }
  void  operator delete(void*ptr) { spfree(ptr); }
  void  operator delete[](void*ptr) { spfree(ptr); }
#endif
  Bool IsMember(const L& Value) { return IndexOf(Value)>=0; }
  friend Bool operator==(const List<L>&L1,const List<L>&L2) { return L1.Equal(L2); }
  friend Bool operator!=(const List<L>&L1,const List<L>&L2) { return !L1.Equal(L2); }
};

typedef char*    PChar;
typedef void*  Pointer;
typedef List<l>                TList;
typedef List<d>               TUList;
typedef List<b>               TArray;
typedef List<PChar>     TListOfPChar;
typedef List<Pointer> TListOfPointer;
typedef List<Pointer>       TPtrList;
typedef List<spstr>    TStringVector;

template <class L>
class Strings {
protected:
  typedef List<L> TListL;
public:
  TListL      LList;
  TStringVector Str;

  Strings() {}
  Strings<L>& operator=(const Strings<L> &F) { Str=F.Str; LList=F.LList; return *this; }
#if !defined(__WATCOMC__)&&!defined(__IBMCPP__)
  // Strings<another L> assigment
  template <class F> Strings(const F &From)
    { Str=From.Str; LList.SetCount(From.LList.Count()); }
#else
  // compilers with ugly template support
  operator TStringVector&() { return Str; }
  Strings(TStringVector &From)
    { Str=From; LList.SetCount(From.Count()); }
  Strings<L>& operator=(const TStringVector&From)
    { Str=From; LList.SetCount(From.Count()); return *this; }
#endif
  l Add(const spstr &s) {LList.IncCount(); return Str.Add(s);}
  l AddObject(const spstr &s,L value) {LList.Add(value); return Str.Add(s);}

  l AddReplace(const spstr &s) {
    l idx=IndexOf(s);
    if (idx>=0) Str[idx]=s;
    return idx<0?Add(s):idx;
  }

  void Delete(l Pos,l Count=1,Bool CleanUp=false) {
    LList.Delete(Pos,Count,CleanUp);
    Str.Delete(Pos,Count,CleanUp);
  }
  void Exchange(l Idx1,l Idx2) { LList.Exchange(Idx1,Idx2); Str.Exchange(Idx1,Idx2);}

  /// name by index
  spstr Name(l idx);
  /// name by value
  spstr Name(const spstr&);
  /// value by name
  spstr Value(const char*);
  /// value by name
  spstr Value(const spstr&);
  /// int value by name
  int   IntValue(const char*);
  /// dword value by name
  d     DwordValue(const char*);
  /// value by index (string number)
  spstr Value(l);

  spstr StrValue(l idx) const { return spstr(Str.Value()[idx]); }


  spstr& operator[](l idx) { return Str[idx]; }
  L& Objects(l idx) {return LList[idx]; }
  l Max() const {return Str.Max();}
  l Count() const {return Str.Count();}
//  void Compact() {LList.Compact(); Str.Compact();}
  void Clear() {LList.Clear(); Str.Clear();}
  void Sort(Bool Forward=true);
  void SortICase(Bool Forward=true);
#ifdef SORT_BY_OBJECTS_ON
  void SortByObjects(Bool Forward=true);
#endif
  l TrimEmptyLines(Bool EndOnly=false);
  void TrimAllLines(l Pos=0);

  l SplitString(const spstr &ss,const char *separators=" \x9");
  l ParseCmdLine(const spstr &ss,char separator=' ');

  void Insert(l Pos,spstr Value,l Repeat=1);
  void InsertObject(l Pos,spstr Value1,L Value2,l Repeat=1) {
    Str.Insert(Pos,Value1,Repeat);
    LList.Insert(Pos,Value2,Repeat);
  }

  void AddStrings(l Pos,const Strings<L> &FromStrings) {
    LList.Insert(Pos,FromStrings.LList);
    Str.Insert(Pos,FromStrings.Str);
  }
#ifdef OLD_STRINGS_INDEXOF
  l IndexOf(const spstr &Value,l StartFrom=0) {return Str.IndexOf(Value,StartFrom);}
#else
  l IndexOf(const spstr &Value,l StartFrom=0);
#endif
  l IndexOfICase(const char *SName,l StartFrom=0);  // ignore case
  l IndexOfName(const char *SName,l StartFrom=0);
  l IndexOfName(const spstr &Name,l StartFrom=0);   // ignore case
  l IndexOfObject(const L Value,l StartFrom=0) { return LList.IndexOf(Value,StartFrom); }
  Bool Equals(const Strings<L> &St) { return Str.Equal(St.Str)&&LList.Equal(St.LList); }
  void Move(l FromIdx,l ToIdx) {
    Str[ToIdx] =Str[FromIdx];
    LList[ToIdx]=LList[FromIdx];
  }
  char *GetText(const char *eol="\x0D\x0A", l start=0, l count=-1) const;
  spstr GetTextToStr(const char *eol="\x0D\x0A", l start=0, l count=-1) const;
  /// merge command line (add quotes around lines with spaces)
  spstr MergeCmdLine(const char *separator=" ", char quote='\"') const;

  /// accept both UNIX & normal text
  void SetText(const char*,l len=0);
  /// special CSV multiline support. \r act as soft-CR and ignored, \n\r only used as CR.
  void SetCSVText(const char*,l len=0);

  spstr MergeBackSlash(l FromPos=0,l *LastPos=0,const char *separator=0) const;
  l Compact(const char *FirstChar=";",Bool Empty=true);

  friend Strings<L>& operator<< (Strings<L>&LL,const spstr Value) {
    LL.Add(Value);
    return LL;
  }
  friend Strings<L>& operator<< (Strings<L>&LL,const char *Value) {
    LL.Add(spstr(Value));
    return LL;
  }
#ifndef NO_LOADSAVESTRINGS
  Bool LoadFromFile(const char*,Bool is_csv=false);
  Bool SaveToFile(const char*);
#endif

  Bool IsMember(const spstr str) { return Str.IndexOf(str)>=0; }
  Bool IsObjectMember(const L& Value) { return LList.IndexOf(Value)>=0; }

  void Init() { LList.Init(); Str.Init(); }
};

#define CalcSizeInDD(ll) ((ll)/32+((ll)&0x1F?1:0))

template <class L>
class Set {
protected:
  d set[CalcSizeInDD(sizeof(L))];
public:
  Set() { Clear(); }
  Set(d v0) {Clear();Include(v0);}
  Set(d v0,d v1) {Clear();Include(v0);Include(v1);}
  Set(d v0,d v1,d v2) {Clear();Include(v0);Include(v1);Include(v2);}
  Set(d v0,d v1,d v2,d v3) { Clear();Include(v0);Include(v1);Include(v2);Include(v3);}
  Set(d v0,d v1,d v2,d v3,d v4) {Clear();Include(v0);Include(v1);Include(v2);Include(v3);Include(v4);}
  Set(d v0,d v1,d v2,d v3,d v4,d v5) {Clear();Include(v0);Include(v1);Include(v2);Include(v3);Include(v4);Include(v5);}
  Set(d v0,d v1,d v2,d v3,d v4,d v5,d v6) {Clear();Include(v0);Include(v1);Include(v2);Include(v3);Include(v4);Include(v5);Include(v6);}
  Set(d v0,d v1,d v2,d v3,d v4,d v5,d v6,d v7) {Clear();Include(v0);Include(v1);Include(v2);Include(v3);Include(v4);Include(v5);Include(v6);Include(v7);}
  Set(d v0,d v1,d v2,d v3,d v4,d v5,d v6,d v7,d v8) {Clear();Include(v0);Include(v1);Include(v2);Include(v3);Include(v4);Include(v5);Include(v6);Include(v7);Include(v8);}
  Set(d v0,d v1,d v2,d v3,d v4,d v5,d v6,d v7,d v8,d v9) {Clear();Include(v0);Include(v1);Include(v2);Include(v3);Include(v4);Include(v5);Include(v6);Include(v7);Include(v8);Include(v9);}
  Set(d v0,d v1,d v2,d v3,d v4,d v5,d v6,d v7,d v8,d v9,d vA) {Clear();Include(v0);Include(v1);Include(v2);Include(v3);Include(v4);Include(v5);Include(v6);Include(v7);Include(v8);Include(v9);Include(vA);}
  Set(d v0,d v1,d v2,d v3,d v4,d v5,d v6,d v7,d v8,d v9,d vA,d vB) {Clear();Include(v0);Include(v1);Include(v2);Include(v3);Include(v4);Include(v5);Include(v6);Include(v7);Include(v8);Include(v9);Include(vA);Include(vB);}
  Set(d v0,d v1,d v2,d v3,d v4,d v5,d v6,d v7,d v8,d v9,d vA,d vB,d vC) {Clear();Include(v0);Include(v1);Include(v2);Include(v3);Include(v4);Include(v5);Include(v6);Include(v7);Include(v8);Include(v9);Include(vA);Include(vB);Include(vC);}
  Set(d v0,d v1,d v2,d v3,d v4,d v5,d v6,d v7,d v8,d v9,d vA,d vB,d vC,d vD) {Clear();Include(v0);Include(v1);Include(v2);Include(v3);Include(v4);Include(v5);Include(v6);Include(v7);Include(v8);Include(v9);Include(vA);Include(vB);Include(vC);Include(vD);}
  Set(d v0,d v1,d v2,d v3,d v4,d v5,d v6,d v7,d v8,d v9,d vA,d vB,d vC,d vD,d vE) {Clear();Include(v0);Include(v1);Include(v2);Include(v3);Include(v4);Include(v5);Include(v6);Include(v7);Include(v8);Include(v9);Include(vA);Include(vB);Include(vC);Include(vD);Include(vE);}
  Set(d v0,d v1,d v2,d v3,d v4,d v5,d v6,d v7,d v8,d v9,d vA,d vB,d vC,d vD,d vE,d vF) {Clear();Include(v0);Include(v1);Include(v2);Include(v3);Include(v4);Include(v5);Include(v6);Include(v7);Include(v8);Include(v9);Include(vA);Include(vB);Include(vC);Include(vD);Include(vE);Include(vF);}

  Set(const Set<L> &st) { memcpy(&set,&st,sizeof(set)); }

  void CopyData(const ptr buf) { memcpy(&set,buf,sizeof(set)); }
  void GetData(ptr buf) { memcpy(buf,&set,sizeof(set)); }
  l    GetSizeOfData() { return sizeof(set); }

  void CopyData(ptr buf,d Count) { memcpy(&set,buf,Round8(Count)>>3); }
  void GetData(ptr buf,d Count) { memcpy(buf,&set,Round8(Count)>>3); }

  d High() { return sizeof(L)-1; }
  d Low() { return 0; }

  Set<L>& operator= (Set<L> &st) { memcpy(&set,&st.set,sizeof(set)); return *this; }

  Bool operator[] (d idx) const {
#ifndef NO_SP_ASSERTS
    assert(idx<sizeof(L));
#endif
    return (set[idx>>5]&1<<(idx&0x1F))!=0;
  }
  friend Bool operator== (Set<L> const&ss1,Set<L> const&ss2) {
    return memcmp(&ss1.set,&ss2.set,sizeof(ss1.set))==0;
  }
  friend Bool operator!= (Set<L> const&ss1,Set<L> const&ss2) {
    return memcmp(&ss1.set,&ss2.set,sizeof(ss1.set))!=0;
  }
  friend Set<L> operator+ (Set<L> const&ss1,Set<L> const&ss2) {
    Set<L> sr(ss1);
    for (l ll=0;ll<sizeof(sr.set)/sizeof(d);ll++) sr.set[ll]|=ss2.set[ll];
    return sr;
  }
  friend Set<L> operator- (Set<L> const&ss1,Set<L> const&ss2) {
    Set<L> sr(ss1);
    for (l ll=0;ll<sizeof(sr.set)/sizeof(d);ll++) sr.set[ll]&=~ss2.set[ll];
    return sr;
  }
  friend Set<L> operator* (Set<L> const&ss1,Set<L> const&ss2) {
    Set<L> sr(ss1);
    for (l ll=0;ll<sizeof(sr.set)/sizeof(d);ll++) sr.set[ll]&=ss2.set[ll];
    return sr;
  }
#ifndef __BCPLUSPLUS__
  friend Set<L> operator+ (Set<L> const&ss,d Value)
    { Set<L> sr(ss); sr.Include(Value); return sr; }
  friend Set<L> operator- (Set<L> const&ss,d Value)
    { Set<L> sr(ss); sr.Exclude(Value); return sr; }
#endif
  Set<L>& operator+= (d Value) { Include(Value); return *this; }
  Set<L>& operator-= (d Value) { Exclude(Value); return *this; }
  Set<L>& operator+= (Set<L> const&ss) {
    for (l ll=0;ll<sizeof(set)/sizeof(d);ll++) set[ll]|=ss.set[ll];
    return *this;
  }
  Set<L>& operator-= (Set<L> const&ss) {
    for (l ll=0;ll<sizeof(set)/sizeof(d);ll++) set[ll]&=~ss.set[ll];
    return *this;
  }
  Set<L>& operator*= (Set<L> const&ss) {
    for (l ll=0;ll<sizeof(set)/sizeof(d);ll++) set[ll]&=ss.set[ll];
    return *this;
  }

  friend Set<L>& operator<< (Set<L>&LL,d Value) {
    LL.Include(Value);
    return LL;
  }
  friend Set<L>& operator>> (Set<L>&LL,d Value) {
    LL.Exclude(Value);
    return LL;
  }

  void Include(d idx) {
#ifndef NO_SP_ASSERTS
    assert(idx<sizeof(L));
#endif
    set[idx>>5]|=1<<(idx&0x1F);
  }
  void Exclude(d idx) {
#ifndef NO_SP_ASSERTS
    assert(idx<sizeof(L));
#endif
    set[idx>>5]&=~(1<<(idx&0x1F));
  }
  void Change(Bool Add,d idx) {
#ifndef NO_SP_ASSERTS
    assert(idx<sizeof(L));
#endif
    Add?(set[idx>>5]|=1<<(idx&0x1F)):(set[idx>>5]&=~(1<<(idx&0x1F)));
  }

  Set<L>&Include(d From,d To) {
    for (d ll=From;ll<=To;ll++) Include(ll);
    return *this;
  }
  Set<L>&Exclude(d From,d To) {
    for (d ll=From;ll<=To;ll++) Exclude(ll);
    return *this;
  }

  void Clear() { memset(&set,0,sizeof(set)); }
  Bool IsEmpty() {
    for (l ll=0;ll<sizeof(set)/sizeof(d);ll++)
      if (set[ll]!=0) return false;
    return true;
  }
  d FindLeft(d idx) const {
    if (!idx) return FFFF;
    d val=--idx&0x1F?set[idx>>5]<<(31-(idx&0x1F)):0;
    while (idx) {
      if ((idx&0x1F)==0x1F) val=set[idx>>5];
      if ((val&0x80000000)!=0) return idx; else { val=val<<1; idx--; }
    }
    return FFFF;
  }
  d FindLeft0(d idx) const {
    if (!idx) return FFFF;
    d val=--idx&0x1F?set[idx>>5]<<(31-(idx&0x1F)):0;
    while (idx) {
      if ((idx&0x1F)==0x1F) val=set[idx>>5];
      if ((val&0x80000000)==0) return idx; else { val=val<<1; idx--; }
    }
    return FFFF;
  }
  d FindRight(d idx) const {
    d val=++idx&0x1F?set[idx>>5]>>(idx&0x1F):0;
    while (idx<sizeof(L)) {
      if (!(idx&0x1F)) val=set[idx>>5];
      if ((val&1)!=0) return idx; else { val=val>>1; idx++; }
    }
    return FFFF;
  }
  d FindRight0(d idx) const {
    d val=++idx&0x1F?set[idx>>5]>>(idx&0x1F):0;
    while (idx<sizeof(L)) {
      if (!(idx&0x1F)) val=set[idx>>5];
      if ((val&1)==0) return idx; else { val=val>>1; idx++; }
    }
    return FFFF;
  }

  operator spstr() const {
    spstr ss;
    d idx=0;
    while (true) {
      idx=FindRight(idx);
      if (idx==FFFF) break;
      ss+=spstr(int(idx))+',';
    }
    if (ss.length()) ss.dellast();
    return ss;
  }
};

typedef b Array64   [   64];
typedef b Array128  [  128];
typedef b Array256  [  256];
typedef b Array512  [  512];
typedef b Array1024 [ 1024];
typedef b Array2048 [ 2048];
typedef b Array4096 [ 4096];
typedef b Array8192 [ 8192];
typedef b Array32768[32768];
typedef b Array65536[65536];

typedef Set<Array64>     set64;
typedef Set<Array128>   set128;
typedef Set<Array256>      set;
typedef Set<Array256>   set256;
typedef Set<Array512>   set512;
typedef Set<Array1024>   set1k;
typedef Set<Array1024> set1024;
typedef Set<Array2048>   set2k;
typedef Set<Array2048> set2048;
typedef Set<Array4096>   set4k;
typedef Set<Array4096> set4096;
typedef Set<Array8192>   set8k;
typedef Set<Array8192> set8192;
typedef Set<Array32768> set32k;
typedef Set<Array65536>   setw;

/*********************************************************************

                     (T)List implementation HERE!

*********************************************************************/

template <class L>
void List<L>::AdjustAllocOn(l Value) {
  L *NewValues=(L*)new L[Value];
  for (l ll=0;ll<=MaxIndex;ll++) NewValues[ll]=Values[ll];
  if (Values) delete[]Values;
  AllocOn=Value;
  Values =NewValues;
}

template <class L>
void List<L>::Clear() {
  if (AllocOn) {
    if (Values) delete[]Values;
    AllocOn=0;
  }
  MaxIndex=-1;
  IndexOfCache=-1;
  Values  =NULL;
}

/*
template <class L>
void List<L>::Pack() {
  l ii,jj;
  for (ii=0,jj=0;ii<=MaxIndex;ii++)
    if (l(Values[ii])) {
      if (ii!=jj) Values[ii]=Values[jj]; jj++;
    }
  MaxIndex    =jj-1;
  IndexOfCache=-1;
  Compact();
}
*/

template <class L>
Bool List<L>::Equal(const List<L> &Lst) const {
  if (MaxIndex==Lst.MaxIndex) {
    for (l ii=0;ii<=MaxIndex;ii++)
      if (Values[ii]!=Lst.Values[ii]) return 0;
    return 1;
  }
  return 0;
}

template <class L>
void List<L>::Insert(l Pos,L Value,l Repeat) {
  Pos=CheckValidEx(Pos);
  CheckCapacity(MaxIndex+Repeat+1);
  if (Pos<=MaxIndex)
    for (l ii=MaxIndex-Pos;ii>=0;ii--)
      Values[Pos+Repeat+ii]=Values[Pos+ii];
  for (l ii=0;ii<Repeat;ii++) Values[Pos+ii]=Value;
  if (IndexOfCache>=Pos) IndexOfCache=-1;
  MaxIndex+=Repeat;
}

template <class L>
void List<L>::Insert(l Pos,const List<L> &FromList) {
  l Cnt=FromList.Count(),ii;
  Pos=CheckValidEx(Pos);
  CheckCapacity(MaxIndex+1+Cnt);
  if (Pos<=MaxIndex)
    for (ii=MaxIndex-Pos;ii>=0;ii--)
      Values[Pos+Cnt+ii]=Values[Pos+ii];
  for (ii=0;ii<Cnt;ii++) Values[Pos+ii]=FromList.Values[ii];
  if (IndexOfCache>=Pos) IndexOfCache=-1;
  MaxIndex+=Cnt;
}

template <class L>
void List<L>::Delete(l Pos,l Count,Bool CleanUp) {
  if (Pos>=0&&Pos<=MaxIndex) {
    if (Pos+Count>MaxIndex) MaxIndex=Pos-1; else {
      for (l ii=0;ii<=MaxIndex-Pos-Count;ii++)
        Values[Pos+ii]=Values[Pos+Count+ii]; // delete (exam. for strings)
      MaxIndex-=Count;
      if (CleanUp) AdjustAllocOn(MaxIndex+1+ADDON_ALLOC_COUNT);
    }
    if (IndexOfCache>=Pos) IndexOfCache=-1;
  }
}

template <class L>
l List<L>::Add(const L Value) {
  CheckCapacity(MaxIndex+2);
  Values[++MaxIndex]=Value;
  return MaxIndex;
}

template <class L>
l List<L>::IndexOf(const L &Value,l StartFrom) {
  if (IndexOfCache>=StartFrom)
    if (Value==Values[IndexOfCache]) return IndexOfCache;
  for (l ii=StartFrom;ii<=MaxIndex;ii++) {
    if (Values[ii]==Value) {
      IndexOfCache=StartFrom?-1:ii;
      return ii;
    }
  }
  return -1;
}

template <class L>
void List<L>::Exchange(l Idx1,l Idx2) {
  Idx1=CheckValid(Idx1);
  Idx2=CheckValid(Idx2);
  if (Idx1!=Idx2) {
    L swp=Values[Idx1];
    Values[Idx1]=Values[Idx2];
    Values[Idx2]=swp;
    if (Idx1==IndexOfCache) IndexOfCache=Idx2>Idx1?-1:Idx2; else
      if (Idx2==IndexOfCache) IndexOfCache=Idx1>Idx2?-1:Idx1;
  }
}


/*********************************************************************

                     (T)Strings implementation HERE!

*********************************************************************/

template <class L>
void Strings<L>::Insert(l Pos,spstr Value,l Repeat) {
  l mx=LList.Count();
  LList.IncCount(Repeat);
  for (l ii=0;ii<Repeat;ii++) LList.Exchange(Pos+ii,mx+ii);
  Str.Insert(Pos,Value,Repeat);
}

template <class L>
spstr Strings<L>::GetTextToStr(const char *eol, l start, l count) const {
  l len=0,ii,inc=strlen(eol);
  if (start<0) start=0;
  if (count<=0) count=Str.Count();
  count+=start;
  if (count>Str.Count()) count=Str.Count();
  for (ii=start;ii<count;ii++) len+=Str.Value()[ii].length()+inc;
  spstr result;
  char *str,*tmp=result.LockPtr(len+1);
  for (ii=start,str=tmp;ii<count;ii++) {
    memcpy(str,Str.Value()[ii](),Str.Value()[ii].length());
    str+=Str.Value()[ii].length();
    memcpy(str,eol,inc); str+=inc;
  }
  result.UnlockPtr(str-tmp);
  return result;
}

template <class L>
spstr Strings<L>::MergeCmdLine(const char *separator, char quote) const {
  l len=0,ii,inc=strlen(separator);
  for (ii=0;ii<Str.Count();ii++)
    len+=Str.Value()[ii].length()+inc+2+Str.Value()[ii].countchar('\"');
  spstr result;
  char *str,*tmp=result.LockPtr(len+1);
  for (ii=0,str=tmp;ii<Str.Count();ii++) {
    spstr qrep(Str.Value()[ii]);
    Bool nq=qrep.cpos(' ')>=0;
    qrep.replace("\"","\\\"");
    if (nq) *str++=quote;
    memcpy(str,qrep(),qrep.length());
    str+=qrep.length();
    if (nq) *str++=quote;
    memcpy(str,separator,inc); str+=inc;
  }
  result.UnlockPtr(str-tmp);
  return result;
}

template <class L>
char *Strings<L>::GetText(const char *eol, l start, l count) const {
  l len=0,ii,inc=strlen(eol);
  if (start<0) start=0;
  if (count<=0) count=Str.Count();
  count+=start;
  if (count>Str.Count()) count=Str.Count();
  for (ii=start;ii<count;ii++) len+=Str.Value()[ii].length()+inc;
#ifdef SPMALLOC_GETTEXT
  char *str,*tmp=(char*)spmalloc(len+1);
#else
  char *str,*tmp=(char*)MALLOC(len+1);
#endif
  for (ii=start,str=tmp;ii<count;ii++) {
    memcpy(str,Str.Value()[ii](),Str.Value()[ii].length());
    str+=Str.Value()[ii].length();
    memcpy(str,eol,inc); str+=inc;
  }
  *str=0;
  return tmp;
}

template <class L>
void Strings<L>::SetText(const char *Text,l len) {
  if (len<=0) len=strlen(Text);
  const char *txt;
  Clear();
  while (len) {
    txt=Text;
    while (len&&*txt!=0x0A&&*txt!=0x0D&&*txt!=0x0C) { len--; txt++; }
    Add((txt==Text?spstr():spstr(Text,txt-Text))());
    if (len&&*txt==0x0D&&*(txt+1)==0x0A) { len--; txt++; }
    if (!len) break;
    len--; Text=++txt;
  }
}

template <class L>
void Strings<L>::SetCSVText(const char*Text,l len) {
  if (len<=0) len=strlen(Text);
  const char *txt;
  Clear();
  while (len) {
    txt=Text;
    while (len&&*txt!=0x0D&&*txt!=0x0C) { len--; txt++; }
    Add((txt==Text?spstr():spstr(Text,txt-Text))());
    Str.Value()[Str.Max()];
    if (len&&*txt==0x0D&&*(txt+1)==0x0A) { len--; txt++; }
    if (!len) break;
    len--; Text=++txt;
  }
}

template <class L>
l Strings<L>::IndexOfName(const char *SName,l StartFrom) {
  for (l ii=StartFrom;ii<=Str.Max();ii++)
    if (stricmp(SName,Name(ii)())==0) return ii;
  return -1;
}

template <class L>
l Strings<L>::IndexOfICase(const char *SName,l StartFrom) {
  for (l ii=StartFrom;ii<=Str.Max();ii++)
    if (stricmp(SName,Str[ii]())==0) return ii;
  return -1;
}

template <class L>
l Strings<L>::IndexOfName(const spstr &SName,l StartFrom) {
  spstr sName=SName;
  sName.upper();
  for (l ii=StartFrom;ii<=Str.Max();ii++)
    if (Name(ii).upper()==sName) return ii;
  return -1;
}

template <class L>
void Strings<L>::Sort(Bool Forward) {
  if (Str.Count()>1) {
    l iCnt=Str.Count(),iOffset=iCnt/2;
    while (iOffset) {
      l iLimit=iCnt-iOffset, iSwitch;
      do {
        iSwitch=false;
        for(l iRow=0;iRow<iLimit;iRow++) {
          if (strcmp(Str.Value()[iRow](),Str.Value()[iRow+iOffset]())>0) {
            Exchange(iRow,iRow+iOffset);
            iSwitch=iRow;
          }
        }
      } while (iSwitch);
      iOffset=iOffset/2;
    }
  }
}

template <class L>
void Strings<L>::SortICase(Bool Forward) {
  if (Str.Count()>1) {
    l iCnt=Str.Count(),iOffset=iCnt/2;
    while (iOffset) {
      l iLimit=iCnt-iOffset, iSwitch;
      do {
        iSwitch=false;
        for(l iRow=0;iRow<iLimit;iRow++) {
          if (stricmp(Str.Value()[iRow](),Str.Value()[iRow+iOffset]())>0) {
            Exchange(iRow,iRow+iOffset);
            iSwitch=iRow;
          }
        }
      } while (iSwitch);
      iOffset=iOffset/2;
    }
  }
}

#ifdef SORT_BY_OBJECTS_ON
template <class L>
void Strings<L>::SortByObjects(Bool Forward) {
  if (LList.Count()>1) {
    l iCnt=LList.Count(),iOffset=iCnt/2;
    while (iOffset) {
      l iLimit=iCnt-iOffset, iSwitch;
      do {
        iSwitch=false;
        for(l iRow=0;iRow<iLimit;iRow++) {
          if (Forward) {
            if (LList.Value()[iRow]>LList.Value()[iRow+iOffset]) { Exchange(iRow,iRow+iOffset); iSwitch=iRow; }
          } else {
            if (LList.Value()[iRow]<LList.Value()[iRow+iOffset]) { Exchange(iRow,iRow+iOffset); iSwitch=iRow; }
          }
        }
      } while (iSwitch);
      iOffset=iOffset/2;
    }
  }
}
#endif

template <class L>
l Strings<L>::TrimEmptyLines(Bool EndOnly) {
  l delcnt=0;
  if (EndOnly) {
    l ii=Str.Max();
    while (ii>=0) {
      spstr st(Str[ii]);
      if (st.trim().length()==0) {
        Str.Delete(ii);
        LList.Delete(ii--);
        delcnt++;
      } else break;
    }
  } else {
    l ii=0;
    while (ii<=Str.Max()) {
      spstr st(Str[ii]);
      if (st.trim().length()==0) {
        Str.Delete(ii);
        LList.Delete(ii);
        delcnt++;
      } else ii++;
    }
  }
  // return number of deleted lines
  return delcnt;
}

template <class L>
void Strings<L>::TrimAllLines(l StartFrom) {
  for (l ii=StartFrom;ii<=Str.Max();ii++) Str[ii].trim();
  if (Str.GetIndexOfCache()>=StartFrom) Str.GetIndexOfCache()=-1;
}

template <class L>
l Strings<L>::SplitString(const spstr &ss, const char *separators) {
  Clear();
  const char *cc=ss();
  if (!cc||!*cc) return 0;
  char *prev,*str=(char*)cc;
  // skip space separators at start...
  Bool Zero=(*(w*)separators==0x0020||*(w*)separators==0x0009||
    *(w*)separators==0x0920||*(w*)separators==0x2009);
  if (Zero) while (*str&&strchr(separators,*str)) str++;
  if (*str) {
    str--;
    if (separators[1]==0)
    do {
      prev=++str;
      str=strchr(prev,*separators);
      spstr toadd;
      if (str!=prev) toadd=spstr(prev,str?str-prev:0);
      Str.Add(toadd);
      if (str&&Zero) while (strchr(str+1,*separators)==str+1) str++; // skip same chars
    } while (str);
      else
    do {
      prev=++str;
      str=strpbrk(prev,separators);
      spstr toadd;
      if (str!=prev) toadd=spstr(prev,str?str-prev:0);
      Str.Add(toadd);
      if (str&&Zero) while (strpbrk(str+1,separators)==str+1) str++; // skip same chars
    } while (str);
  } else
  if (Zero) Str.Add(spstr());
  if (Str.Count()) LList.IncCount(Str.Count());
  return Str.Count();
}

template <class L>
l Strings<L>::ParseCmdLine(const spstr &ss,char separator) {
  const char *pp=ss();
  spstr tmp;
  char *al=tmp.LockPtr(ss.length()+260), *cl=al;
  int qout=0,argc=0;
  Clear();
  while (true) {
    if (*pp) while (*pp==separator||separator==' '&&*pp==9) pp++;
    if (*pp==0) break;
    argc++;
    while (true) {
      int copy    =1;
      int numslash=0;
      while (*pp=='\\') { ++pp; ++numslash; }
      if (*pp=='\"') {
        if ((numslash&1)==0) {
          if (qout) {
            if (pp[1]=='\"') pp++; else copy=0;
          } else copy=0;
          qout=!qout;
        }
        numslash>>=1;               /* divide numslash by two */
      }
      while (numslash--) *al++='\\';
      if (*pp==0||(!qout&&(*pp==separator||separator==' '&&*pp==9))) break;
      if (copy) *al++=*pp;
      ++pp;
    }
    *al=0;
    Add(spstr(cl));
    al=cl;
  }
  tmp.UnlockPtr();
  return argc;
}

template <class L>
spstr Strings<L>::Name(l idx) {
  const char *str=Str[idx]();
  char *ps=strchr((char *)str,'=');
  if (ps) {
    int len=ps-str,st=0;
    while (st<len&&(str[st]==' '||str[st]==9)) { st++; len--; }
    while (st<len&&(str[len-1]==' '||str[len-1]==9)) len--;
    if (len) return spstr(str+st,len);
  }
  return spstr(Str[idx]);
}

template <class L>
spstr Strings<L>::Name(const spstr &SValue) {
  for (l ii=0;ii<=Str.Max();ii++) {
    if (Value(ii)==SValue) return Name(ii);
  }
  return spstr();
}

template <class L>
spstr Strings<L>::Value(const spstr &SName) {
  l idx=IndexOfName(SName);
  return idx>=0?Value(idx):spstr();
}

template <class L>
spstr Strings<L>::Value(const char *SName) {
  l idx=IndexOfName(SName);
  return idx>=0?Value(idx):spstr();
}

template <class L>
int Strings<L>::IntValue(const char *SName) {
  l idx=IndexOfName(SName);
  if (idx<0) return 0;
  const char *str=Str[idx]();
  char *ps=strchr((char *)str,'=');
  if (ps++) {
    while (*ps&&*ps==' ') ps++;
    while (*ps=='0'&&*(ps+1)!='x'&&*(ps+1)!='X') ps++;
    return strtol(ps,0,0);
  } else return 0;
}

template <class L>
d Strings<L>::DwordValue(const char *SName) {
  l idx=IndexOfName(SName);
  if (idx<0) return 0;
  const char *str=Str[idx]();
  char *ps=strchr((char *)str,'=');
  if (ps++) {
    while (*ps&&*ps==' ') ps++;
    while (*ps=='0'&&*(ps+1)!='x'&&*(ps+1)!='X') ps++;
    return strtoul(ps,0,0);
  } else return 0;
}

template <class L>
spstr Strings<L>::Value(l idx) {
  const char *str=Str[idx]();
  char *ps=strchr((char*)str,'=');
  if (ps) return spstr(ps+1); else return spstr();
}

template <class L>
spstr Strings<L>::MergeBackSlash(l FromPos,l *LastPos,const char *separator) const {
  if (FromPos>Str.Max()) return spstr();
  l len=0,ii=FromPos,ok=0,
    seplen=separator?strlen(separator):0;
  while (Str.Value()[ii].lastchar()=='\\'&&ii<Str.Count())
    len+=Str.Value()[ii++].length()-1+seplen;
  len+=Str.Value()[ii].length()+seplen;
  spstr res;
  char *cc=res.LockPtr(len),*csave=cc;
  ii=FromPos;
  do {
    const spstr *str=&Str.Value()[ii++];
    l first=0,last=str->length()-1;
    while ((*str)()[first]==' ') first++;
    while ((*str)()[last]==' '&&last) last--;
    if ((*str)()[last]!='\\') ok=1; else
      if (--last>0)
        while ((*str)()[last]==' '&&last) last--;
    if (last>=first) {
      l cnt=last-first+1;
      memcpy(cc,(*str)()+first,cnt);
      cc+=cnt;
      if (!ok&&separator) {
        memcpy(cc,separator,seplen);
        cc+=seplen;
      }
    }
  } while (!ok);
  if (LastPos) *LastPos=ii-1;
//  *cc=0;
  res.UnlockPtr(cc-csave);
  return res;
}

template <class L>
l Strings<L>::Compact(const char *FirstChar,Bool Empty) {
  l ii=0,rc=Str.Count(),st=-1,ok=0,sch=FirstChar&&*FirstChar;
  while (Str.Count()>ii) {
    const char *str=Str[ii]();
    if (!str) ok=Empty; else
    if (*str==' '||*str=='\x9') {
      int ps=1;
      while (str[ps]&&(str[ps]==' '||str[ps]=='\x9')) ps++;
      ok=str[ps]==0;
    } else
    if (sch)
      if (strchr(FirstChar,*(b*)str)) ok=1;
    if (ok&&st<0) st=ii; else
    if (!ok&&st>=0) {
      int cnt=ii-st;
      Str.Delete(st,cnt);
      LList.Delete(st,cnt);
      ii-=cnt;
    }
    ii++;
  }
  return rc-Str.Count();
}

/*
template <class L>
istream& operator>> (istream &istr,Strings<L> &S) {
  spstr tmp;
  S.Clear();
  while (!istr.eof()) {
    istr>>tmp;
    S.Add(tmp);
  }
  return istr;
}

template <class L>
ostream& operator<< (ostream &ostr,Strings<L> &S) {
  l smax=S.Max();
  while (smax>=0&&S[smax]=="") smax--;
  for (l ii=0;ii<=smax;ii++) ostr<<S[ii]<<endl;
  return ostr;
}*/

#ifndef OLD_STRINGS_INDEXOF
template <class L>
l Strings<L>::IndexOf(const spstr &Value,l StartFrom) {
  l idx=Str.GetIndexOfCache();
  if (idx>=StartFrom)
    if (Value.hash()==Str[idx].hash())
      if (Value==Str[idx]) return idx;
  for (l ii=StartFrom;ii<=Str.Max();ii++)
    if (Str[ii].hash()==Value.hash())
      if (Str[ii]==Value) {
        Str.GetIndexOfCache()=StartFrom?-1:ii;
        return ii;
      }
  return -1;
}
#endif

#ifndef NO_LOADSAVESTRINGS

#if defined(__IBMCPP_RN_ON__)||defined(SP_LINUX)&&defined(HRT)||defined(USE_SPFILEIO_FOR_STRINGS) // IBM/Rn Load/Save
#include "sp_fileio.h" // (un)fortunately we hav'nt fopen here :)
template <class L>
Bool Strings<L>::LoadFromFile(const char *FName,Bool is_csv) {
  Clear();
  d rf=fOpen(FName,"rbdws+");
  if (rf) {
    d fsz=fGetSize(rf);
    char *cc=(char*)MALLOC(fsz+1);
    cc[fsz]=0;
    if (fRead(rf,cc,fsz)==fsz)
      if (is_csv) SetCSVText(cc,fsz); else SetText(cc,fsz);
    FREE(cc);
    fClose(rf);
  }
  return rf!=0;
}

template <class L>
Bool Strings<L>::SaveToFile(const char *FName) {
  d rf=fOpen(FName,"wbd+s+");
  Bool res=false;
  if (rf) {
    spstr txt=GetTextToStr();
    res=fWrite(rf,(const ptr)(txt()),txt.length())==(size_t)txt.length();
    fClose(rf);
  }
  return res;
}
#else
template <class L>
Bool Strings<L>::LoadFromFile(const char *FName,Bool is_csv) {
  Clear();
  FILE *rf=fopen(FName,"rb");
  if (rf) {
    d fsz=fsize(rf);
    char *cc=(char*)MALLOC(fsz+1);
    cc[fsz]=0;
    if (fread(cc,1,fsz,rf)==fsz)
      if (is_csv) SetCSVText(cc); else SetText(cc);
    FREE(cc);
    fclose(rf);
  }
  return rf!=0;
}

template <class L>
Bool Strings<L>::SaveToFile(const char *FName) {
  FILE *rf=fopen(FName,"wb");
  Bool res=false;
  if (rf) {
    spstr txt=GetTextToStr();
    res=fwrite((const ptr)(txt()),1,txt.length(),rf)==(size_t)txt.length();
    fclose(rf);
  }
  return res;
}
#endif

#endif // NO_LOADSAVESTRINGS

typedef Strings<l> TStrings;
typedef Strings<Pointer> TPtrStrings;

template <class F> void FreeItems(F &From) {
  for (l ll=0;ll<From.Count();ll++) {
    delete From.Objects(ll);
    From.Objects(ll)=NULL;
  }
}

template <class F> void FreeListItems(F &From) {
  for (l ll=0;ll<From.Count();ll++) {
    delete From[ll];
    From[ll]=NULL;
  }
}

template <class F> l SearchFreeNumber(F &Where) {
  for (l ll=0;ll<0x7FFFFFFF;ll++)
    if (Where.IndexOf(ll)<0) return ll;
  return -1;
}

template <class F> l SearchNULL(F &Where) {
  for (l ll=0;ll<0x7FFFFFFF;ll++)
    if (!Where[ll]) return ll;
  return -1;
}

template <class F> l SearchListNULL(F &Where) {
  for (l ll=0;ll<Where.Count();ll++)
    if (!Where[ll]) return ll;
  return -1;
}

template <class F> l SearchNULLObject(F &Where) {
  for (l ll=0;ll<Where.Count();ll++)
    if (!Where.Objects(ll)) return ll;
  return -1;
}

#ifdef SP_MEM_INTERFACE
template <class F> void SPFreeObject(F &Where) {
  for (l ll=0;ll<Where.Count();ll++)
    spfree(Where.Objects(ll));
}

template <class F> void SPFreeItems(F &Where) {
  for (l ll=0;ll<Where.Count();ll++)
    spfree(Where[ll]);
}
#endif

#ifdef _MSC_VER
#define IMPLEMENT_FAKE_COMPARATION(Class)
#else
#define IMPLEMENT_FAKE_COMPARATION(Class)                           \
  inline Bool operator==(const Class &,const Class &) { return 0; } \
  inline Bool operator!=(const Class &,const Class &) { return 1; }
#endif

#endif //SP_CLASSES
