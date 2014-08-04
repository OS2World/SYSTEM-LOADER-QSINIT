#ifndef NO_TV_STREAMS
/*------------------------------------------------------------*/
/* filename -       tobjstrm.cpp                              */
/*                                                            */
/* function(s)                                                */
/*                  Member function(s) of following classes:  */
/*                     TParamText                             */
/*                     TStreamable                            */
/*                     TStreamableClass                       */
/*                     TStreamableTypes                       */
/*                     TPWrittenObjects                       */
/*                     TPReadObjects                          */
/*                     pstream                                */
/*                     ipstream                               */
/*                     opstream                               */
/*                     iopstream                              */
/*                     fpbase                                 */
/*                     ifpstream                              */
/*                     ofpstream                              */
/*                     fpstream                               */
/*------------------------------------------------------------*/

/*------------------------------------------------------------*/
/*                                                            */
/*    Turbo Vision -  Version 1.0                             */
/*                                                            */
/*                                                            */
/*    Copyright (c) 1991 by Borland International             */
/*    All Rights Reserved.                                    */
/*                                                            */
/*------------------------------------------------------------*/

#define Uses_TStreamable
#define Uses_TStreamableClass
#define Uses_TStreamableTypes
#define Uses_TPWrittenObjects
#define Uses_TPReadObjects
#define Uses_pstream
#define Uses_ipstream
#define Uses_opstream
#define Uses_iopstream
#define Uses_fpbase
#define Uses_ifpstream
#define Uses_ofpstream
#define Uses_fpstream
#include <tv.h>

#include <limits.h>
#include <string.h>
#include <fstream.h>
#include <io.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <assert.h>

#if defined(__WATCOMC__)||defined(__MSVC__)
#define HARDFAIL        0
#else
#define HARDFAIL        ios::hardfail
#endif

const uchar nullStringLen = UCHAR_MAX;

TStreamableClass::TStreamableClass(const char *n, BUILDER b, int d) :
   name(n),
   build(b),
   delta(d) {
   pstream::initTypes();
   pstream::registerType(this);
}

TStreamableTypes::TStreamableTypes() : TNSSortedCollection(5, 5) {
}

void *TStreamableTypes::operator new(size_t, void *arena) {
   return arena;
}

TStreamableTypes::~TStreamableTypes() {
}

void TStreamableTypes::registerType(const TStreamableClass *d) {
   insert((void *)d);
}

const TStreamableClass *TStreamableTypes::lookup(const char *name) {
   ccIndex loc;
   if (search((void *)name, loc))
      return (TStreamableClass *)at(loc);
   else
      return 0;
}

void *TStreamableTypes::keyOf(void *d) {
   return (void *)((TStreamableClass *)d)->name;
}

int TStreamableTypes::compare(void *d1, void *d2) {
   return strcmp((char *)d1, (char *)d2);
}

TPWrittenObjects::TPWrittenObjects() : TNSSortedCollection(5, 5), curId(0) {
}

TPWrittenObjects::~TPWrittenObjects() {
}

void TPWrittenObjects::registerObject(const void *adr) {
   TPWObj *o = new TPWObj(adr, curId++);
   insert(o);
}

P_id_type TPWrittenObjects::find(const void *d) {
   ccIndex loc;
   if (search((void *)d, loc))
      return ((TPWObj *)at(loc))->ident;
   else
      return P_id_notFound;
}

void *TPWrittenObjects::keyOf(void *d) {
   return (void *)((TPWObj *)d)->address;
}

int TPWrittenObjects::compare(void *o1, void *o2) {
   if (o1 == o2)
      return 0;
#ifdef __DOS16__
   else if (((char *)o1)+1 < ((char *)o2)+1)  // force normalization
#else
   else if (o1<o2)  // flat pointers
#endif
      return -1;
   else
      return 1;
}

TPWObj::TPWObj(const void *adr, P_id_type id) : address(adr), ident(id) {
}

TPReadObjects::TPReadObjects() : TNSCollection(5, 5), curId(0) {
}

TPReadObjects::~TPReadObjects() {
}

#pragma warn -aus

void TPReadObjects::registerObject(const void *adr) {
   ccIndex loc = insert((void *)adr);
   assert(loc == curId);       // to be sure that TNSCollection
   // continues to work the way
   // it does now...
   curId++;
}

#pragma warn .aus

const void *TPReadObjects::find(P_id_type id) {
   return at(id);
}

_Cdecl pstream::pstream(streambuf *sb) {
   init(sb);
}

_Cdecl pstream::~pstream() {
}

void pstream::initTypes() {
   if (types == 0)
      types = new TStreamableTypes;
}

int _Cdecl pstream::rdstate() const {
   return state;
}

int _Cdecl pstream::eof() const {
   return state & ios::eofbit;
}

int _Cdecl pstream::fail() const {
   return state & (ios::failbit | ios::badbit | HARDFAIL);
}

int _Cdecl pstream::bad() const {
   return state & (ios::badbit | HARDFAIL);
}

int _Cdecl pstream::good() const {
   return state == 0;
}

void _Cdecl pstream::clear(int i) {
   state = (i & 0xFF) | (state & HARDFAIL);
}

void pstream::registerType(TStreamableClass *ts) {
   types->registerType(ts);
}

pstream::operator void *() const {
   return fail() ? 0 : (void *)this;
}

int _Cdecl pstream::operator!() const {
   return fail();
}

streambuf *_Cdecl pstream::rdbuf() const {
   return bp;
}

_Cdecl pstream::pstream() {
}

void _Cdecl pstream::error(StreamableError) {
   abort();
}

void _Cdecl pstream::error(StreamableError, const TStreamable &) {
   abort();
}

void _Cdecl pstream::init(streambuf *sbp) {
   state = 0;
   bp = sbp;
}

void _Cdecl pstream::setstate(int b) {
   state |= (b&0xFF);
}

_Cdecl ipstream::ipstream(streambuf *sb) {
   pstream::init(sb);
}

_Cdecl ipstream::~ipstream() {
   objs.shouldDelete = False;
   objs.shutDown();
}

streampos _Cdecl ipstream::tellg() {
   return bp->seekoff(0, ios::cur, ios::in);
}

ipstream &_Cdecl ipstream::seekg(streampos pos) {
   objs.removeAll();
   bp->seekoff(pos, ios::beg);
   return *this;
}

ipstream &_Cdecl ipstream::seekg(streamoff off, ios::seek_dir dir) {
   objs.removeAll();
   bp->seekoff(off, dir);
   return *this;
}

uchar _Cdecl ipstream::readByte() {
   return bp->sbumpc();
}

ushort _Cdecl ipstream::readWord() {
   ushort temp;
   bp->sgetn((char *)&temp, sizeof(ushort));
   return temp;
}

void _Cdecl ipstream::readBytes(void *data, size_t sz) {
   bp->sgetn((char *)data, sz);
}

char *_Cdecl ipstream::readString() {
   uchar len = readByte();
   if (len == nullStringLen)
      return 0;
   char *buf = new char[len+1];
   if (buf == 0)
      return 0;
   readBytes(buf, len);
   buf[len] = EOS;
   return buf;
}

char *_Cdecl ipstream::readString(char *buf, unsigned maxLen) {
   assert(buf != 0);

   uchar len = readByte();
   if (len > maxLen-1)
      return 0;
   readBytes(buf, len);
   buf[len] = EOS;
   return buf;
}

ipstream &_Cdecl operator >> (ipstream &ps, signed char &ch) {
   ch = ps.readByte();
   return ps;
}

ipstream &_Cdecl operator >> (ipstream &ps, unsigned char &ch) {
   ch = ps.readByte();
   return ps;
}

ipstream &_Cdecl operator >> (ipstream &ps, signed short &sh) {
   sh = ps.readWord();
   return ps;
}

ipstream &_Cdecl operator >> (ipstream &ps, unsigned short &sh) {
   sh = ps.readWord();
   return ps;
}

ipstream &_Cdecl operator >> (ipstream &ps, signed int &i) {
   i = ps.readWord();
   return ps;
}

ipstream &_Cdecl operator >> (ipstream &ps, unsigned int &i) {
   i = ps.readWord();
   return ps;
}

ipstream &_Cdecl operator >> (ipstream &ps, signed long &l) {
   ps.readBytes(&l, sizeof(l));
   return ps;
}

ipstream &_Cdecl operator >> (ipstream &ps, unsigned long &l) {
   ps.readBytes(&l, sizeof(l));
   return ps;
}

ipstream &_Cdecl operator >> (ipstream &ps, float &f) {
   ps.readBytes(&f, sizeof(f));
   return ps;
}

ipstream &_Cdecl operator >> (ipstream &ps, double &d) {
   ps.readBytes(&d, sizeof(d));
   return ps;
}

ipstream &_Cdecl operator >> (ipstream &ps, TStreamable &t) {
   const TStreamableClass *pc = ps.readPrefix();
   ps.readData(pc, &t);
   ps.readSuffix();
   return ps;
}

ipstream &_Cdecl operator >> (ipstream &ps, void *&t) {
   char ch = ps.readByte();
   switch (ch) {
      case pstream::ptNull:
         t = 0;
         break;
      case pstream::ptIndexed: {
         P_id_type index = ps.readWord();
         t = (void *)ps.find(index);
         assert(t != 0);
         break;
      }
      case pstream::ptObject: {
         const TStreamableClass *pc = ps.readPrefix();
         t = ps.readData(pc, 0);
         ps.readSuffix();
         break;
      }
      default:
         ps.error(pstream::peInvalidType);
         break;
   }
   return ps;
}

_Cdecl ipstream::ipstream() {
}

#pragma warn -aus

const TStreamableClass *_Cdecl ipstream::readPrefix() {
   char ch = readByte();
   assert(ch == '[');      // don't combine this with the previous line!
   // We must always do the read, even if we're
   // not checking assertions

   char name[128];
   readString(name, sizeof name);
   return types->lookup(name);
}

#pragma warn .aus

void *_Cdecl ipstream::readData(const TStreamableClass *c, TStreamable *mem) {
   if (mem == 0)
      mem = c->build();

   registerObject((char *)mem - c->delta);     // register the actual address
   // of the object, not the address
   // of the TStreamable sub-object
   return mem->read(*this);
}

#pragma warn -aus

void _Cdecl ipstream::readSuffix() {
   char ch = readByte();
   assert(ch == ']');      // don't combine this with the previous line!
   // We must always do the write, even if we're
   // not checking assertions

}

#pragma warn .aus

const void *_Cdecl ipstream::find(P_id_type id) {
   return objs.find(id);
}

void _Cdecl ipstream::registerObject(const void *adr) {
   objs.registerObject(adr);
}

_Cdecl opstream::opstream() {
   objs = new TPWrittenObjects;
}

_Cdecl opstream::opstream(streambuf *sb) {
   objs = new TPWrittenObjects;
   pstream::init(sb);
}

_Cdecl opstream::~opstream() {
   objs->shutDown();
   delete objs;
}

opstream &_Cdecl opstream::seekp(streampos pos) {
   objs->removeAll();
   bp->seekoff(pos, ios::beg);
   return *this;
}

opstream &_Cdecl opstream::seekp(streamoff pos, ios::seek_dir dir) {
   objs->removeAll();
   bp->seekoff(pos, dir);
   return *this;
}

streampos _Cdecl opstream::tellp() {
   return bp->seekoff(0, ios::cur, ios::out);
}

opstream &_Cdecl opstream::flush() {
   bp->sync();
   return *this;
}

void _Cdecl opstream::writeByte(uchar ch) {
   bp->sputc(ch);
}

void _Cdecl opstream::writeBytes(const void *data, size_t sz) {
   bp->sputn((char *)data, sz);
}

void _Cdecl opstream::writeWord(ushort sh) {
   bp->sputn((char *)&sh, sizeof(ushort));
}

void _Cdecl opstream::writeString(const char *str) {
   if (str == 0) {
      writeByte(nullStringLen);
      return;
   }
   int len = strlen(str);
   writeByte((uchar)len);
   writeBytes(str, len);
}

opstream &_Cdecl operator << (opstream &ps, signed char ch) {
   ps.writeByte(ch);
   return ps;
}

opstream &_Cdecl operator << (opstream &ps, unsigned char ch) {
   ps.writeByte(ch);
   return ps;
}

opstream &_Cdecl operator << (opstream &ps, signed short sh) {
   ps.writeWord(sh);
   return ps;
}

opstream &_Cdecl operator << (opstream &ps, unsigned short sh) {
   ps.writeWord(sh);
   return ps;
}

opstream &_Cdecl operator << (opstream &ps, signed int i) {
   ps.writeWord(i);
   return ps;
}

opstream &_Cdecl operator << (opstream &ps, unsigned int i) {
   ps.writeWord(i);
   return ps;
}
opstream &_Cdecl operator << (opstream &ps, signed long l) {
   ps.writeBytes(&l, sizeof(l));
   return ps;
}

opstream &_Cdecl operator << (opstream &ps, unsigned long l) {
   ps.writeBytes(&l, sizeof(l));
   return ps;
}

opstream &_Cdecl operator << (opstream &ps, float f) {
   ps.writeBytes(&f, sizeof(f));
   return ps;
}

opstream &_Cdecl operator << (opstream &ps, double d) {
   ps.writeBytes(&d, sizeof(d));
   return ps;
}

opstream &_Cdecl operator << (opstream &ps, TStreamable &t) {
   ps.writePrefix(t);
   ps.writeData(t);
   ps.writeSuffix(t);
   return ps;
}

opstream &_Cdecl operator << (opstream &ps, TStreamable *t) {
   P_id_type index;
   if (t == 0)
      ps.writeByte(pstream::ptNull);
   else if ((index = ps.find(t)) != P_id_notFound) {
      ps.writeByte(pstream::ptIndexed);
      ps.writeWord(index);
   } else {
      ps.writeByte(pstream::ptObject);
      ps << *t;
   }
   return ps;
}

void _Cdecl opstream::writePrefix(const TStreamable &t) {
   writeByte('[');
   writeString(t.streamableName());
}

void _Cdecl opstream::writeData(TStreamable &t) {
   if (types->lookup(t.streamableName()) == 0)
      error(peNotRegistered, t);
   else {
      registerObject(&t);
      t.write(*this);
   }
}

void _Cdecl opstream::writeSuffix(const TStreamable &) {
   writeByte(']');
}

P_id_type _Cdecl opstream::find(const void *adr) {
   return objs->find(adr);
}

void _Cdecl opstream::registerObject(const void *adr) {
   objs->registerObject(adr);
}

_Cdecl iopstream::iopstream(streambuf *sb) {
   pstream::init(sb);
}

_Cdecl iopstream::~iopstream() {
}

_Cdecl iopstream::iopstream() {
}

_Cdecl fpbase::fpbase() {
   pstream::init(&buf);
}

_Cdecl fpbase::fpbase(const char *name, int omode, int prot) {
   pstream::init(&buf);
   open(name, omode, prot);
}

_Cdecl fpbase::fpbase(int f) : buf(f) {
   pstream::init(&buf);
}

_Cdecl fpbase::fpbase(int f, char *b, int len) : buf(f, b, len) {
   pstream::init(&buf);
}

_Cdecl fpbase::~fpbase() {
}

void _Cdecl fpbase::open(const char *b, int m, int prot) {
   if (buf.is_open())
      clear(ios::failbit);        // fail - already open
   else if (buf.open(b, m, prot))
      clear(ios::goodbit);        // successful open
   else
      clear(ios::badbit);     // open failed
}

void _Cdecl fpbase::attach(int f) {
   if (buf.is_open())
      setstate(ios::failbit);
   else if (buf.attach(f))
      clear(ios::goodbit);
   else
      clear(ios::badbit);
}

void _Cdecl fpbase::close() {
   if (buf.close())
      clear(ios::goodbit);
   else
      setstate(ios::failbit);
}

void _Cdecl fpbase::setbuf(char *b, int len) {
   if (buf.setbuf(b, len))
      clear(ios::goodbit);
   else
      setstate(ios::failbit);
}

filebuf *_Cdecl fpbase::rdbuf() {
   return &buf;
}

_Cdecl ifpstream::ifpstream() {
}

_Cdecl ifpstream::ifpstream(const char *name, int omode, int prot) :
   fpbase(name, omode | ios::in | ios::binary, prot) {
}

_Cdecl ifpstream::ifpstream(int f) : fpbase(f) {
}

_Cdecl ifpstream::ifpstream(int f, char *b, int len) : fpbase(f, b, len) {
}

_Cdecl ifpstream::~ifpstream() {
}

filebuf *_Cdecl ifpstream::rdbuf() {
   return fpbase::rdbuf();
}

void _Cdecl ifpstream::open(const char *name, int omode, int prot) {
   fpbase::open(name, omode | ios::in | ios::binary, prot);
}

_Cdecl ofpstream::ofpstream() {
}

_Cdecl ofpstream::ofpstream(const char *name, int omode, int prot) :
   fpbase(name, omode | ios::out | ios::binary, prot) {
}

_Cdecl ofpstream::ofpstream(int f) : fpbase(f) {
}

_Cdecl ofpstream::ofpstream(int f, char *b, int len) : fpbase(f, b, len) {
}

_Cdecl ofpstream::~ofpstream() {
}

filebuf *_Cdecl ofpstream::rdbuf() {
   return fpbase::rdbuf();
}

void _Cdecl ofpstream::open(const char *name, int omode, int prot) {
   fpbase::open(name, omode | ios::out | ios::binary, prot);
}

_Cdecl fpstream::fpstream() {
}

_Cdecl fpstream::fpstream(const char *name, int omode, int prot) :
   fpbase(name, omode | ios::binary, prot) {
}

_Cdecl fpstream::fpstream(int f) : fpbase(f) {
}

_Cdecl fpstream::fpstream(int f, char *b, int len) : fpbase(f, b, len) {
}

_Cdecl fpstream::~fpstream() {
}

filebuf *_Cdecl fpstream::rdbuf() {
   return fpbase::rdbuf();
}

void _Cdecl fpstream::open(const char *name, int omode, int prot) {
   fpbase::open(name, omode | ios::in | ios::out | ios::binary, prot);
}

#endif  // ifndef NO_TV_STREAMS

