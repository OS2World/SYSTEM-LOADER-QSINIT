/* ------------------------------------------------------------------------*/
/*                                                                         */
/*   EDITORS.H                                                             */
/*                                                                         */
/*   Copyright (c) Borland International 1991                              */
/*   All Rights Reserved.                                                  */
/*                                                                         */
/*   defines the classes TIndicator, TEditor, TMemo, TFileEditor,          */
/*   and TEditWindow                                                       */
/*                                                                         */
/* ------------------------------------------------------------------------*/

#ifdef __BORLANDC__
#if !defined( __DIR_H )
#include <dir.h>
#endif  // __DIR_H
#endif

#if !defined( __STRING_H )
#include <string.h>
#endif  // __STRING_H

#if !defined( __LIMITS_H )
#include <limits.h>
#endif  // __LIMITS_H

#include <tvvo.h>

#if !defined(__EDIT_COMMAND_CODES)
#define __EDIT_COMMAND_CODES

#define ufUpdate          (0x01)
#define ufLine            (0x02)
#define ufView            (0x04)

#define smExtend          (0x01)
#define smDouble          (0x02)

const unsigned
sfSearchFailed = UINT_MAX; // 0xFFFF;

#define cmSave            (80)
#define cmSaveAs          (81)
#define cmFind            (82)
#define cmReplace         (83)
#define cmSearchAgain     (84)

#define cmCharLeft        (500)
#define cmCharRight       (501)
#define cmWordLeft        (502)
#define cmWordRight       (503)
#define cmLineStart       (504)
#define cmLineEnd         (505)
#define cmLineUp          (506)
#define cmLineDown        (507)
#define cmPageUp          (508)
#define cmPageDown        (509)
#define cmTextStart       (510)
#define cmTextEnd         (511)
#define cmNewLine         (512)
#define cmBackSpace       (513)
#define cmDelChar         (514)
#define cmDelWord         (515)
#define cmDelStart        (516)
#define cmDelEnd          (517)
#define cmDelLine         (518)
#define cmInsMode         (519)
#define cmStartSelect     (520)
#define cmHideSelect      (521)
#define cmIndentMode      (522)
#define cmUpdateTitle     (523)

#define edOutOfMemory     (0)
#define edReadError       (1)
#define edWriteError      (2)
#define edCreateError     (3)
#define edSaveModify      (4)
#define edSaveUntitled    (5)
#define edSaveAs          (6)
#define edFind            (7)
#define edSearchFailed    (8)
#define edReplace         (9)
#define edReplacePrompt   (10)

#define efCaseSensitive   (0x0001)
#define efWholeWordsOnly  (0x0002)
#define efPromptOnReplace (0x0004)
#define efReplaceAll      (0x0008)
#define efDoReplace       (0x0010)
#define efBackupFiles     (0x0100)

#ifdef __DOS16__
#define  maxLineLength    (256)
#else
#define  maxLineLength    (16384)
#endif

#endif  // __EDIT_COMMAND_CODES

typedef ushort(*TEditorDialog)(int, ...);
ushort defEditorDialog(int dialog, ...);

#if defined( Uses_TIndicator ) && !defined( __TIndicator )
#define __TIndicator

class TRect;

class TIndicator : public TView {

public:

   TIndicator(const TRect &);

   virtual void draw();
   virtual TPalette &getPalette() const;
   virtual void setState(ushort, Boolean);
   void setValue(const TPoint &, Boolean);

protected:

   TPoint location;
   Boolean modified;

private:

   static const char dragFrame;
   static const char normalFrame;

#ifndef NO_TV_STREAMS
   virtual const char *streamableName() const
   { return name; }

protected:

   TIndicator(StreamableInit);

public:

   static const char *const name;
   static TStreamable *build();
#endif  // ifndef NO_TV_STREAMS

};

#ifndef NO_TV_STREAMS
inline ipstream &operator >> (ipstream &is, TIndicator &cl)
{ return is >> (TStreamable &)cl; }
inline ipstream &operator >> (ipstream &is, TIndicator *&cl)
{ return is >> (void *&)cl; }

inline opstream &operator << (opstream &os, TIndicator &cl)
{ return os << (TStreamable &)cl; }
inline opstream &operator << (opstream &os, TIndicator *cl)
{ return os << (TStreamable *)cl; }
#endif  // ifndef NO_TV_STREAMS

#endif  // Uses_TIndicator


#if defined( Uses_TEditor ) && !defined( __TEditor )
#define __TEditor

class TRect;
class TScrollBar;
class TIndicator;
class TEvent;

class TEditor : public TView {

public:

   friend void genRefs();

   TEditor(const TRect &, TScrollBar *, TScrollBar *, TIndicator *, size_t);
   virtual ~TEditor();

   virtual void shutDown();

   char TV_CDECL bufChar(size_t);
   size_t TV_CDECL bufPtr(size_t);
   virtual void changeBounds(const TRect &);
   virtual void convertEvent(TEvent &);
   Boolean cursorVisible();
   void deleteSelect();
   virtual void doneBuffer();
   virtual void draw();
   virtual TPalette &getPalette() const;
   virtual void handleEvent(TEvent &);
   virtual void initBuffer();
   Boolean insertBuffer(char *, size_t, size_t, Boolean, Boolean);
   virtual Boolean insertFrom(TEditor *);
   Boolean insertText(const void *, size_t, Boolean);
   void scrollTo(int, int);
   Boolean search(const char *, ushort);
   virtual Boolean setBufSize(size_t);
   void setCmdState(ushort, Boolean);
   void setSelect(size_t, size_t, Boolean);
   virtual void setState(ushort, Boolean);
   void trackCursor(Boolean);
   void undo();
   virtual void updateCommands();
   virtual Boolean valid(ushort);

   int charPos(size_t, size_t);
   size_t charPtr(size_t, int);
   Boolean clipCopy();
   void clipCut();
   void clipPaste();
   void deleteRange(size_t, size_t, Boolean);
   void doUpdate();
   void doSearchReplace();
   void drawLines(int, int, size_t);
   void TV_CDECL formatLine(void *, size_t, int, ushort);
   void find();
   size_t getMousePtr(TPoint);
   Boolean hasSelection();
   void hideSelect();
   Boolean isClipboard();
   size_t TV_CDECL lineEnd(size_t);
   size_t lineMove(size_t, int);
   size_t TV_CDECL lineStart(size_t);
   void lock();
   void newLine();
   size_t TV_CDECL nextChar(size_t);
   size_t nextLine(size_t);
   size_t nextWord(size_t);
   size_t TV_CDECL prevChar(size_t);
   size_t prevLine(size_t);
   size_t prevWord(size_t);
   void replace();
   void setBufLen(size_t);
   void setCurPtr(size_t, uchar);
   void startSelect();
   void toggleInsMode();
   void unlock();
   void update(uchar);
   void checkScrollBar(const TEvent &, TScrollBar *, int &);

   TScrollBar *hScrollBar;
   TScrollBar *vScrollBar;
   TIndicator *indicator;
   char *buffer;
   size_t bufSize;
   size_t bufLen;
   size_t gapLen;
   size_t selStart;
   size_t selEnd;
   size_t curPtr;
   TPoint curPos;
   TPoint delta;
   TPoint limit;
   int drawLine;
   size_t drawPtr;
   size_t delCount;
   size_t insCount;
   Boolean isValid;
   Boolean canUndo;
   Boolean modified;
   Boolean selecting;
   Boolean overwrite;
   Boolean autoIndent;

   static TEditorDialog editorDialog;
   static ushort editorFlags;
   static char findStr[maxFindStrLen];
   static char replaceStr[maxReplaceStrLen];
   static TEditor *clipboard;
   uchar lockCount;
   uchar updateFlags;
   int keyState;

private:

#ifndef NO_TV_STREAMS
   virtual const char *streamableName() const
   { return name; }

protected:

   TEditor(StreamableInit);
   virtual void write(opstream &);
   virtual void *read(ipstream &);

public:

   static const char *const name;
   static TStreamable *build();
#endif  // ifndef NO_TV_STREAMS

};

#ifndef NO_TV_STREAMS
inline ipstream &operator >> (ipstream &is, TEditor &cl)
{ return is >> (TStreamable &)cl; }
inline ipstream &operator >> (ipstream &is, TEditor *&cl)
{ return is >> (void *&)cl; }

inline opstream &operator << (opstream &os, TEditor &cl)
{ return os << (TStreamable &)cl; }
inline opstream &operator << (opstream &os, TEditor *cl)
{ return os << (TStreamable *)cl; }
#endif  // ifndef NO_TV_STREAMS

#endif  // Uses_TEditor

#if defined( Uses_TMemo ) && !defined( __TMemo )
#define __TMemo

class TEvent;

struct TMemoData {
   size_t length;
#if defined(__MSVC__)||defined(__IBMCPP__)
   char buffer[1];
#else
   char buffer[INT_MAX];
#endif
};

class TMemo : public TEditor {

public:

   TMemo(const TRect &, TScrollBar *, TScrollBar *, TIndicator *, size_t);
   virtual void getData(void *rec);
   virtual void setData(void *rec);
   virtual size_t dataSize();
   virtual TPalette &getPalette() const;
   virtual void handleEvent(TEvent &);

private:

#ifndef NO_TV_STREAMS
   virtual const char *streamableName() const
   { return name; }

protected:

   TMemo(StreamableInit);
   virtual void write(opstream &);
   virtual void *read(ipstream &);

public:

   static const char *const name;
   static TStreamable *build();
#endif  // ifndef NO_TV_STREAMS

};

#ifndef NO_TV_STREAMS
inline ipstream &operator >> (ipstream &is, TMemo &cl)
{ return is >> (TStreamable &)cl; }
inline ipstream &operator >> (ipstream &is, TMemo *&cl)
{ return is >> (void *&)cl; }

inline opstream &operator << (opstream &os, TMemo &cl)
{ return os << (TStreamable &)cl; }
inline opstream &operator << (opstream &os, TMemo *cl)
{ return os << (TStreamable *)cl; }
#endif  // ifndef NO_TV_STREAMS

#endif  // Uses_TMemo


#if defined( Uses_TFileEditor ) && !defined( __TFileEditor )
#define __TFileEditor

#include <tvdir.h>

class TRect;
class TScrollBar;
class TIndicator;
class TEvent;

class TFileEditor : public TEditor {

public:

   char fileName[MAXPATH];
   TFileEditor(const TRect &,
               TScrollBar *,
               TScrollBar *,
               TIndicator *,
               const char *
              );
   virtual void doneBuffer();
   virtual void handleEvent(TEvent &);
   virtual void initBuffer();
   Boolean loadFile();
   Boolean save();
   Boolean saveAs();
   Boolean saveFile();
   virtual Boolean setBufSize(size_t);
   virtual void shutDown();
   virtual void updateCommands();
   virtual Boolean valid(ushort);

private:

   static const char *backupExt;

#ifndef NO_TV_STREAMS
   virtual const char *streamableName() const
   { return name; }

protected:

   TFileEditor(StreamableInit);
   virtual void write(opstream &);
   virtual void *read(ipstream &);

public:

   static const char *const name;
   static TStreamable *build();
#endif  // ifndef NO_TV_STREAMS

};

#ifndef NO_TV_STREAMS
inline ipstream &operator >> (ipstream &is, TFileEditor &cl)
{ return is >> (TStreamable &)cl; }
inline ipstream &operator >> (ipstream &is, TFileEditor *&cl)
{ return is >> (void *&)cl; }

inline opstream &operator << (opstream &os, TFileEditor &cl)
{ return os << (TStreamable &)cl; }
inline opstream &operator << (opstream &os, TFileEditor *cl)
{ return os << (TStreamable *)cl; }
#endif  // ifndef NO_TV_STREAMS

#endif  // Uses_TFileEditor


#if defined( Uses_TEditWindow ) && !defined( __TEditWindow )
#define __TEditWindow

class TFileEditor;

class TEditWindow : public TWindow {

public:

   TEditWindow(const TRect &, const char *, int);
   virtual void close();
   virtual const char *getTitle(short);
   virtual void handleEvent(TEvent &);
   virtual void sizeLimits(TPoint &min, TPoint &max);

   TFileEditor *editor;

private:

   static const char *clipboardTitle;
   static const char *untitled;

#ifndef NO_TV_STREAMS
   virtual const char *streamableName() const
   { return name; }

protected:

   TEditWindow(StreamableInit);
   virtual void write(opstream &);
   virtual void *read(ipstream &);

public:

   static const char *const name;
   static TStreamable *build();
#endif  // ifndef NO_TV_STREAMS

};

#ifndef NO_TV_STREAMS
inline ipstream &operator >> (ipstream &is, TEditWindow &cl)
{ return is >> (TStreamable &)cl; }
inline ipstream &operator >> (ipstream &is, TEditWindow *&cl)
{ return is >> (void *&)cl; }

inline opstream &operator << (opstream &os, TEditWindow &cl)
{ return os << (TStreamable &)cl; }
inline opstream &operator << (opstream &os, TEditWindow *cl)
{ return os << (TStreamable *)cl; }
#endif  // ifndef NO_TV_STREAMS

#endif  // Uses_TEditWindow


#if defined( Uses_TFindDialogRec ) && !defined( __TFindDialogRec )
#define __TFindDialogRec

#if !defined( __STRING_H )
#include <String.h>
#endif  // __STRING_H

struct TFindDialogRec {
   TFindDialogRec(const char *str, ushort flgs) {
      strcpy(find, str);
      options = flgs;
   }
   char find[maxFindStrLen];
   ushort options;
};

#endif  // Uses_TFindDialogRec

#if defined( Uses_TReplaceDialogRec ) && !defined( __TReplaceDialogRec )
#define __TReplaceDialogRec

#if !defined( __STRING_H )
#include <String.h>
#endif  // __STRING_H

struct TReplaceDialogRec {
   TReplaceDialogRec(const char *str, const char *rep, ushort flgs) {
      strcpy(find, str);
      strcpy(replace, rep);
      options = flgs;
   }
   char find[maxFindStrLen];
   char replace[maxReplaceStrLen];
   ushort options;
};

#endif  // Uses_TReplaceDialogRec

#include <tvvo2.h>

