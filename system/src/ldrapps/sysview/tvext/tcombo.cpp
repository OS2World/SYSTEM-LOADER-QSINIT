/*---------------------------------------------------------------------------
                           TCombo

TCombo is a control which may be attached to any TInputLine to
provide a drop down list of possible entries for the TInputLine.  In
action (and default appearance) it is similar to the THistory control
but differs in that the list of entries is supplied by the program rather
that from past entries to the TInputLine.

Incremental search is implemented in TCombo so that possible entries may be
selected by typing only the first 1 or 2 characters of the entry.  Selections
may also be made by double clicking with the mouse or by hiliting the entry
and pressing the Enter key.

TCombo may be attached to any TInputLine or descendent of TInputLine.  In
addition, the TInputLine may have a validator or even a THistory attached
(not sure that makes any sense).  Some variation in TCombo's appearance
can be achieved by changing its width and display character.  The activation
character (the keyboard character activating the drop down list) may also
be changed.

A flags field allows variations in TCombo's action.

  cbxOnlyList
    If this bit is set, only items in the list are acceptable by the
    TInputLine.  In this mode, incremental search is implemented even when
    the drop down listbox is closed.  Or the space bar will allow toggling
    through the list.  (Shift space bar toggles in the reverse direction.)

    If this bit is not set, the TInputLine will accept any entry even if it
    is not in the list.

  cbxDisposesList
    If this bit is set, TCombo will dispose of the list in it's
    destructor effectively disposing of it when the dialog is closed.

  cbxNoTransfer
    If this bit is set, the transfer mechanism is disabled and dataSize()
    returns 0.  This bit would probably be set if the list is fixed and
    defined at the time TCombo is constructed.

    If this bit is not set, the dialog transfer mechanism transfers a
    pointer to the desired list.  dataSize() returns sizeof(Pointer).


Fields:

    ushort flags;         //a combination of the cbxXXXX constants
    ushort activateCode;  //keycode to activate the drop down list,
                          // default is kbDown
    char showChar;        //the character drawn. Default is 25, a thin arrow
    TInputLine* iLink;    //points to the TInputLine
    TSortedCollection* comboList;  //Points to the drop down list

Member functions:

TCombo::TCombo( const TRect& bounds, TInputLine* aLink, ushort aFlags,
                     TSItem* aStrings)  :
    bounds should have a height of 1 and a width of 1 or 3.  aLink points
    to the owning TInputLine, and aFlags is the desired combination of the
    cbxXXXX constants.

    The aStrings parameter allows you to define the list when the TCombo
    is constructed.  It consists of a series of calls to new TSItem() in the
    same manner you would use to define the labels for TCheckBoxes or
    TRadioButtons.  (See the documentation for TCluster.)  If you're
    planning on defining the list later either through the dialog transfer
    mechanism or by using newList(), set aStrings to Nil.

    Constructs a TCombo, setting activateCode to kbDown, showChar to
    #25, and comboList to the list of strings defined by aStrings.

TCombo::~TCombo()
    If cbxDisposesList is set, disposes of comboList.

void TCombo::activateChar(char ch)
    Allows changes to showChar, the character displayed.  If ch is 25 or
    31 (a down arrow) then activateCode is set to kbDown otherwise
    activatCode is set to ch.

ushort TCombo::dataSize()
void TCombo::getData(void* rec)
void TCombo::setData(void* rec)
    The transfer methods.  If cbxNoTransfer is not set, dataSize() returns
    sizeof(comboList) and rec should be a pointer to a TNoCaseStringCollection.
    If cbxNoTransfer is set, then dataSize() returns 0 and getData(), setData()
    do nothing.

void TCombo::newList(TSortedCollection* aList)
    Disposes of the current comboList if it exists and assigns aList
    to comboList.

void TCombo::update(short )
    update() is called whenever a selection is made in the combobox.
    TCombo::update() does nothing but you may need to override this method
    in cases where another control needs updating when a selection is made.

The following methods may require overriding for lists other than
TNoCaseStringCollection:

TListBox* TCombo::initListBox(const TRect& R, TScrollBar *PSB)
    Initializes a TStringListBox for TCombo.

Boolean TCombo::equal(const char* s1, const char* s2, size_t maxlen)
    Returns True if strings s1 and s2 match (without regards to case)
    for the first maxlen Characters.


Setting up a TCombo -- an example

TInputLine *myInputLine;
TCombo *combo;
TDialog* dlg;

 ..............

myInputLine = new TInputLine(TRect(7, 3, 25, 4), 17);
dlg->insert(myInputLine);

  combo = new TCombo(TRect(25, 3, 26, 4), (TInputLine*)myInputLine,
      cbxDisposesList | cbxNoTransfer,
      // build a list to display
      new TSItem("cmCancel",
      new TSItem("cmOK",
      new TSItem("cmYes", 0))));

  //change the displayed character from default thin arrow to thick arrow
  combo->activateChar(31);
  dlg->insert(combo);

---------------------------------------------------------------------------*/

#define Uses_TCombo
#define Uses_TDialog
#define Uses_TInputLine
#define Uses_TListBox
#define Uses_TScrollBar
#define Uses_TSortedCollection
#define Uses_TStringCollection
#define Uses_TView
#define Uses_TEvent
#define Uses_TProgram
#define Uses_TPalette
#define Uses_THistory
#define Uses_TSItem
#ifndef NO_TV_STREAMS
#define Uses_opstream
#define Uses_ipstream
#endif

#if defined (TV2)
#   include <tvision\tv.h>
#   include <tvision\tkeys.h>
#elif defined (TV1)
#   include <tv.h>
#   include <tkeys.h>
#else
#   error TV1 or TV2 must be defined
#endif

#if !defined( __STRING_H )
#include <string.h>
#endif

#include <ctype.h>

#include "tcombo.h"

#define cpCombo "\x16\x17"

TCombo::TCombo( const TRect& bounds, TInputLine *aLink, ushort aFlags,
   TSItem *aStrings) : flags(aFlags), iLink(aLink), TView(bounds)
{
   showChar = 25;   //a thin arrow
   activateCode = kbDown;  // a down arrow
   options |= ofPreProcess;
   eventMask |= evBroadcast;
   comboList = new TNoCaseStringCollection(3,3);
   while (aStrings) {
      TSItem *p = aStrings;
      comboList->insert(newStr(aStrings->value));
      aStrings = aStrings->next;
      delete p;
   }
}

TCombo::~TCombo() {
   if (comboList && (flags & cbxDisposesList)) destroy(comboList);
}

void TCombo::activateChar(char ch) {
   showChar = ch;
   if (ch == 25 || ch == 31) activateCode = kbDown; else activateCode = ch;
}

size_t TCombo::dataSize() {
   if (flags & cbxNoTransfer) return 0; else
      return sizeof(comboList);
}

void TCombo::getData(void *rec) {
   if (!(flags & cbxNoTransfer)) *(TSortedCollection**)rec = comboList;
}

void TCombo::setData(void* rec) {
   if (!(flags & cbxNoTransfer)) newList(*(TSortedCollection**)rec);
}

void TCombo::newList(TSortedCollection* aList) {
   if (comboList) destroy(comboList);
   comboList = aList;
   if (aList && (flags & cbxOnlyList)) putString((char*)(aList->at(0)));
}

void TCombo::putString(char* s) {
   //puts the string to the TInputLine without overwriting any memory
   memcpy( iLink->data, s, iLink->maxLen );
   iLink->data[iLink->maxLen] = EOS;
   iLink->drawView();
}

Boolean TCombo::equal(const char* s1, const char* s2, size_t maxlen) {
   //caseless compare two strings for maxLen characters
   return Boolean(strnicmp(s1, s2, maxlen) == 0);
}

void TCombo::incrementalSearch(TEvent& event) {
   char curString[256], newString[256];
   short searchPos, oldPos;
   ccIndex oldValue, value;

   if (comboList)
      if ((event.keyDown.keyCode != 0) || (event.keyDown.keyCode == kbLeft)
         || (event.keyDown.keyCode == kbHome))
      {
         strcpy(curString, iLink->data);
         searchPos = iLink->curPos;
         oldPos = searchPos;
         if ((event.keyDown.keyCode==kbBack) || (event.keyDown.keyCode==kbLeft)
            || (event.keyDown.keyCode==kbHome))
         {
            if (searchPos == 0) return;
            if (event.keyDown.keyCode == kbHome)  searchPos = 0;
               else searchPos--;
            curString[searchPos] = '\0';
         } else {
            curString[searchPos++] = event.keyDown.charScan.charCode;
            curString[searchPos] = '\0';
         }
         if (!comboList->search(iLink->data, oldValue)) oldValue = -1;
         comboList->search(curString, value);

         if (value < comboList->getCount()) {
            strcpy(newString, (char*)(comboList->at(value)));
            if (equal(newString, curString, searchPos)) {
               if (value != oldValue) {
                  putString(newString);
                  iLink->selectAll(False);
                  iLink->setCursor(1+searchPos, 0);
               } else
                  iLink->setCursor(iLink->cursor.x+(searchPos-oldPos), iLink->cursor.y);
            } else
               searchPos = oldPos;
         } else
            searchPos = oldPos;
         iLink->curPos = searchPos;
      }
   clearEvent(event);
}

void TCombo::popup() {
   TListBox *PLB;
   TView *dlg = TProgram::application->validView((TListDialog*)makeDialog(PLB));
   if (dlg) {
      ccIndex item;
      if (comboList) {
         if (!comboList->search(iLink->data, item)) item = 0;
      } else
         item = 0;
      PLB->focusItem(item);
      ushort command = owner->execView(dlg);
      if (comboList && command == cmOK) {
         item = PLB->focused;
         PLB->getText(iLink->data, item, iLink->maxLen);
         iLink->selectAll(False);    //nothing selected, drawview
         update(item);
      }
      destroy(dlg);
   }
}

void TCombo::handleEvent(TEvent& event) {
   if (owner->current == (TView*)iLink && event.what == evKeyDown)
   {  //PreProcess of chars going to our TInputLine
      //the active key may be either a character key or a special key with
      // charcode = 0}
      if ((activateCode!=0 && event.keyDown.charScan.charCode==activateCode)
         || event.keyDown.keyCode==activateCode)
      {
         popup();    //activate by key
         clearEvent(event);
      } else
      if ((flags & cbxOnlyList)!=0) {
         // cbxOnlyList is set
         event.keyDown.keyCode = ctrlToArrow(event.keyDown.keyCode);
         char ch = event.keyDown.charScan.charCode;
         if (ch == ' ') {
            // toggle to next or previous item in list
            if (comboList) {
               ccIndex value;
               if (!comboList->search(iLink->data, value))  value = -1;
#ifdef TV2
               if (event.keyDown.controlKeyState & (kbRightShift | kbLeftShift))
                  value--; else value++;
#else
               unsigned char shiftState = getShiftState();
               if (shiftState & (kbRightShift | kbLeftShift))
                  value--; else value++;
#endif
               if (value < 0)  value = comboList->getCount()-1;
                  else
               if (value >= comboList->getCount()) value = 0;
               putString((char*)(comboList->at(value)));
               iLink->selectAll(False);
            }
            clearEvent(event);
         } else
         if (ch >= ' ' && (unsigned)ch <= 255) incrementalSearch(event);
            else
         switch (event.keyDown.keyCode) {
            case kbBack:
            case kbLeft:
            case kbHome: incrementalSearch(event); break;
            case kbDel :
            case kbEnd :
            case kbIns :
            case kbRight: clearEvent(event); break;
         }
      }
   } else
   if (event.what == evMouseDown && mouseInView(event.mouse.where)) {
      // activate by mouse
      iLink->select();
      popup();
      clearEvent(event);
   }
   TView::handleEvent(event);

   if (flags & cbxOnlyList && event.what == evBroadcast &&
      event.message.command == cmReceivedFocus &&
         event.message.infoPtr == iLink)
            iLink->selectAll(False);  //cursor at start, no hilite
}

void TCombo::update(short) { }

TListBox* TCombo::initListBox(const TRect& R, TScrollBar *PSB) {
   TStringListBox* rslt = new TStringListBox(R, 1, PSB, this);
   rslt->growMode = gfGrowHiX | gfGrowHiY;
   rslt->newList(comboList);
   return rslt;
}

TDialog* TCombo::makeDialog(TListBox*& PLB) {
   TRect r, r1;
   short ht;

   r = iLink->getBounds();  //get TInputLine's bounds
   r.a.x--;                 //figure bounds of popup dialog
   r.b.x += 2;
   if (comboList)
      // ht = no less than3, no more than 9
      ht = min(9, max(comboList->getCount()+2, 3));
   else ht = 3;
   r.b.y = r.b.y + ht;
   r1 = iLink->owner->getExtent();
   if (r.b.y >= r1.b.y)    //check to see if bottom is in window
      r.b.y = r1.b.y - 1;
   r.a.y = r.b.y - ht;           //find top
   if (r.a.y <= r1.a.y)    //see if top is too high
      r.a.y = r1.a.y+1;

   TDialog* dlg = new TListDialog(r);
   dlg->flags |= wfGrow;

   r = dlg->getExtent();
   r.grow(-1, -1);
   r1 = r;
   r.a.x = r.b.x-1;
   TScrollBar* scrollBar = new  TScrollBar(r);
   dlg->insert(scrollBar);

   r1.b.x--;
   PLB = initListBox(r1, scrollBar);
   dlg->insert(PLB);

   return dlg;
}

void TCombo::draw() {
   TDrawBuffer b;
   if (size.x == 1) writeChar(0, 0, showChar, 1, 1); else {
      char s[] = "\xDE~~~\xDD";
      s[2] = showChar;
      b.moveCStr(0, s, getColor(0x0102));
      writeLine(0, 0, size.x, size.y, b);
   }
}

TPalette& TCombo::getPalette() const {
   static TPalette palette( cpCombo, sizeof( cpCombo)-1 );
   return palette;
}

#ifndef NO_TV_STREAMS
void TCombo::write(opstream& os)
{
   TView::write( os );
   os << flags << activateCode << showChar << iLink << comboList;
}

void *TCombo::read(ipstream& is) {
   TView::read( is );
   is >> flags >> activateCode >> showChar >> iLink >> comboList;
   return this;
}

TStreamable *TCombo::build() {
   return new TCombo( streamableInit );
}

TCombo::TCombo( StreamableInit ) : TView( streamableInit ) { }

const char * const near TCombo::name = "TCombo";
#endif

TStringListBox::TStringListBox(const TRect& bounds, ushort aNumCols,
   TScrollBar *aScrollBar, TCombo* aCombo) :
   TListBox(bounds, aNumCols, aScrollBar), searchPos(0), myCombo(aCombo)
{
   showCursor();
   setCursor(1,0);
}

void TStringListBox::newList(TCollection* aList) {
   TListBox::newList(aList);
   searchPos = 0;
}

void TStringListBox::handleEvent(TEvent& event) {
#ifdef TV2
   if (event.what == evMouseDown && (event.mouse.eventFlags & meDoubleClick) != 0)
#else
   if (event.what == evMouseDown && event.mouse.doubleClick)
#endif
   {
      event.what = evCommand;
      event.message.command = cmOK;
      putEvent(event);
      clearEvent(event);
   } else {
      short oldValue = focused;
      if (event.what == evKeyDown)
         switch (ctrlToArrow(event.keyDown.keyCode)) {
            case kbUp:
            case kbDown:
            case kbPgDn:
            case kbPgUp:
            case kbHome:
            case kbEnd:
            case kbCtrlPgDn:
            case kbCtrlPgUp: searchPos = 0; break;
            case kbEsc : return;
         }
      if (event.what != evKeyDown || event.keyDown.charScan.charCode != ' ')
         TListBox::handleEvent(event);
      if (oldValue != focused) searchPos = 0;

      if (list() && event.what == evKeyDown) {
         if (event.keyDown.charScan.charCode != 0 || event.keyDown.keyCode == kbLeft)
         {
            ccIndex value = focused;
            char curString[256];
            if (value < range) getText(curString, value, 255);
               else curString[0] = '\0';

            short oldPos = searchPos;

            if (event.keyDown.keyCode == kbBack || event.keyDown.keyCode == kbLeft)
            {
               if (searchPos == 0) return;
               searchPos--;
               curString[searchPos] = '\0';
            } else {
               curString[searchPos++] = event.keyDown.charScan.charCode;
               curString[searchPos] = '\0';
            }
            ((TSortedCollection*)list())->search(curString, value);
            if (value < range) {
               char newString[256];
               getText(newString, value, 255);

               if (myCombo->equal(newString, curString, searchPos)) {
                  if (value != oldValue) {
                     focusItem(value);
                     /* Assumes ListControl will set the cursor to the
                        first character of the sfFocused item */
                     setCursor(cursor.x+searchPos, cursor.y);
                  } else
                     setCursor(cursor.x+(searchPos-oldPos), cursor.y);
               } else
                  searchPos = oldPos;
            } else
               searchPos = oldPos;
            clearEvent(event);
         }
      }
   }
}

TListDialog::TListDialog(const TRect& bounds) : TDialog(bounds, 0),
   TWindowInit(TDialog::initFrame)
{
   width = bounds.b.x - bounds.a.x;
}

TPalette& TListDialog::getPalette() const {
   static TPalette palette( 0, 0 );
   return palette;
}

void TListDialog::handleEvent(TEvent& event) {
   if (event.what == evKeyDown && event.keyDown.charScan.charCode == 13) {
      endModal(cmOK);
      clearEvent(event);
   }
   TDialog::handleEvent(event);
}

void TListDialog::sizeLimits(TPoint& min, TPoint& max) {
   TDialog::sizeLimits(min, max);
   if (width < min.x) min.x = width;
   min.y = 3;
}
