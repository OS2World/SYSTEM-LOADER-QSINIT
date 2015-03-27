//
//  ini files
//
//  (c) dixie/SP 1997-2013. Freeware.
//
#ifndef SP_INIFILES
#define SP_INIFILES

#include "classes.hpp"

#if defined(NO_LOADSAVESTRINGS)
#define NO_INIFILESRW
#endif

class TINIFile {
protected:
  Bool       Changes;
  Bool       NoWrite;
  TStrings      File;
  TStrings  Sections;
  spstr        FName;
  l SectionIndex(const spstr &Section);
  l ValueIndex(const spstr &Section,const spstr &Key);
  l CountSection(const spstr &Section);
public:
#ifndef NO_INIFILESRW
  // TIniFile
  TINIFile(const char*, Bool NoFileUpdate=false);
#endif
  // TMemIniFile
  TINIFile(const TStrings *from=NULL);
  virtual ~TINIFile();

  const TStrings &GetFileText() const { return File; }

  Bool  SectionExists(const spstr &Section) { return SectionIndex(Section)>=0; }
  spstr ReadString   (const spstr &Section,const spstr &Key,const spstr Default=spstr());
  l     ReadInteger  (const spstr &Section,const spstr &Key,l Default=0);
  void  WriteString  (const spstr &Section,const spstr &Key,const spstr &Value);
  void  WriteInteger (const spstr &Section,const spstr &Key,l Value);
  void  WriteStr     (const spstr &Section,const spstr &Key,const spstr &Value)
                       { WriteString(Section,Key,Value); }
  spstr ReadStr      (const char  *Section,const char  *Key,const char *Default="");
  spstr ReadStr      (const spstr &Section,const spstr &Key,const spstr Default=spstr())
                       { return ReadString(Section,Key,Default); }
  l     ReadInt      (const char  *Section,const char  *Key,l Default=0);
  l     ReadInt      (const spstr &Section,const spstr &Key,l Default=0)
                       { return ReadInteger(Section,Key,Default); }
  void  WriteInt     (const spstr &Section,const spstr &Key,l Value)
                       { WriteInteger(Section,Key,Value); }
  Bool  ReadBool     (const spstr &Section,const spstr &Key,Bool Default=false)
                       { return ReadInteger(Section,Key,Default)!=0?1:0; }
  void  WriteBool    (const spstr &Section,const spstr &Key,Bool Value)
                       { WriteInteger(Section,Key,Value); }
  float ReadFloat    (const spstr &Section,const spstr &Key,float Default=.0);
  void  WriteFloat   (const spstr &Section,const spstr &Key,float Value);
  TList ReadArray    (const spstr &Section,const spstr &Key,const TList &Default=TList());
  void  WriteArray   (const spstr &Section,const spstr &Key,TList &Value);
  void  WriteArray   (const spstr &Section,const spstr &Key,ptr Buf,d Size);
  void  WriteSection (const spstr &Section,const TStrings &Text,Bool Replace=true);
  TStrings ReadSections       ();
  void     ReadSections       (TStrings &names);
  // read non-uppercased section names (slow)
  TStrings ReadSectionOrgNames();
  void     ReadSectionOrgNames(TStrings &names);
  TStrings ReadSection        (const spstr &Section);
  void     ReadSection        (const spstr &Section,TStrings &text);
  TStrings ReadSectionKeys    (const spstr &Section);
  void     ReadSectionKeys    (const spstr &Section,TStrings &keys);
  TStrings ReadSectionValues  (const spstr &Section);
  void     ReadSectionValues  (const spstr &Section,TStrings &values);
  void  EraseSection (const spstr &Section);
  void  DeleteKey    (const spstr &Section,const spstr &Key);
#ifndef NO_INIFILESRW
  Bool  SaveCopy     (const char *NewFile) { return File.SaveToFile(NewFile); }
  void  Flush()      { File.SaveToFile(FName()); }
#endif
  Bool  ValueExists  (const spstr &Section,const spstr &Key)
                       { return ValueIndex(Section,Key)>=0; }
  spstr FileName()   { return FName; }
};

#endif //SP_INIFILES
