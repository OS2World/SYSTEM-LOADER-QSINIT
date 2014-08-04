/* ------------------------------------------------------------------------*/
/*                                                                         */
/*   TEXTVIEW.H                                                            */
/*                                                                         */
/*   Copyright (c) Borland International 1991                              */
/*   All Rights Reserved.                                                  */
/*                                                                         */
/*   defines the classes TTextDevice and TTerminal                         */
/*                                                                         */
/* ------------------------------------------------------------------------*/

#include <tvvo.h>

#if defined( Uses_TTextDevice ) && !defined( __TTextDevice )
#define __TTextDevice

#if !defined(NO_TV_STREAMS)
#include <iostream.h>
#else
#include <stdio.h>
#endif

#include <tvvo.h>

class TRect;
class TScrollBar;

#if !defined(NO_TV_STREAMS)
class TTextDevice : public streambuf, public TScroller
#else
class TTextDevice : public TScroller
#endif
{

public:

   TTextDevice(const TRect &bounds,
               TScrollBar *aHScrollBar,
               TScrollBar *aVScrollBar
              );

   virtual int do_sputn(const char *s, int count) = 0;
#if defined(__OS2__) && defined(__BORLANDC__)
   virtual int _RTLENTRY overflow(int = EOF); // flush buffer and make more room
#elif defined(__IBMCPP__)
   virtual int overflow(int = EOF);
#else
   virtual int TV_CDECL overflow(int = EOF);
#endif
#if defined(__WATCOMC__)
   virtual int underflow(void) { return 0; }
#endif

};

#endif  // Uses_TTextDevice

#if defined( Uses_TTerminal ) && !defined( __TTerminal )
#define __TTerminal

class TRect;
class TScrollBar;

class TTerminal: public TTextDevice {

public:

   friend void genRefs();

   TTerminal(const TRect &bounds,
             TScrollBar *aHScrollBar,
             TScrollBar *aVScrollBar,
             size_t aBufSize
            );
   ~TTerminal();

   uchar (*getLineColor)(const char *s);

   virtual int do_sputn(const char *s, int count);

   void bufInc(size_t &val);
   Boolean canInsert(size_t amount);
   virtual void draw();
   size_t nextLine(size_t pos);
   size_t TV_CDECL prevLines(size_t pos, int lines);
   Boolean queEmpty();

protected:

   size_t bufSize;
   char *buffer;
   size_t queFront, queBack;
   void bufDec(size_t &val);
};

#endif  // Uses_TTerminal

#if defined( Uses_otstream ) && !defined( __otstream )
#define __otstream

#include <tvvo.h>

#if !defined(NO_TV_STREAMS)
class otstream : public ostream {

public:

   otstream(TTerminal *);

};
#else

class otstream {
private:
   TTerminal *tty;

public:
   otstream(TTerminal *tt) { tty = tt; }

   otstream &operator<<(char const *str);
   otstream &operator<<(unsigned char const *str) { return(*this << (char const *)str); }
   otstream &operator<<(char c);
   otstream &operator<<(unsigned char uc) { return(*this << (char)uc); }
};
#endif

#endif

#include <tvvo2.h>

