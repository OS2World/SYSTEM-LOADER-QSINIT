/*------------------------------------------------------------*/
/* filename -       framelin.cpp                              */
/*                                                            */
/* function(s)                                                */
/*                  TFrame frameLine member function          */
/*------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#define Uses_TFrame
#define Uses_TDrawBuffer
#define Uses_TWindow
#define Uses_TRect
#define Uses_TPoint
#define Uses_TEvent
#include <tv.h>

/* Erkl„rung der Mask:

       Bit 0
         |
  Bit 3 - - Bit 1
         |
       Bit 2

Wenn z.B. sichergestellt werden soll, dass eine linke obere Ecke im
Muster vorhanden ist, nimmt man :
  mask |= 0x06 .
*/

void TFrame::frameLine(TDrawBuffer &frameBuf, short y, short n, uchar color) {
   unsigned char frameMask[maxViewWidth];
   short int i;
   frameMask[0]=initFrame[n];
   for (i=1; i+1<size.x; i++) frameMask[i]=initFrame[n+1];
   frameMask[size.x-1]=initFrame[n+2];

   TView *p;
   p=owner->last;
   while (1) {
      p=p->next;
      if (p==this) break;
      if ((p->options & ofFramed) && (p->state & sfVisible)) {
         unsigned char mask1, mask2;
         if (y+1<p->origin.y) continue; else
            if (y+1==p->origin.y) { mask1=0x0A; mask2=0x06; } else 
               if (y==p->origin.y+p->size.y) { mask1=0x0A; mask2=0x03; } else
                  if (y<p->origin.y+p->size.y) { mask1=0; mask2=0x05; } 
                     else continue;
         unsigned short xMin=p->origin.x;
         unsigned short xMax=p->origin.x+p->size.x;
         if (xMin<1) xMin=1;
         if (xMax>size.x-1) xMax=size.x-1;
         if (xMax>xMin) {
            if (mask1==0) {
               frameMask[xMin-1] |= mask2;
               frameMask[xMax]   |= mask2;
            } else {
               frameMask[xMin-1] |= mask2;
               frameMask[xMax]   |= (mask2 ^ mask1);
               for (i=xMin; i< xMax; i++) {
                  frameMask[i] |= mask1;
               }
            }
         }
      }
   } // while
   //  unsigned char* src=frameMask;
   unsigned short *dest=frameBuf.data;
   i=size.x;
   short int i1=0;
   while (i--) {
      *dest++= (((unsigned short)color) << 8) + (unsigned char) frameChars[frameMask[i1]];
      i1++;
   } /* endwhile */
}
