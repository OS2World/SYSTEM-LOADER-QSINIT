/* ------------------------------------------------------------------------
   TCombo.H

   defines the classes TNoCaseStringCollection, and TCombo
 ------------------------------------------------------------------------*/

#if !defined( __TNoCaseStringCollection )
#define __TNoCaseStringCollection

class TNoCaseStringCollection : public TStringCollection {
public:
   TNoCaseStringCollection( short aLimit, short aDelta );

private:
   virtual int compare( void *key1, void *key2 );

#ifndef NO_TV_STREAMS
   virtual const char *streamableName() const { return name; }
protected:
   TNoCaseStringCollection( StreamableInit ) :
              TStringCollection ( streamableInit ) {};
public:
   static const char * const near name;
   static TStreamable *build();
#endif
};

#ifndef NO_TV_STREAMS
inline ipstream& operator >> ( ipstream& is, TNoCaseStringCollection& cl )
    { return is >> (TStreamable&)cl; }
inline ipstream& operator >> ( ipstream& is, TNoCaseStringCollection*& cl )
    { return is >> (void *&)cl; }

inline opstream& operator << ( opstream& os, TNoCaseStringCollection& cl )
    { return os << (TStreamable&)cl; }
inline opstream& operator << ( opstream& os, TNoCaseStringCollection* cl )
    { return os << (TStreamable *)cl; }
#endif

#endif  // TNoCaseStringCollection

#if defined Uses_TCombo && !defined( __TCombo )
#define __TCombo

//flags for TCombo constructor

const int
  cbxOnlyList = 1,       //Only items in list may be entered
  cbxDisposesList = 2,   //TCombo responsible for saving and disposing
  cbxNoTransfer = 4;     //Disables transfe}

class far TRect;
class far TEvent;
class far TDialog;

class TCombo : public TView {
   friend class TStringListBox;
public:
   TCombo( const TRect& bounds, TInputLine *aLink, ushort aFlags,
                    TSItem *aStrings = 0);
   ~TCombo();
   virtual void activateChar(char ch);
   virtual size_t dataSize();
   virtual void getData( void *rec );
   virtual void setData( void *rec );
   virtual void newList(TSortedCollection *aList);
   virtual void handleEvent(TEvent& event);
   virtual void draw(void);
   virtual TPalette& getPalette() const;
private:
   void popup();
#ifndef NO_TV_STREAMS
   virtual const char *streamableName() const { return name; }
#endif
protected:
   ushort flags;
   ushort activateCode; //keycode to activate drop down list
   char showChar;       //the character drawn  #25, #31, or '*'
   TInputLine *iLink;
   TSortedCollection *comboList;

   virtual Boolean equal(const char* s1, const char* s2, size_t maxlen);
   virtual TListBox* initListBox(const TRect& R, TScrollBar *PSB);
   virtual TDialog* makeDialog(TListBox*& PLB);
   virtual void incrementalSearch(TEvent& event);
   virtual void update(short item);
   virtual void putString(char* s);

#ifndef NO_TV_STREAMS
   TCombo( StreamableInit );
   virtual void write( opstream& );
   virtual void *read( ipstream& );
public:
   static const char * const near name;
   static TStreamable *build();
#endif
};

#ifndef NO_TV_STREAMS
inline ipstream& operator >> ( ipstream& is, TCombo& cl )
    { return is >> (TStreamable&)cl; }
inline ipstream& operator >> ( ipstream& is, TCombo*& cl )
    { return is >> (void *&)cl; }

inline opstream& operator << ( opstream& os, TCombo& cl )
    { return os << (TStreamable&)cl; }
inline opstream& operator << ( opstream& os, TCombo* cl )
    { return os << (TStreamable *)cl; }
#endif                                            

//TListDialog is a popup window holder for a Listbox.  It is used by TCombo

class TListDialog : public TDialog {
public:
   TListDialog( const TRect& bounds);
   virtual TPalette& getPalette() const;
   virtual void handleEvent(TEvent& event);
   virtual void sizeLimits(TPoint& min, TPoint& max);
protected:
   short width;
};

/* TStringListBox is a listbox holding a sorted collection of strings.
   Incremental search is implemented. */

class TStringListBox : public TListBox {
public:
   TStringListBox( const TRect& bounds, ushort aNumCols,
          TScrollBar *aScrollBar, TCombo* aCombo);
   virtual void handleEvent(TEvent& event);
   virtual void newList(TCollection *aList);
protected:
   ushort searchPos;
   TCombo* myCombo;
};

#endif  // Uses_TCombo
