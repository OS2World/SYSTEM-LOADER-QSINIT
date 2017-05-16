/* ------------------------------------------------------------------------*/
/*                                                                         */
/*   COLORSEL.H                                                            */
/*                                                                         */
/*   defines the class TColorDialog, used to set application palettes      */
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

#if !defined( __COLOR_COMMAND_CODES )
#define __COLOR_COMMAND_CODES

const int
  cmColorForegroundChanged = 71,
  cmColorBackgroundChanged = 72,
  cmColorSet               = 73,
  cmNewColorItem           = 74,
  cmNewColorIndex          = 75,
  cmSaveColorIndex         = 76;

#endif  // __COLOR_COMMAND_CODES

class TColorItem;
class TColorGroup;

TColorItem &operator + (TColorItem &i1, TColorItem &i2);
TColorGroup &operator + (TColorGroup &g, TColorItem &i);
TColorGroup &operator + (TColorGroup &g1, TColorGroup &g2);

#if defined( Uses_TColorItem ) && !defined( __TColorItem )
#define __TColorItem

class TColorGroup;

class TColorItem {

public:

   TColorItem(const char *nm, uchar idx, TColorItem *nxt = 0);
   virtual ~TColorItem();
   const char *name;
   uchar index;
   TColorItem *next;
   friend TColorGroup &operator + (TColorGroup &, TColorItem &);
   friend TColorItem &operator + (TColorItem &i1, TColorItem &i2);

};

#endif  // Uses_TColorItem

#if defined( Uses_TColorGroup ) && !defined( __TColorGroup )
#define __TColorGroup

class TColorItem;

class TColorGroup {

public:

   TColorGroup(const char *nm, TColorItem *itm = 0, TColorGroup *nxt = 0);
   virtual ~TColorGroup();
   const char *name;
   uchar index;
   TColorItem *items;
   TColorGroup *next;
   friend TColorGroup &operator + (TColorGroup &, TColorItem &);
   friend TColorGroup &operator + (TColorGroup &g1, TColorGroup &g2);


};

class TColorIndex {

public:
    uchar groupIndex;
    uchar colorSize;
    uchar colorIndex[256];
};

#endif  // Uses_TColorGroup

#if defined( Uses_TColorSelector ) && !defined( __TColorSelector )
#define __TColorSelector

class TRect;
class TEvent;

class TColorSelector : public TView {

public:

   enum ColorSel { csBackground, csForeground };

   TColorSelector(const TRect &Bounds, ColorSel ASelType);
   virtual void draw();
   virtual void handleEvent(TEvent &event);

protected:

   uchar color;
   ColorSel selType;

private:

   void colorChanged();

   static const char icon;

#ifndef NO_TV_STREAMS
   virtual const char *streamableName() const
   { return name; }

protected:

   TColorSelector(StreamableInit);
   virtual void write(opstream &);
   virtual void *read(ipstream &);

public:

   static const char *const name;
   static TStreamable *build();
#endif  // ifndef NO_TV_STREAMS

};

#ifndef NO_TV_STREAMS
inline ipstream &operator >> (ipstream &is, TColorSelector &cl)
{ return is >> (TStreamable &)cl; }
inline ipstream &operator >> (ipstream &is, TColorSelector *&cl)
{ return is >> (void *&)cl; }

inline opstream &operator << (opstream &os, TColorSelector &cl)
{ return os << (TStreamable &)cl; }
inline opstream &operator << (opstream &os, TColorSelector *cl)
{ return os << (TStreamable *)cl; }
#endif  // ifndef NO_TV_STREAMS

#endif  // Uses_TColorSelector


#if defined( Uses_TMonoSelector ) && !defined( __TMonoSelector )
#define __TMonoSelector

class TRect;
class TEvent;

class TMonoSelector : public TCluster {

public:

   TMonoSelector(const TRect &bounds);
   virtual void draw();
   virtual void handleEvent(TEvent &event);
   virtual Boolean mark(int item);
   void newColor();
   virtual void press(int item);
   void movedTo(int item);

private:

   static const char *button;
   static const char *normal;
   static const char *highlight;
   static const char *underline;
   static const char *inverse;

#ifndef NO_TV_STREAMS
   virtual const char *streamableName() const
   { return name; }

protected:

   TMonoSelector(StreamableInit);

public:

   static const char *const name;
   static TStreamable *build();
#endif  // ifndef NO_TV_STREAMS

};

#ifndef NO_TV_STREAMS
inline ipstream &operator >> (ipstream &is, TMonoSelector &cl)
{ return is >> (TStreamable &)cl; }
inline ipstream &operator >> (ipstream &is, TMonoSelector *&cl)
{ return is >> (void *&)cl; }

inline opstream &operator << (opstream &os, TMonoSelector &cl)
{ return os << (TStreamable &)cl; }
inline opstream &operator << (opstream &os, TMonoSelector *cl)
{ return os << (TStreamable *)cl; }
#endif  // ifndef NO_TV_STREAMS

#endif  // Uses_TMonoSelector

#if defined( Uses_TColorDisplay ) && !defined( __TColorDisplay )
#define __TColorDisplay

class TRect;
class TEvent;

class TColorDisplay : public TView {

public:

   TColorDisplay(const TRect &bounds, const char *aText);
   virtual ~TColorDisplay();
   virtual void draw();
   virtual void handleEvent(TEvent &event);
   virtual void setColor(uchar *aColor);

protected:

   uchar *color;
   const char *text;

private:

#ifndef NO_TV_STREAMS
   virtual const char *streamableName() const
   { return name; }

protected:

   TColorDisplay(StreamableInit);
   virtual void write(opstream &);
   virtual void *read(ipstream &);

public:

   static const char *const name;
   static TStreamable *build();
#endif  // ifndef NO_TV_STREAMS

};

#ifndef NO_TV_STREAMS
inline ipstream &operator >> (ipstream &is, TColorDisplay &cl)
{ return is >> (TStreamable &)cl; }
inline ipstream &operator >> (ipstream &is, TColorDisplay *&cl)
{ return is >> (void *&)cl; }

inline opstream &operator << (opstream &os, TColorDisplay &cl)
{ return os << (TStreamable &)cl; }
inline opstream &operator << (opstream &os, TColorDisplay *cl)
{ return os << (TStreamable *)cl; }
#endif  // ifndef NO_TV_STREAMS

#endif  // Uses_TColorDisplay


#if defined( Uses_TColorGroupList ) && !defined( __TColorGroupList )
#define __TColorGroupList

class TRect;
class TScrollBar;
class TColorGroup;
class TColorItem;

class TColorGroupList : public TListViewer {

public:

   TColorGroupList(const TRect &bounds,
                   TScrollBar *aScrollBar,
                   TColorGroup *aGroups
                  );
   virtual ~TColorGroupList();
   virtual void focusItem(int item);
   virtual void getText(char *dest, int item, int maxLen);

   virtual void handleEvent(TEvent&);

   void setGroupIndex(uchar groupNum, uchar itemNum);
   TColorGroup* getGroup(uchar groupNum);
   uchar getGroupIndex(uchar groupNum);
   uchar getNumGroups();
protected:

   TColorGroup *groups;

private:

#ifndef NO_TV_STREAMS
   virtual const char *streamableName() const
   { return name; }
   static void writeItems(opstream &, TColorItem *);
   static void writeGroups(opstream &, TColorGroup *);
   static TColorItem *readItems(ipstream &);
   static TColorGroup *readGroups(ipstream &);

protected:

   TColorGroupList(StreamableInit);
   virtual void write(opstream &);
   virtual void *read(ipstream &);

public:

   static const char *const name;
   static TStreamable *build();
#endif  // ifndef NO_TV_STREAMS

};

#ifndef NO_TV_STREAMS
inline ipstream &operator >> (ipstream &is, TColorGroupList &cl)
{ return is >> (TStreamable &)cl; }
inline ipstream &operator >> (ipstream &is, TColorGroupList *&cl)
{ return is >> (void *&)cl; }

inline opstream &operator << (opstream &os, TColorGroupList &cl)
{ return os << (TStreamable &)cl; }
inline opstream &operator << (opstream &os, TColorGroupList *cl)
{ return os << (TStreamable *)cl; }
#endif  // ifndef NO_TV_STREAMS

#endif  // Uses_TColorGroupList


#if defined( Uses_TColorItemList ) && !defined( __TColorItemList )
#define __TColorItemList

class TRect;
class TScrollBar;
class TColorItem;
class TEvent;

class TColorItemList : public TListViewer {

public:

   TColorItemList(const TRect &bounds,
                  TScrollBar *aScrollBar,
                  TColorItem *aItems
                 );
   virtual void focusItem(int item);
   virtual void getText(char *dest, int item, int maxLen);
   virtual void handleEvent(TEvent &event);

protected:

   TColorItem *items;

private:

#ifndef NO_TV_STREAMS
   virtual const char *streamableName() const
   { return name; }

protected:

   TColorItemList(StreamableInit);

public:

   static const char *const name;
   static TStreamable *build();
#endif  // ifndef NO_TV_STREAMS

};

#ifndef NO_TV_STREAMS
inline ipstream &operator >> (ipstream &is, TColorItemList &cl)
{ return is >> (TStreamable &)cl; }
inline ipstream &operator >> (ipstream &is, TColorItemList *&cl)
{ return is >> (void *&)cl; }

inline opstream &operator << (opstream &os, TColorItemList &cl)
{ return os << (TStreamable &)cl; }
inline opstream &operator << (opstream &os, TColorItemList *cl)
{ return os << (TStreamable *)cl; }
#endif  // ifndef NO_TV_STREAMS

#endif  // Uses_TColorItemList


#if defined( Uses_TColorDialog ) && !defined( __TColorDialog )
#define __TColorDialog

class TColorGroup;
class TEvent;
class TColorDisplay;
class TColorGroupList;
class TLabel;
class TColorSelector;
class TMonoSelector;
class TPalette;

class TColorDialog : public TDialog {

public:

   TColorDialog(TPalette *aPalette, TColorGroup *aGroups);
   ~TColorDialog();
   virtual size_t dataSize();
   virtual void getData(void *rec);
   virtual void handleEvent(TEvent &event);
   virtual void setData(void *rec);

   TPalette *pal;

   void getIndexes(TColorIndex*&);
   void setIndexes(TColorIndex*&);
protected:

   TColorDisplay *display;
   TColorGroupList *groups;
   TLabel *forLabel;
   TColorSelector *forSel;
   TLabel *bakLabel;
   TColorSelector *bakSel;
   TLabel *monoLabel;
   TMonoSelector *monoSel;
   uchar groupIndex;

private:

   static const char *colors;
   static const char *groupText;
   static const char *itemText;
   static const char *forText;
   static const char *bakText;
   static const char *textText;
   static const char *colorText;
   static const char *okText;
   static const char *cancelText;

#ifndef NO_TV_STREAMS
   virtual const char *streamableName() const
   { return name; }

protected:

   TColorDialog(StreamableInit);
   virtual void write(opstream &);
   virtual void *read(ipstream &);

public:

   static const char *const name;
   static TStreamable *build();
#endif  // ifndef NO_TV_STREAMS

};

#ifndef NO_TV_STREAMS
inline ipstream &operator >> (ipstream &is, TColorDialog &cl)
{ return is >> (TStreamable &)cl; }
inline ipstream &operator >> (ipstream &is, TColorDialog *&cl)
{ return is >> (void *&)cl; }

inline opstream &operator << (opstream &os, TColorDialog &cl)
{ return os << (TStreamable &)cl; }
inline opstream &operator << (opstream &os, TColorDialog *cl)
{ return os << (TStreamable *)cl; }
#endif  // ifndef NO_TV_STREAMS

#endif  // TColorDialog

#include <tvvo2.h>

