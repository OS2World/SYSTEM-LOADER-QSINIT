/*------------------------------------------------------------*/
/* filename -       tdircoll.cpp                              */
/*                                                            */
/* function(s)                                                */
/*                  TDirCollection member functions           */
/*------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#define Uses_TDirCollection
#define Uses_TDirEntry
#define Uses_opstream
#define Uses_ipstream
#define Uses_TThreaded
#include <tv.h>
#include <tvdir.h>

#ifdef __QSINIT__
#include <qsutil.h>
#include <qsio.h>
#else
#ifndef __IBMCPP__
#include <dos.h>
#endif
#include <string.h>
#include <ctype.h>
#endif
#include <sys/stat.h>

Boolean driveValid(char drive) {
   drive = toupper(drive);
#ifdef __MSDOS__
   struct diskfree_t df;
   return Boolean(_dos_getdiskfree(drive-'@',&df) == 0);
#elif defined(__OS2__)
   FSALLOCATE a;
   return Boolean(!DosQueryFSInfo(drive - '@', FSIL_ALLOC, &a, sizeof(a)));
#elif defined(__NT__)
   DWORD mask = 0x01 << (drive - 'A');
   return (Boolean)(GetLogicalDrives() & mask);
#elif defined(__QSINIT__)
   return (Boolean)(io_ismounted(drive - 'A', 0));
#else
#error Unknown platform!
#endif
}

Boolean isDir(const char *str) {
   struct stat st;
   return Boolean(stat(str, &st) == 0 && (st.st_mode & S_IFDIR));
}

Boolean pathValid(const char *path) {
   char expPath[MAXPATH];
   strcpy(expPath, path);
   fexpand(expPath);
   int len = strlen(expPath);
   if (len <= 3)
      return driveValid(expPath[0]);

   if (expPath[len-1] == '\\')
      expPath[len-1] = EOS;

   return isDir(expPath);
}

Boolean validFileName(const char *fileName) {
#ifdef __MSDOS__
   static const char *const illegalChars = ";,=+<>|\"[] \\";
#else
   static const char *const illegalChars = "<>|\"\\";
#endif

   char path[MAXPATH];
   char dir[MAXDIR];
   char name[MAXFILE];
   char ext[MAXEXT];

   ext[1] = 0; // V.Timonin
   fnsplit(fileName, path, dir, name, ext);
   strcat(path, dir);
   if (*dir != EOS && !pathValid(path))
      return False;
   if (strpbrk(name, illegalChars) != 0 ||
       strpbrk(ext+1, illegalChars) != 0 ||
       strchr(ext+1, '.') != 0
      )
      return False;
   return True;
}

void getCurDir(char *dir) {
   getcwd(dir,MAXPATH);
   int len = strlen(dir);
   if (len > 3 && len < MAXPATH - 1) {
      dir[len] = '\\';
      dir[len + 1] = '\0';
   }
}

Boolean isWild(const char *f) {
   return Boolean(strpbrk(f, "?*") != 0);
}

#ifndef NO_TV_STREAMS
TStreamable *TDirCollection::build() {
   return new TDirCollection(streamableInit);
}

void TDirCollection::writeItem(void *obj, opstream &os) {
   TDirEntry *item = (TDirEntry *)obj;
   os.writeString(item->text());
   os.writeString(item->dir());
}

void *TDirCollection::readItem(ipstream &is) {
   char *txt = is.readString();
   char *dir = is.readString();
   TDirEntry *entry = new TDirEntry(txt, dir);
   delete txt;
   delete dir;
   return entry;
}
#endif  // ifndef NO_TV_STREAMS
