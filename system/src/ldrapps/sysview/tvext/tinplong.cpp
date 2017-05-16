/*-----------------------------------------------------------------------
TInputLong is a derivitave of TInputLine designed to accept long integer
numeric input.  Since both the upper and lower limit of acceptable numeric
input can be set, TInputLong may be used for short integer or unsigned shorts
as well.  Option flag bits allow optional hex input and display.  A blank
field may optionally be rejected or interpreted as zero.

TInputLong will be of most interest to users of Turbo Vision 1.x.  If you're
using TVision version 2.0,  you may prefer using a TInputLine with a
TRangeValidator.

Data Members

ushort ilOptions;
   the options (see constructor discussion).

long lLim, uLim;
   the lower and upper limits.


Member functions

TInputLong( const TRect& bounds, int aMaxLen, long lowerLim,
            long upperLim, ushort flags, const char* labelName = 0 ) :

  Creats a TInputLong control.  Flags may be a combination of:

  ilHex = 1,          //will enable hex input with leading '0x'
  ilBlankEqZero = 2,  //No input (blank) will be interpreted as '0'
  ilDisplayHex = 4,   //Number displayed as hex when possible
  ilUlong = 8;        //Limits and data are actually ulong!

  The optional labelName adds an identifier to any range error message.
  Normally, this would be the same string as the field's label (minus
  the '~'s).


virtual void TInputLong::write( opstream& os );
virtual void *TInputLong::read( ipstream& is );
static TStreamable *build();
TInputLong( StreamableInit streamableInit);

virtual ushort dataSize();
virtual void getData( void *rec );
virtual void setData( void *rec );

  The transfer methods.  dataSize is sizeof(long) and rec should be
  the address of a long.


virtual Boolean rangeCheck();

  Returns True if the entered string evaluates to a number >= lowerLim and
  <= upperLim.


virtual void error();

  error is called when rangeCheck fails.  It displays a messagebox
  indicating the allowable range.  The optional labelName parameter in the
  constructor is worked into the message to help identify the field.


virtual void  handleEvent( TEvent& event );

  handleEvent() filters out characters which are not appropriate to numeric
  input.  Tab and Shift Tab cause a call to rangeCheck() and a call to error()
  if rangeCheck() returns false.  The input must be valid to Tab from the
  view.  There's no attempt made to stop moving to another view with the mouse.


virtual Boolean valid( ushort command );

  if TInputLine.valid() is true and Cmd is neither cmValid or cmCancel, valid
  then calls rangeCheck().  If rangeCheck() is false, then error() is called
  and valid() returns False.

------------------------------------------------------------------------*/

//#define Uses_TKeys
#define Uses_TInputLong
#define Uses_TInputLine
#define Uses_TDrawBuffer
#define Uses_TEvent
#define Uses_opstream
#define Uses_ipstream
#define Uses_MsgBox

#if defined (TV2)
#   include <tv.h>
#   include <tkeys.h>
#elif defined (TV1)
#   include <tv.h>
#   include <tkeys.h> 
#else
#   error TV1 or TV2 must be defined
#endif

#include <stdlib.h>
#include "tinplong.h"
#include <ctype.h>
#include <string.h>
#include <stdio.h>

char *TInputLong::formHexStr(char *s, long L) {
   if ((ilOptions & (ilUlong|ilPureHex))==0 && L < 0) {
      s[0] = '-';  s[1] = '0';  s[2] = 'x';
      sprintf(s+3, "%X", -L);
   } else {
      char *sp = s, fmt[8];
      if ((ilOptions & ilPureHex)==0) { *sp++ = '0';  *sp++ = 'x'; fmt[0] = 0; }
         else sprintf(fmt, "%%0%dX", maxLen);
      sprintf(sp, fmt[0]?fmt:"%X", L);
   }
   return s;
}

TInputLong::TInputLong(const TRect& bounds, int aMaxLen, long lowerLim,
                       long upperLim, ushort flags, const char* labelName) :
    TInputLine(bounds, aMaxLen),
    lLim (lowerLim),
    uLim(upperLim),
    ilOptions(flags),
    label(newStr(labelName))
{
   if (ilOptions & ilPureHex)
      ilOptions |= ilDisplayHex;
   if (ilOptions & ilDisplayHex)
      ilOptions |= ilHex;
   if (ilOptions & ilBlankEqZero)
      strcpy(data, "0");
}

TInputLong::~TInputLong() {
   if (label) delete label;
}

size_t TInputLong::dataSize() {
    return sizeof(long);
}

long TInputLong::getCurrentValue(char **eptr) {
   int radix = (ilOptions & ilHex) && (strchr(data,'X') || strchr(data,'x')) ||
               (ilOptions & ilPureHex) ? 16 : 10;
   long   rc = ilOptions & ilUlong? strtoul(data, eptr, radix) : 
                  strtol(data, eptr, radix);
   return rc;
}

void TInputLong::getData(void *rec) {
   if (data[0] == '\0' && (ilOptions & ilBlankEqZero)) strcpy(data, "0");
   *(long*)rec = getCurrentValue();
}

void  TInputLong::handleEvent(TEvent& event) {
   if (event.what == evKeyDown) {
      switch (event.keyDown.keyCode) {
         case kbTab:
         case kbShiftTab:
            if (!rangeCheck()) {
               error();
               selectAll(True);
               clearEvent(event);
            }
            break;
      }
   }
   if (event.keyDown.charScan.charCode) {
      int ch = toupper(event.keyDown.charScan.charCode);
      switch (ch) {
         case '-' : 
            if ((ilOptions & (ilUlong|ilPureHex)) ||
               (!(lLim < 0 && (curPos == 0 || selEnd > 0)))) clearEvent(event); 
            break;
         case 'X' : 
            if ((ilOptions & ilPureHex)==0 && ((ilOptions & ilHex) && 
               (curPos == 1 && data[0] == '0') || (curPos == 2 && 
                  data[0] == '-' && data[1] == '0'))); else clearEvent(event);
            break;
         case 'A' :
         case 'B' :
         case 'C' :
         case 'D' :
         case 'E' :
         case 'F' :
            if ((ilOptions & ilPureHex)==0 && !strchr(data, 'X') && 
               !strchr(data, 'x')) clearEvent(event);
            break;
         default : 
            if ((ch < '0' || ch > '9') && (ch < 1 || ch > 0x1B)) clearEvent(event);
            break;
      }
   }
   TInputLine::handleEvent(event);
}

void TInputLong::setData( void *rec ) {
   int ulv = ilOptions & ilUlong;
   char s[40];
   if (ulv) {
      unsigned long cvalue = *(unsigned long*)rec;
      if (cvalue > (unsigned long)uLim) cvalue = (unsigned long)uLim; else 
      if (cvalue < (unsigned long)lLim) cvalue = (unsigned long)lLim;  
      if (ilOptions & ilDisplayHex) formHexStr(s, cvalue);
         else ultoa(cvalue, s, 10);
   } else {
      long cvalue = *(long*)rec;
      if (cvalue > uLim) cvalue = uLim; else 
      if (cvalue < lLim) cvalue = lLim;
      if (ilOptions & ilDisplayHex) formHexStr(s, cvalue);
         else ltoa(cvalue, s, 10);
   }
   memcpy(data, s, maxLen);
   data[maxLen] = EOS;
   selectAll(True);
}

Boolean TInputLong::rangeCheck() {
   if (data[0] == '\0')
      if (ilOptions & ilBlankEqZero) strcpy(data, "0");
         else return False;
   char *endptr;
   long  cvalue = getCurrentValue(&endptr);

   if (*endptr) return False;

   return Boolean((ilOptions & ilUlong)==0 ? cvalue>=lLim && cvalue<=uLim :
      (unsigned long)cvalue>=(unsigned long)lLim && 
         (unsigned long)cvalue<=(unsigned long)uLim);
}

void TInputLong::error()
#define SIZE 60
{  char sl[40], su[40], s[SIZE], fmt[80];
   if (label == 0) s[0] = '\0'; else {
      strcpy(s, "\"");
      strncat(s, label, SIZE-5);
      strcat(s, "\"\n\x03");
   }
   int ulv = ilOptions & ilUlong,
       hex = ilOptions & ilHex;
   sprintf(fmt, "\x03""%%sValue not within range %%l%s%s to %%l%s%s",
      ulv?"u":"d", hex?"(%s)":"", ulv?"u":"d", hex?"(%s)":"");

   if (hex) { 
      ushort svopts = ilOptions;
      ilOptions &= ~ilPureHex;
      messageBox(mfError | mfOKButton, fmt, s, lLim, formHexStr(sl, lLim), 
         uLim, formHexStr(su, uLim));
      ilOptions = svopts;
   } else
      messageBox(mfError | mfOKButton, fmt, s, lLim, uLim);
}

Boolean TInputLong::valid(ushort command) {
   Boolean rslt = TInputLine::valid(command);
   if (rslt && (command != 0) && (command != cmCancel)) {
      rslt = rangeCheck();
      if (!rslt) {
         error();
         select();
         selectAll(True);
      }
   }
   return rslt;
}

#ifndef NO_TV_STREAMS
void TInputLong::write(opstream& os) {
   TInputLine::write( os );
   os << ilOptions << lLim << uLim;
   os.writeString(label);
}

void *TInputLong::read(ipstream& is) {
   TInputLine::read( is );
   is >> ilOptions >> lLim >> uLim;
   label = is.readString();
   return this;
}

TStreamable *TInputLong::build() {
   return new TInputLong( streamableInit );
}

TInputLong::TInputLong(StreamableInit) : TInputLine(streamableInit) {
}
#endif

const char * const near TInputLong::name = "TInputLong";
