/*------------------------------------------------------------*/
/* filename -       tcolortx.cpp                              */
/*                                                            */
/* function(s)                                                */
/*                  TColoredText member functions             */


/*    Portions Copyright (c) 1991 by Borland International    */
/*    All Rights Reserved.                                    */
/*------------------------------------------------------------*/

/*----------------------------------------------------------------------
TColoredText is a descendent of TStaticText designed to allow the writing
of colored text when color monitors are used.  With a monochrome or BW
monitor, TColoredText acts the same as TStaticText.

TColoredText is used in exactly the same way as TStaticText except that
the constructor has an extra Byte parameter specifying the attribute
desired.  (Do not use a 0 attribute, black on black).

----------------------------------------------------------------------*/

#define Uses_TColoredText
#define Uses_TStaticText
#define Uses_TApplication
#define Uses_TDrawBuffer
#define Uses_opstream
#define Uses_ipstream

#if defined(__DOS16__)||defined(SP_LINUX)
#define TEXTBUF_SIZE    256
#else
#define TEXTBUF_SIZE    (4096+1)
#endif

#if defined (TV2)
#   include <tvision/tv.h>
#elif defined (TV1)
#   include <tv.h>
#else
#   error TV1 or TV2 must be defined
#endif

#include "tcolortx.h"

#if !defined( __CTYPE_H )
#include <ctype.h>
#endif  // __CTYPE_H

#if !defined( __STRING_H )
#include <string.h>
#endif  // __STRING_H

TColoredText::TColoredText( const TRect& bounds, const char *aText,
                                ushort attribute ) :
    TStaticText( bounds, aText ),
    attr(  attribute )
{
}

ushort TColoredText::getTheColor()
{
    if (TProgram::application->appPalette == apColor)
      return attr;
    else return getColor(1);
}

void TColoredText::draw()
// Largely taken from Borland's TStaticText::draw()
{
    uchar color;
    Boolean center;
    int i, j, l, p, y;
    TDrawBuffer b;
    char s[TEXTBUF_SIZE];

    color = (uchar)getTheColor();
    getText(s);
    l = strlen(s);
    p = 0;
    y = 0;
    center = False;
    while (y < size.y)
        {
        b.moveChar(0, ' ', color, size.x);
        if (p < l)
            {
            if (s[p] == 3)
                {
                center = True;
                ++p;
                }
            i = p;
            do {
               j = p;
               while ((p < l) && (s[p] == ' ')) 
                   ++p;
               while ((p < l) && (s[p] != ' ') && (s[p] != '\n'))
                   ++p;
               } while ((p < l) && (p < i + size.x) && (s[p] != '\n'));
            if (p > i + size.x)
                if (j > i)
                    p = j; 
                else 
                    p = i + size.x;
            if (center == True)
               j = (size.x - p + i) / 2 ;
            else 
               j = 0;
            b.moveBuf(j, &s[i], color, (p - i));
            while ((p < l) && (s[p] == ' '))
                p++;
            if ((p < l) && (s[p] == '\n'))
                {
                center = False;
                p++;
                if ((p < l) && (s[p] == 10))
                    p++;
                }
            }
        writeLine(0, y++, size.x, 1, b);
        }
}

#ifndef NO_TV_STREAMS
void TColoredText::write( opstream& os )
{
    TStaticText::write( os );
    os << attr;
}

void *TColoredText::read( ipstream& is )
{
    TStaticText::read( is );
    is >> attr;
    return this;
}

TStreamable *TColoredText::build()
{
    return new TColoredText( streamableInit );
}

TColoredText::TColoredText( StreamableInit ) : TStaticText( streamableInit )
{
}
#endif

const char * const near TColoredText::name = "TColoredText";
