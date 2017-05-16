/* ------------------------------------------------------------------------*/
/*                                                                         */
/*   VIEWS.H                                                               */
/*                                                                         */
/*   defines the classes TView, TFrame, TScrollBar, TScroller,             */
/*   TListViewer, TGroup, and TWindow                                      */
/*                                                                         */
/* ------------------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#include <tvvo.h>

#if !defined( __COMMAND_CODES )
#define __COMMAND_CODES

//  Standard command codes
#define cmValid         (0)
#define cmQuit          (1)
#define cmError         (2)
#define cmMenu          (3)
#define cmClose         (4)
#define cmZoom          (5)
#define cmResize        (6)
#define cmNext          (7)
#define cmPrev          (8)
#define cmHelp          (9)

//  TDialog standard commands
#define cmOK            (10)
#define cmCancel        (11)
#define cmYes           (12)
#define cmNo            (13)
#define cmDefault       (14)

// Standard application commands
#define cmNew           (30)
#define cmOpen          (31)
#define cmSave          (32)
#define cmSaveAs        (33)
#define cmSaveAll       (34)
#define cmChDir         (35)
#define cmDosShell      (36)
#define cmCloseAll      (37)

//  TView State masks
#define sfVisible       (0x001)
#define sfCursorVis     (0x002)
#define sfCursorIns     (0x004)
#define sfShadow        (0x008)
#define sfActive        (0x010)
#define sfSelected      (0x020)
#define sfFocused       (0x040)
#define sfDragging      (0x080)
#define sfDisabled      (0x100)
#define sfModal         (0x200)
#define sfDefault       (0x400)
#define sfExposed       (0x800)

// TView Option masks
#define ofSelectable    (0x001)
#define ofTopSelect     (0x002)
#define ofFirstClick    (0x004)
#define ofFramed        (0x008)
#define ofPreProcess    (0x010)
#define ofPostProcess   (0x020)
#define ofBuffered      (0x040)
#define ofTileable      (0x080)
#define ofCenterX       (0x100)
#define ofCenterY       (0x200)
#define ofCentered      (0x300)
#define ofValidate      (0x400)

// TView GrowMode masks
#define gfGrowLoX       (0x01)
#define gfGrowLoY       (0x02)
#define gfGrowHiX       (0x04)
#define gfGrowHiY       (0x08)
#define gfGrowAll       (0x0f)
#define gfGrowRel       (0x10)
#define gfFixed         (0x20)

// TView DragMode masks
#define dmDragMove      (0x01)
#define dmDragGrow      (0x02)
#define dmLimitLoX      (0x10)
#define dmLimitLoY      (0x20)
#define dmLimitHiX      (0x40)
#define dmLimitHiY      (0x80)
#define dmLimitAll      (dmLimitLoX | dmLimitLoY | dmLimitHiX | dmLimitHiY)

// TView Help context codes
#define hcNoContext     (0)
#define hcDragging      (1)

// TScrollBar part codes
#define sbLeftArrow     (0)
#define sbRightArrow    (1)
#define sbPageLeft      (2)
#define sbPageRight     (3)
#define sbUpArrow       (4)
#define sbDownArrow     (5)
#define sbPageUp        (6)
#define sbPageDown      (7)
#define sbIndicator     (8)

// TScrollBar options for TWindow.StandardScrollBar
#define sbHorizontal     (0x000)
#define sbVertical       (0x001)
#define sbHandleKeyboard (0x002)

// TWindow Flags masks
#define wfMove          (0x01)
#define wfGrow          (0x02)
#define wfClose         (0x04)
#define wfZoom          (0x08)

//  TView inhibit flags
#define noMenuBar       (0x0001)
#define noDeskTop       (0x0002)
#define noStatusLine    (0x0004)
#define noBackground    (0x0008)
#define noFrame         (0x0010)
#define noViewer        (0x0020)
#define noHistory       (0x0040)

// TWindow number constants
#define wnNoNumber      (0)

// TWindow palette entries
#define wpBlueWindow    (0)
#define wpCyanWindow    (1)
#define wpGrayWindow    (2)

//  Application command codes
#define cmCut           (20)
#define cmCopy          (21)
#define cmPaste         (22)
#define cmUndo          (23)
#define cmClear         (24)
#define cmTile          (25)
#define cmCascade       (26)

// Standard messages
#define cmReceivedFocus     (50)
#define cmReleasedFocus     (51)
#define cmCommandSetChanged (52)

// TScrollBar messages
#define cmScrollBarChanged  (53)
#define cmScrollBarClicked  (54)

// TWindow select messages
#define cmSelectWindowNum   (55)

//  TListViewer messages
#define cmListItemSelected  (56)

//  Event masks
#define positionalEvents    (evMouse)
#define focusedEvents       (evKeyboard | evCommand)

#endif  // __COMMAND_CODES

#if defined( Uses_TCommandSet ) && !defined( __TCommandSet )
#define __TCommandSet

class TCommandSet {

public:

   TCommandSet();
   TCommandSet(const TCommandSet &);

   Boolean has(int cmd);

   void disableCmd(int cmd);
   void enableCmd(int cmd);
   void operator += (int cmd);
   void operator -= (int cmd);

   void disableCmd(const TCommandSet &);
   void enableCmd(const TCommandSet &);
   void operator += (const TCommandSet &);
   void operator -= (const TCommandSet &);

   Boolean TCommandSet::isEmpty();

   TCommandSet &operator &= (const TCommandSet &);
   TCommandSet &operator |= (const TCommandSet &);

   friend TCommandSet operator & (const TCommandSet &, const TCommandSet &);
   friend TCommandSet operator | (const TCommandSet &, const TCommandSet &);

   friend int operator == (const TCommandSet &tc1, const TCommandSet &tc2);
   friend int operator != (const TCommandSet &tc1, const TCommandSet &tc2);

private:

   int loc(int);
   int mask(int);

   static int masks[8];

   uchar cmds[32];

};

inline void TCommandSet::operator += (int cmd) {
   enableCmd(cmd);
}

inline void TCommandSet::operator -= (int cmd) {
   disableCmd(cmd);
}

inline void TCommandSet::operator += (const TCommandSet &tc) {
   enableCmd(tc);
}

inline void TCommandSet::operator -= (const TCommandSet &tc) {
   disableCmd(tc);
}

inline int operator != (const TCommandSet &tc1, const TCommandSet &tc2) {
   return !operator == (tc1, tc2);
}

inline int TCommandSet::loc(int cmd) {
   return cmd / 8;
}

inline int TCommandSet::mask(int cmd) {
   return masks[ cmd & 0x07 ];
}

#endif  // Uses_TCommandSet

#if defined( Uses_TPalette ) && !defined( __TPalette )
#define __TPalette

class TPalette {

public:

   TPalette(const char *, ushort);
   TPalette(const TPalette &);
   ~TPalette();

   TPalette &operator = (const TPalette &);

   uchar &operator[](int) const;

   uchar *data;

};

#endif  // Uses_TPalette

#if defined( Uses_TView ) && !defined( __TView )
#define __TView

struct write_args {
   void *self;
   void *target;
   void *buf;
   ushort offset;
};

class TRect;
class TEvent;
class TGroup;

class TView : public TObject
#ifndef NO_TV_STREAMS
   , public TStreamable
#endif  // !NO_TV_STREAMS
{

public:

   friend void genRefs();

   enum phaseType { phFocused, phPreProcess, phPostProcess };
   enum selectMode { normalSelect, enterSelect, leaveSelect };

   TView(const TRect &bounds);
   ~TView();

   virtual void sizeLimits(TPoint &min, TPoint &max);
   TRect getBounds();
   TRect getExtent();
   TRect getClipRect();
   Boolean mouseInView(TPoint mouse);
   Boolean containsMouse(TEvent &event);

   void locate(TRect &bounds);
   virtual void dragView(TEvent &event, uchar mode,    //  temporary fix
                         TRect &limits, TPoint minSize, TPoint maxSize);  //  for Miller's stuff
   virtual void calcBounds(TRect &bounds, TPoint delta);
   virtual void changeBounds(const TRect &bounds);
   void growTo(short x, short y);
   void moveTo(short x, short y);
   void setBounds(const TRect &bounds);

   virtual ushort getHelpCtx();

   virtual Boolean valid(ushort command);

   void hide();
   void show();
   virtual void draw();
   void drawView();
   Boolean exposed();
   Boolean focus();
   void hideCursor();
   void drawHide(TView *lastView);
   void drawShow(TView *lastView);
   void drawUnderRect(TRect &r, TView *lastView);
   void drawUnderView(Boolean doShadow, TView *lastView);

   virtual size_t dataSize();
   virtual void getData(void *rec);
   virtual void setData(void *rec);

   virtual void awaken();
   void blockCursor();
   void normalCursor();
   virtual void resetCursor();
   void setCursor(int x, int y);
   void showCursor();
   void drawCursor();

   void clearEvent(TEvent &event);
   Boolean eventAvail();
   virtual void getEvent(TEvent &event);
   virtual void handleEvent(TEvent &event);
   virtual void putEvent(TEvent &event);

   static Boolean commandEnabled(ushort command);
   static void disableCommands(TCommandSet &commands);
   static void enableCommands(TCommandSet &commands);
   static void disableCommand(ushort command);
   static void enableCommand(ushort command);
   static void getCommands(TCommandSet &commands);
   static void setCommands(TCommandSet &commands);
   static void setCmdState(TCommandSet &commands, Boolean enable);

   virtual void endModal(ushort command);
   virtual ushort execute();

   ushort getColor(ushort color);
   virtual TPalette &getPalette() const;
   uchar mapColor(uchar);

   Boolean getState(ushort aState);
   void select();
   virtual void setState(ushort aState, Boolean enable);

   void keyEvent(TEvent &event);
   Boolean mouseEvent(TEvent &event, ushort mask);


   TPoint makeGlobal(TPoint source);
   TPoint makeLocal(TPoint source);

   TView *nextView();
   TView *prevView();
   TView *prev();
   TView *next;

   void makeFirst();
   void putInFrontOf(TView *Target);
   TView *TopView();

   void writeBuf(short x, short y, short w, short h, const void *b);
   void writeBuf(short x, short y, short w, short h, const TDrawBuffer &b);
   void writeChar(short x, short y, char c, uchar color, short count);
   void writeLine(short x, short y, short w, short h, const TDrawBuffer &b);
   void writeLine(short x, short y, short w, short h, const void *b);
   void writeStr(short x, short y, const char *str, uchar color);

   TPoint size;
   ushort options;
   ushort eventMask;
   ushort state;
   TPoint origin;
   TPoint cursor;
   uchar growMode;
   uchar dragMode;
   ushort helpCtx;
   static Boolean commandSetChanged;
   static TCommandSet curCommandSet;
   TGroup *owner;

   static Boolean showMarkers;
   static uchar errorAttr;

   virtual void shutDown();
   // dixie: just 8 bytes for any user needs, zeroed in constructor
   ulong userValue[2];
private:

   void moveGrow(TPoint p,
                 TPoint s,
                 TRect &limits,
                 TPoint minSize,
                 TPoint maxSize,
                 uchar mode
                );
   void change(uchar, TPoint delta, TPoint &p, TPoint &s, ulong ctrlState);

   int exposedRec1(short x1, short x2, TView *p);
   int exposedRec2(short x1, short x2, TView *p);

protected:
#ifdef __DOS16__
   static void writeView(write_args);
#else
   void writeViewRec1(short x1, short x2, TView *p, int shadowCounter);
   void writeViewRec2(short x1, short x2, TView *p, int shadowCounter);
   void writeView(short x1, short x2, short y, const void *buf);
#endif
public:

#ifndef NO_TV_STREAMS
   virtual const char *streamableName() const
   { return name; }

protected:

   TView(StreamableInit);

public:

   static const char *const name;
   static TStreamable *build();

protected:

   virtual void write(opstream &);
   virtual void *read(ipstream &);
#endif  // ifndef NO_TV_STREAMS

};

#ifndef NO_TV_STREAMS
inline ipstream &operator >> (ipstream &is, TView &cl)
{ return is >> (TStreamable &)cl; }
inline ipstream &operator >> (ipstream &is, TView *&cl)
{ return is >> (void *&)cl; }

inline opstream &operator << (opstream &os, TView &cl)
{ return os << (TStreamable &)cl; }
inline opstream &operator << (opstream &os, TView *cl)
{ return os << (TStreamable *)cl; }
#endif  // ifndef NO_TV_STREAMS

inline void TView::writeBuf(short x, short y, short w, short h,
                            const TDrawBuffer &b) {
   writeBuf(x, y, w, h, b.data);
}

inline void TView::writeLine(short x, short y, short w, short h,
                             const TDrawBuffer &b) {
   writeLine(x, y, w, h, b.data);
}

#endif  // Uses_TView

/* ---------------------------------------------------------------------- */
/*      class TFrame                                                      */
/*                                                                        */
/*      Palette layout                                                    */
/*        1 = Passive frame                                               */
/*        2 = Passive title                                               */
/*        3 = Active frame                                                */
/*        4 = Active title                                                */
/*        5 = Icons                                                       */
/* ---------------------------------------------------------------------- */

#if defined( Uses_TFrame ) && !defined( __TFrame )
#define __TFrame

class TRect;
class TEvent;
class TDrawBuffer;

class TFrame : public TView {

public:

   TFrame(const TRect &bounds);

   virtual void draw();
   virtual TPalette &getPalette() const;
   virtual void handleEvent(TEvent &event);
   virtual void setState(ushort aState, Boolean enable);

private:

   void frameLine(TDrawBuffer &frameBuf, short y, short n, uchar color);
   void dragWindow(TEvent &event, uchar dragMode);

   friend class TDisplay;
   static const char initFrame[19];
   static char frameChars[33];
   static const char *closeIcon;
   static const char *zoomIcon;
   static const char *unZoomIcon;
   static const char *dragIcon;

#ifndef NO_TV_STREAMS
   virtual const char *streamableName() const
   { return name; }

protected:

   TFrame(StreamableInit);

public:

   static const char *const name;
   static TStreamable *build();
#endif  // ifndef NO_TV_STREAMS

};

#ifndef NO_TV_STREAMS
inline ipstream &operator >> (ipstream &is, TFrame &cl)
{ return is >> (TStreamable &)cl; }
inline ipstream &operator >> (ipstream &is, TFrame *&cl)
{ return is >> (void *&)cl; }

inline opstream &operator << (opstream &os, TFrame &cl)
{ return os << (TStreamable &)cl; }
inline opstream &operator << (opstream &os, TFrame *cl)
{ return os << (TStreamable *)cl; }
#endif  // ifndef NO_TV_STREAMS

#endif  // Uses_TFrame

/* ---------------------------------------------------------------------- */
/*      class TScrollBar                                                  */
/*                                                                        */
/*      Palette layout                                                    */
/*        1 = Page areas                                                  */
/*        2 = Arrows                                                      */
/*        3 = Indicator                                                   */
/* ---------------------------------------------------------------------- */

#if defined( Uses_TScrollBar ) && !defined( __TScrollBar )
#define __TScrollBar

class TRect;
class TEvent;

typedef char TScrollChars[5];

class TScrollBar : public TView {

public:

   TScrollBar(const TRect &bounds);

   virtual void draw();
   virtual TPalette &getPalette() const;
   virtual void handleEvent(TEvent &event);
   virtual void scrollDraw();
   virtual int scrollStep(int part);
   void setParams(int aValue, int aMin, int aMax,
                  int aPgStep, int aArStep);
   void setRange(int aMin, int aMax);
   void setStep(int aPgStep, int aArStep);
   void setValue(int aValue);

   void drawPos(int pos);
   int getPos();
   int getSize();

   int value;

   TScrollChars chars;
   int minVal;
   int maxVal;
   int pgStep;
   int arStep;

private:

   int getPartCode(void);

   static TScrollChars vChars;
   static TScrollChars hChars;

#ifndef NO_TV_STREAMS
   virtual const char *streamableName() const
   { return name; }

protected:

   TScrollBar(StreamableInit);
   virtual void write(opstream &);
   virtual void *read(ipstream &);

public:

   static const char *const name;
   static TStreamable *build();
#endif  // ifndef NO_TV_STREAMS

};

#ifndef NO_TV_STREAMS
inline ipstream &operator >> (ipstream &is, TScrollBar &cl)
{ return is >> (TStreamable &)cl; }
inline ipstream &operator >> (ipstream &is, TScrollBar *&cl)
{ return is >> (void *&)cl; }

inline opstream &operator << (opstream &os, TScrollBar &cl)
{ return os << (TStreamable &)cl; }
inline opstream &operator << (opstream &os, TScrollBar *cl)
{ return os << (TStreamable *)cl; }
#endif  // ifndef NO_TV_STREAMS

#endif  // Uses_TScrollBar

/* ---------------------------------------------------------------------- */
/*      class TScroller                                                   */
/*                                                                        */
/*      Palette layout                                                    */
/*      1 = Normal text                                                   */
/*      2 = Selected text                                                 */
/* ---------------------------------------------------------------------- */

#if defined( Uses_TScroller ) && !defined( __TScroller )
#define __TScroller

class TRect;
class TScrollBar;
class TEvent;

class TScroller : public TView {

public:

   TScroller(const TRect &bounds,
             TScrollBar *aHScrollBar,
             TScrollBar *aVScrollBar
            );

   virtual void changeBounds(const TRect &bounds);
   virtual TPalette &getPalette() const;
   virtual void handleEvent(TEvent &event);
   virtual void scrollDraw();
   void scrollTo(int x, int y);
   void setLimit(int x, int y);
   virtual void setState(ushort aState, Boolean enable);
   void checkDraw();
   virtual void shutDown();
   TPoint delta;

protected:

   uchar drawLock;
   Boolean drawFlag;
   TScrollBar *hScrollBar;
   TScrollBar *vScrollBar;
   TPoint limit;

private:

   void showSBar(TScrollBar *sBar);

#ifndef NO_TV_STREAMS
   virtual const char *streamableName() const
   { return name; }

protected:

   TScroller(StreamableInit);
   virtual void write(opstream &);
   virtual void *read(ipstream &);

public:

   static const char *const name;
   static TStreamable *build();
#endif  // ifndef NO_TV_STREAMS

};

#ifndef NO_TV_STREAMS
inline ipstream &operator >> (ipstream &is, TScroller &cl)
{ return is >> (TStreamable &)cl; }
inline ipstream &operator >> (ipstream &is, TScroller *&cl)
{ return is >> (void *&)cl; }

inline opstream &operator << (opstream &os, TScroller &cl)
{ return os << (TStreamable &)cl; }
inline opstream &operator << (opstream &os, TScroller *cl)
{ return os << (TStreamable *)cl; }
#endif  // ifndef NO_TV_STREAMS

#endif  // Uses_TScroller

#if defined( Uses_TListViewer ) && !defined( __TListViewer )
#define __TListViewer

class TRect;
class TScrollBar;
class TEvent;

class TListViewer : public TView {

   static const char *emptyText;

public:

   TListViewer(const TRect &bounds,
               ushort aNumCols,
               TScrollBar *aHScrollBar,
               TScrollBar *aVScrollBar
              );

   virtual void changeBounds(const TRect &bounds);
   virtual void draw();
   virtual void focusItem(int item);
   virtual TPalette &getPalette() const;
   virtual void getText(char *dest, int item, int maxLen);
   virtual Boolean isSelected(int item);
   virtual void handleEvent(TEvent &event);
   virtual void selectItem(int item);
   void setRange(int aRange);
   virtual void setState(ushort aState, Boolean enable);

   virtual void focusItemNum(int item);
   virtual void shutDown();

   TScrollBar *hScrollBar;
   TScrollBar *vScrollBar;
   short numCols;
   int topItem;
   int focused;
   int range;

private:

#ifndef NO_TV_STREAMS
   virtual const char *streamableName() const
   { return name; }

protected:

   TListViewer(StreamableInit);
   virtual void write(opstream &);
   virtual void *read(ipstream &);

public:

   static const char *const name;
   static TStreamable *build();
#endif  // ifndef NO_TV_STREAMS

};

#ifndef NO_TV_STREAMS
inline ipstream &operator >> (ipstream &is, TListViewer &cl)
{ return is >> (TStreamable &)cl; }
inline ipstream &operator >> (ipstream &is, TListViewer *&cl)
{ return is >> (void *&)cl; }

inline opstream &operator << (opstream &os, TListViewer &cl)
{ return os << (TStreamable &)cl; }
inline opstream &operator << (opstream &os, TListViewer *cl)
{ return os << (TStreamable *)cl; }
#endif  // ifndef NO_TV_STREAMS

#endif  // Uses_TListViewer

#if defined( Uses_TGroup ) && !defined( __TGroup )
#define __TGroup

class TView;

class TGroup : public TView {

public:

   friend void genRefs();

   TGroup(const TRect &bounds);
   ~TGroup();

   virtual void shutDown();

   ushort execView(TView *p);
   virtual ushort execute();
   virtual void awaken();

   void insertView(TView *p, TView *Target);
   void remove(TView *p);
   void removeView(TView *p);
   void resetCurrent();
   void setCurrent(TView *p, selectMode mode);
   void selectNext(Boolean forwards);
   TView *firstThat(Boolean(*func)(TView *, void *), void *args);
   Boolean focusNext(Boolean forwards);
   void forEach(void (*func)(TView *, void *), void *args);
   void insert(TView *p);
   void insertBefore(TView *p, TView *Target);
   TView *current;
   TView *at(short index);
   TView *firstMatch(ushort aState, ushort aOptions);
   short indexOf(TView *p);
   Boolean matches(TView *p);
   TView *first();

   virtual void setState(ushort aState, Boolean enable);

   virtual void handleEvent(TEvent &event);

   void drawSubViews(TView *p, TView *bottom);

   virtual void changeBounds(const TRect &bounds);

   virtual size_t dataSize();
   virtual void getData(void *rec);
   virtual void setData(void *rec);

   virtual void draw();
   void redraw();
   void lock();
   void unlock();
   virtual void resetCursor();

   virtual void endModal(ushort command);

   virtual void eventError(TEvent &event);

   virtual ushort getHelpCtx();

   virtual Boolean valid(ushort command);

   void freeBuffer();
   void getBuffer();

   TView *last;

   TRect clip;
   phaseType phase;

   ushort *buffer;
   uchar lockFlag;
   ushort endState;

private:

   Boolean invalid(TView *p, ushort command);
   void focusView(TView *p, Boolean enable);
   void selectView(TView *p, Boolean enable);
   TView *findNext(Boolean forwards);

#ifndef NO_TV_STREAMS
   virtual const char *streamableName() const
   { return name; }

protected:

   TGroup(StreamableInit);
   virtual void write(opstream &);
   virtual void *read(ipstream &);

public:

   static const char *const name;
   static TStreamable *build();
#endif  // ifndef NO_TV_STREAMS

};

#ifndef NO_TV_STREAMS
inline ipstream &operator >> (ipstream &is, TGroup &cl)
{ return is >> (TStreamable &)cl; }
inline ipstream &operator >> (ipstream &is, TGroup *&cl)
{ return is >> (void *&)cl; }

inline opstream &operator << (opstream &os, TGroup &cl)
{ return os << (TStreamable &)cl; }
inline opstream &operator << (opstream &os, TGroup *cl)
{ return os << (TStreamable *)cl; }
#endif  // ifndef NO_TV_STREAMS

#endif  // Uses_TGroup

#if defined( Uses_TWindow ) && !defined( __TWindow )
#define __TWindow

#define cpBlueWindow "\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F"
#define cpCyanWindow "\x10\x11\x12\x13\x14\x15\x16\x17"
#define cpGrayWindow "\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F"

class TFrame;
class TRect;
class TPoint;
class TEvent;
class TFrame;
class TScrollBar;

class TWindowInit {

public:

   TWindowInit(TFrame *(*cFrame)(TRect));

protected:

   TFrame *(*createFrame)(TRect);

};

/* ---------------------------------------------------------------------- */
/*      class TWindow                                                     */
/*                                                                        */
/*      Palette layout                                                    */
/*        1 = Frame passive                                               */
/*        2 = Frame active                                                */
/*        3 = Frame icon                                                  */
/*        4 = ScrollBar page area                                         */
/*        5 = ScrollBar controls                                          */
/*        6 = Scroller normal text                                        */
/*        7 = Scroller selected text                                      */
/*        8 = Reserved                                                    */
/* ---------------------------------------------------------------------- */

class TWindow: public TGroup, public virtual TWindowInit {

public:

   TWindow(const TRect &bounds,
           const char *aTitle,
           short aNumber
          );
   ~TWindow();

   virtual void close();
   virtual TPalette &getPalette() const;
   virtual const char *getTitle(short maxSize);
   virtual void handleEvent(TEvent &event);
   static TFrame *initFrame(TRect);
   virtual void setState(ushort aState, Boolean enable);
   virtual void sizeLimits(TPoint &min, TPoint &max);
   TScrollBar *standardScrollBar(ushort aOptions);
   virtual void zoom();
   virtual void shutDown();

   uchar flags;
   TRect zoomRect;
   short number;
   short palette;
   TFrame *frame;
   const char *title;

private:

#ifndef NO_TV_STREAMS
   virtual const char *streamableName() const
   { return name; }

protected:

   TWindow(StreamableInit);
   virtual void write(opstream &);
   virtual void *read(ipstream &);

public:

   static const char *const name;
   static TStreamable *build();
#endif  // ifndef NO_TV_STREAMS

};

#ifndef NO_TV_STREAMS
inline ipstream &operator >> (ipstream &is, TWindow &cl)
{ return is >> (TStreamable &)cl; }
inline ipstream &operator >> (ipstream &is, TWindow *&cl)
{ return is >> (void *&)cl; }

inline opstream &operator << (opstream &os, TWindow &cl)
{ return os << (TStreamable &)cl; }
inline opstream &operator << (opstream &os, TWindow *cl)
{ return os << (TStreamable *)cl; }
#endif  // ifndef NO_TV_STREAMS

#endif  // Uses_TWindow

#include <tvvo2.h>

