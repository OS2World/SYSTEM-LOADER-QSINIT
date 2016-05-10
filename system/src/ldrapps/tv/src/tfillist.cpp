/*------------------------------------------------------------*/
/* filename -       tfillist.cpp                              */
/*                                                            */
/* function(s)                                                */
/*                  TFileList member functions                */
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

#define Uses_TVMemMgr
#define Uses_MsgBox
#define Uses_TFileList
#define Uses_TRect
#define Uses_TSearchRec
#define Uses_TEvent
#define Uses_TGroup
#include <tv.h>
#include <tvdir.h>

#include <errno.h>
#ifndef __QSINIT__
#include <io.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#ifndef __IBMCPP__
#include <dos.h>
#endif
#else // __QSINIT__
#include <stdlib.h>
#include <time.h>
#endif
#include <string.h>

#ifdef __WATCOMC__
#pragma disable_message (919);
#endif

TFileList::TFileList(const TRect &bounds,
                     TScrollBar *aScrollBar) :
   TSortedListBox(bounds, 2, aScrollBar) {
}

TFileList::~TFileList() {
   if (list()) destroy(list());
}

void TFileList::focusItem(int item) {
   TSortedListBox::focusItem(item);
   message(owner, evBroadcast, cmFileFocused, list()->at(item));
}

void TFileList::getData(void *) {
}

void TFileList::setData(void *) {
}

size_t TFileList::dataSize() {
   return 0;
}

void *TFileList::getKey(const char *s) {
   static TSearchRec sR;

   if ((getShiftState() & 0x03) != 0 || *s == '.')
      sR.attr = FA_DIREC;
   else
      sR.attr = 0;
   strcpy(sR.name, s);
#ifdef __MSDOS__
   strupr(sR.name);
#endif
   return &sR;
}

void TFileList::getText(char *dest, int item, int maxChars) {
   TSearchRec *f = (TSearchRec *)(list()->at(item));

   strncpy(dest, f->name, maxChars);
   dest[maxChars] = '\0';
   if (f->attr & FA_DIREC) strcat(dest, "\\");
}

void TFileList::handleEvent(TEvent &event) {
   if (event.what == evMouseDown && event.mouse.doubleClick) {
      event.what = evCommand;
      event.message.command = cmOK;
      putEvent(event);
      clearEvent(event);
   } else
      TSortedListBox::handleEvent(event);
}

void TFileList::readDirectory(const char *dir, const char *wildCard) {
   char path[MAXPATH];
   strcpy(path, dir);
   strcat(path, wildCard);
   readDirectory(path);
}

struct DirSearchRec : public TSearchRec {
   void *operator new(size_t);
};

void *DirSearchRec::operator new(size_t sz) {
   void *temp = ::operator new(sz);
   if (TVMemMgr::safetyPoolExhausted()) {
      delete temp;
      temp = 0;
   }
   return temp;
}

static long SystemTimeRecode(ffblk &s) {
#ifdef __QSINIT__
   return timetodostime(s.ff_ftime);
#elif defined(__OS2__)
   return *(unsigned short *)&s.ff_ftime+(*(unsigned short *)&s.ff_fdate<<16);
#elif !defined(__MSVC__)&&!defined(__IBMCPP__)
   return s.ff_ftime + (long(s.ff_fdate) << 16);
#else
   long res=0;
   FileTimeToDosDateTime(&s.ff_ftime,((ushort *)&res)+1,(ushort *)&res);
   return res;
#endif
}

void TFileList::readDirectory(const char *aWildCard) {
   ffblk s;
   char path[MAXPATH], drive[MAXDRIVE], dir[MAXDIR], file[MAXFILE], ext[MAXEXT];
   int res;
   DirSearchRec *p;
   const unsigned findAttr = FA_RDONLY | FA_ARCH;
   TFileCollection *fileList = new TFileCollection(256, 256);

   strcpy(path, aWildCard);
   fexpand(path);
   fnsplit(path, drive, dir, file, ext);

   long handle = findfirst(aWildCard, &s, findAttr);
   p = (DirSearchRec *)&p; res=handle==-1;
   while (p != 0 && res == 0) {
      if ((s.ff_attrib & FA_DIREC) == 0) {
         p = new DirSearchRec;
         if (p != 0) {
            p->attr = s.ff_attrib;
            p->time = SystemTimeRecode(s);
            p->size = s.ff_fsize;
            strcpy(p->name,s.ff_name);
            fileList->insert(p);
         }
      }
#if defined(__MSVC__)||defined(__IBMCPP__)
      res = findnext(handle, &s);
#else
      res = findnext(&s);
#endif
   }
#if defined(__MSVC__)||defined(__IBMCPP__)
   findclose(handle);
#else
   findclose(&s);
#endif

   fnmerge(path, drive, dir, "*", ".*");

   int  upattr = FA_DIREC;
   long uptime = 0x210000uL;
   long upsize = 0;

   handle = findfirst(path, &s, FA_DIREC); res=0; res=handle==-1;
   while (p != 0 && res == 0) {
      if ((s.ff_attrib & FA_DIREC) != 0) {
         if (strcmp(s.ff_name,"..") == 0) {
            upattr = s.ff_attrib;
            uptime = SystemTimeRecode(s);
            upsize = s.ff_fsize;
         } else if (s.ff_name[0] != '.' || s.ff_name[1] != '\0') {
            p = new DirSearchRec;
            if (p != 0) {
               p->attr = s.ff_attrib;
               p->time = SystemTimeRecode(s);
               p->size = s.ff_fsize;
               strcpy(p->name,s.ff_name);
               fileList->insert(p);
            }
         }
      }
#if defined(__MSVC__)||defined(__IBMCPP__)
      res = findnext(handle, &s);
#else
      res = findnext(&s);
#endif
   }
#if defined(__MSVC__)||defined(__IBMCPP__)
   findclose(handle);
#else
   findclose(&s);
#endif
   if (dir[0] != '\0' && dir[1] != '\0') {
      p = new DirSearchRec;
      if (p != 0) {
         p->attr = upattr;
         p->time = uptime;
         p->size = upsize;
         strcpy(p->name, "..");
         fileList->insert(p);
      }
   }
   if (p == 0)
      messageBox(tooManyFiles, mfOKButton | mfWarning);
   newList(fileList);
   if (list()->getCount() > 0)
      message(owner, evBroadcast, cmFileFocused, list()->at(0));
   else {
      static DirSearchRec noFile;
      message(owner, evBroadcast, cmFileFocused, &noFile);
   }
}

/*
    fexpand:    reimplementation of pascal's FExpand routine.  Takes a
                relative DOS path and makes an absolute path of the form

                    drive:\[subdir\ ...]filename.ext

                works with '/' or '\' as the subdir separator on input;
                changes all to '\' on output.

*/
static void squeeze(char *path) {
   char *dest = path;
   char *src  = path;

   while (*src != 0) {
      if (*src == '.')
         if (src[1] == '.') {
            src += 2;
            if (dest > path) {
               dest--;
               while ((*--dest != '\\')&&(dest > path)) // back up to the previous '\'
                  ;
               dest++;         // move to the next position
            }
         } else if (src[1] == '\\') src++;
         else *dest++ = *src++;
      else *dest++ = *src++;
   }
   *dest = EOS;                // zero terminator
   dest  = path;
   src   = path;
   while (*src != 0) {
      if ((*src == '\\')&&(src[1] == '\\'))
         src++;
      else
         *dest++ = *src++;
   }
   *dest = EOS;                // zero terminator
}

void fexpand(char *rpath) {
   char path[MAXPATH];
   char drive[MAXDRIVE];
   char dir[MAXDIR];
   char file[MAXFILE];
   char ext[MAXEXT];

   int flags = fnsplit(rpath, drive, dir, file, ext);
   if ((flags & DRIVE) == 0) {
      drive[0] = getdisk() + 'A';
      drive[1] = ':';
      drive[2] = '\0';
   }
   drive[0] = toupper(drive[0]);
   if ((flags & DIRECTORY) == 0 || (dir[0] != '\\' && dir[0] != '/')) {
      char curdir[MAXDIR];
      getcurdir(drive[0] - 'A' + 1, curdir);
      // ++ V.Timonin : better more than nothing
      int len = strlen(curdir);
      if (curdir[len - 1] != '\\') {
         curdir[len] = '\\';
         curdir[len + 1] = EOS;
      }
      // -- V.Timonin
      strcat(curdir, dir);
      if (*curdir != '\\' && *curdir != '/') {
         *dir = '\\';
         strcpy(dir+1, curdir);
      } else
         strcpy(dir, curdir);
   }

   //++ V.Timonin - squeeze must be after '/' --> '\\'
   char *p = dir;
   while ((p = strchr(p, '/')) != 0) *p = '\\';
   squeeze(dir);
   //-- V.Timonin
   fnmerge(path, drive, dir, file, ext);
#ifdef __MSDOS__
   strupr(path);
#endif
   strcpy(rpath, path);
}


#ifndef NO_TV_STREAMS
TStreamable *TFileList::build() {
   return new TFileList(streamableInit);
}
#endif  // ifndef NO_TV_STREAMS


