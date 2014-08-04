/*------------------------------------------------------------*/
/* filename -       tfildlg.cpp                               */
/*                                                            */
/* function(s)                                                */
/*                  TFileDialog member functions              */
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

#define Uses_TFileDialog
#define Uses_MsgBox
#define Uses_TRect
#define Uses_TFileInputLine
#define Uses_TButton
#define Uses_TLabel
#define Uses_TFileList
#define Uses_THistory
#define Uses_TScrollBar
#define Uses_TEvent
#define Uses_TFileInfoPane
#define Uses_opstream
#define Uses_ipstream
#include <tv.h>

#include <errno.h>
#ifndef __QSINIT__
#include <io.h>
#endif
#include <stdio.h>
#include <ctype.h>
#include <string.h>

// File dialog flags
const int
ffOpen        = 0x0001,
ffSaveAs      = 0x0002;

const int
cmOpenDialogOpen    = 100,
cmOpenDialogReplace = 101;

TFileDialog::TFileDialog(const char *aWildCard,
                         const char *aTitle,
                         const char *inputName,
                         ushort aOptions,
                         uchar histId
                        ) :
   TDialog(TRect(15, 1, 64, 20), aTitle),
   directory(0),
   TWindowInit(TFileDialog::initFrame) {
   options |= ofCentered;
   strcpy(wildCard, aWildCard);

   fileName = new TFileInputLine(TRect(3, 3, 31, 4), MAXPATH);
   strcpy(fileName->data, wildCard);
   insert(fileName);

   insert(new TLabel(TRect(2, 2, 3+cstrlen(inputName), 3),
                     inputName,
                     fileName
                    ));
   insert(new THistory(TRect(31, 3, 34, 4), fileName, histId));
   TScrollBar *sb = new TScrollBar(TRect(3, 14, 34, 15));
   insert(sb);
   insert(fileList = new TFileList(TRect(3, 6, 34, 14), sb));
   insert(new TLabel(TRect(2, 5, 8, 6), filesText, fileList));

   ushort opt = bfDefault;
   TRect r(35, 3, 46, 5);

   if ((aOptions & fdOpenButton) != 0) {
      insert(new TButton(r, openText, cmFileOpen, opt));
      opt = bfNormal;
      r.a.y += 3;
      r.b.y += 3;
   }

   if ((aOptions & fdOKButton) != 0) {
      insert(new TButton(r, okText, cmFileOpen, opt));
      opt = bfNormal;
      r.a.y += 3;
      r.b.y += 3;
   }

   if ((aOptions & fdReplaceButton) != 0) {
      insert(new TButton(r, replaceText, cmFileReplace, opt));
      opt = bfNormal;
      r.a.y += 3;
      r.b.y += 3;
   }

   if ((aOptions & fdClearButton) != 0) {
      insert(new TButton(r, clearText, cmFileClear, opt));
      //        opt = bfNormal;
      r.a.y += 3;
      r.b.y += 3;
   }

   insert(new TButton(r, cancelText, cmCancel, bfNormal));
   r.a.y += 3;
   r.b.y += 3;

   if ((aOptions & fdHelpButton) != 0) {
      insert(new TButton(r, helpText, cmHelp, bfNormal));
      //        opt = bfNormal;
      r.a.y += 3;
      r.b.y += 3;
   }

   insert(new TFileInfoPane(TRect(1, 16, 48, 18)));

   selectNext(False);
   if ((aOptions & fdNoLoadDir) == 0)
      readDirectory();
}

TFileDialog::~TFileDialog() {
   delete(char *)directory;
}

void TFileDialog::shutDown() {
   fileName = 0;
   fileList = 0;
   TDialog::shutDown();
}

static Boolean relativePath(const char *path) {
   if (path[0] != EOS && (path[0] == '\\' || path[1] == ':'))
      return False;
   else
      return True;
}

static void noWildChars(char *dest, const char *src) {
   while (*src != EOS) {
      if (*src != '?' && *src != '*') *dest++ = *src;
      src++;
   }
   *dest = EOS;
}

#if defined(__MSDOS__) || defined(__QSINIT__)
static void trim(char *dest, const char *src) {
   while (*src != EOS &&  isspace(uchar(*src))) src++;
   while (*src != EOS && !isspace(uchar(*src))) *dest++ = *src++;
   *dest = EOS;
}
#else
#define trim strcpy
#endif

void TFileDialog::getFileName(char *s) {
   char buf  [2*MAXPATH];
   char drive[MAXDRIVE], path[MAXDIR], name[MAXFILE], ext[MAXEXT];
   char TName[MAXFILE],  TExt[MAXEXT];

   if (fileName->data[0] == '.'                                 // +++ yjh
       && (fileName->data[1] == '\\' || fileName->data[1] == '\0')) { // +++ yjh
      // +++ yjh
      getcwd(buf, sizeof(buf));                                // +++ yjh
      if (fileName->data[2])                                   // +++ yjh
         trim(buf + strlen(buf), fileName->data+1);            // +++ yjh
   }                                                            // +++ yjh
   else {                                                       // +++ yjh
      trim(buf, fileName->data);
   }
   if (relativePath(buf) == True) {
      strcpy(buf, directory);
      trim(buf + strlen(buf), fileName->data);
   }
   fexpand(buf);
   fnsplit(buf, drive, path, name, ext);
   //    printf("split %s\n drive=%s,path=%s,name=%s,ext=%s\n",buf,drive,path,name,ext);
   if ((name[0] == EOS || ext[0] == EOS) && !isDir(buf)) {
      fnsplit(wildCard, 0, 0, TName, TExt);
      if (name[0] == EOS && ext[0] == EOS)
         fnmerge(buf, drive, path, TName, TExt);
      else if (name[0] == EOS)
         fnmerge(buf, drive, path, TName, ext);
      else if (ext[0] == EOS) {
         if (isWild(name))
            fnmerge(buf, drive, path, name, TExt);
         else {
            fnmerge(buf, drive, path, name, 0);
            noWildChars(buf + strlen(buf), TExt);
         }
      }
   }
   strcpy(s, buf);
   //    printf("getFileName %s\n",s);
}

void TFileDialog::handleEvent(TEvent &event) {
   TDialog::handleEvent(event);
   if (event.what == evCommand)
      switch (event.message.command) {
         case cmFileOpen:
         case cmFileReplace:
         case cmFileClear: {
            endModal(event.message.command);
            clearEvent(event);
            break;
         }
         default:
            break;
      }
}

void TFileDialog::readDirectory() {
   char curDir[MAXPATH];
   if (relativePath(wildCard)) {
      //      fileList->readDirectory( wildCard );
      getCurDir(curDir);
   } else {
      char drive[MAXDRIVE], dir[MAXDIR], name[MAXFILE], ext[MAXEXT];
      fnsplit(wildCard, drive, dir, name, ext);
      strcpy(curDir,drive);
      strcat(curDir,dir);
      strcpy(wildCard,name);
      strcat(wildCard,ext);
   }
   delete(char *)directory;
   directory = newStr(curDir);
   fileList->readDirectory(directory, wildCard);
   //    printf("curDir=%s, directory=%p\n",curDir,directory);
}

void TFileDialog::setData(void *rec) {
   TDialog::setData(rec);
   if (*(char *)rec != EOS && isWild((char *)rec)) {
      valid(cmFileInit);
      fileName->select();
   }
}

void TFileDialog::getData(void *rec) {
   getFileName((char *)rec);
}

Boolean TFileDialog::checkDirectory(const char *str) {
   if (pathValid(str))
      return True;
   else {
      messageBox(invalidDriveText, mfError | mfOKButton);
      fileName->select();
      return False;
   }
}

Boolean TFileDialog::valid(ushort command) {
   if (!TDialog::valid(command)) return False;
   if (command == cmValid || command == cmCancel || command == cmFileClear)
      return True;

   char fName[MAXPATH], drive[MAXDRIVE], dir[MAXDIR], name[MAXFILE], ext[MAXEXT];

   getFileName(fName);
   if (isWild(fName)) {
      fnsplit(fName, drive, dir, name, ext);
      char path[MAXPATH];
      strcpy(path, drive);
      strcat(path, dir);
      if (checkDirectory(path)) {
         delete(char *)directory;
         directory = newStr(path);
         strcpy(wildCard, name);
         strcat(wildCard, ext);
         if (command != cmFileInit) fileList->select();
         fileList->readDirectory(directory, wildCard);
      }
      return False;
   }
   if (isDir(fName)) {
      if (checkDirectory(fName)) {
         delete(char *)directory;
         strcat(fName, "\\");
         directory = newStr(fName);
         if (command != cmFileInit) fileList->select();
         fileList->readDirectory(directory, wildCard);
      }
      return False;
   }
   if (validFileName(fName)) return True;

   messageBox(invalidFileText, mfError | mfOKButton);
   return False;
}

#ifndef NO_TV_STREAMS
void TFileDialog::write(opstream &os) {
   TDialog::write(os);
   os.writeString(wildCard);
   os << fileName << fileList;
}

void *TFileDialog::read(ipstream &is) {
   TDialog::read(is);
   is.readString(wildCard, sizeof(wildCard));
   is >> fileName >> fileList;
   readDirectory();
   return this;
}

TStreamable *TFileDialog::build() {
   return new TFileDialog(streamableInit);
}
#endif  // ifndef NO_TV_STREAMS
