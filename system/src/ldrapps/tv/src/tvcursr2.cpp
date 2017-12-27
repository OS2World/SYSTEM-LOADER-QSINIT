/*------------------------------------------------------------*/
/* filename -       tstrlist.cpp                              */
/*                                                            */
/* function(s)                                                */
/*                  Tview resetCursor member function         */
/*------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 *      This file Copyright (c) 1993 by J”rn Sierwald
 */

#define Uses_TRect
#define Uses_TPoint
#define Uses_TView
#define Uses_TGroup
#define Uses_TThreaded
#define Uses_TScreen
#include <tv.h>

#ifdef __DOS32__
#include <dos.h>
#endif
#ifdef __QSINIT__
#include <vio.h>
#endif

void TView::resetCursor() {
   TView *p,*p2;
   TGroup *g;
   TPoint cur;
   if ((state & (sfVisible | sfCursorVis | sfFocused))
       == (sfVisible | sfCursorVis | sfFocused)) {
      p=this;
      cur=cursor;
      while (1) {
         if (!(cur.x>=0 && cur.x<p->size.x
               && cur.y>=0 && cur.y<p->size.y)) {
            break;
         }
         cur.x += p->origin.x;
         cur.y += p->origin.y;
         p2 =p;
         g=p->owner;
         if (g==0) {
            //cursor setzen
#if defined(__OS2__)
            VIOCURSORINFO &info = TThreads::tiled->cursorInfo;
            info.cEnd=-100;
            info.cx=1;
            info.attr=0;
            if (state & sfCursorIns) {
               info.yStart=-1;  // Big Cursor
            } else {
               info.yStart=-90; // Small Cursor
            }
            VioSetCurType(&info, 0);
            VioSetCurPos(cur.y, cur.x, 0);
#elif defined(__DOS32__)
            REGS r;
            r.h.ah = 2;
            r.h.bh = 0;
            r.h.dh = cur.y;
            r.h.dl = cur.x;
            int386(0x10, &r, &r);
            r.h.ah = 1;
            if (state & sfCursorIns)
               r.w.cx = 0x0007; // Big Cursor
            else
               r.w.cx = 0x0607; // Small Cursor
            int386(0x10, &r, &r);
#elif defined(__NT__)
            TThreads::crInfo.bVisible = True;
            if (state & sfCursorIns)
               TThreads::crInfo.dwSize = 99;         // big cursor
            else
               TThreads::crInfo.dwSize = 10;         // small cursor
            SetConsoleCursorInfo(TThreads::chandle[cnOutput],&TThreads::crInfo);
            COORD coord = { cur.x, cur.y };
            SetConsoleCursorPosition(TThreads::chandle[cnOutput],coord);
#elif defined(__QSINIT__)
            vio_setpos(cur.y, cur.x);
            vio_setshape(state&sfCursorIns ? VIO_SHAPE_FULL : TScreen::startupCursor);
#else
#error Unknown platform!
#endif
            return;
         }
         if (!(g->state & sfVisible)) break;
         p=g->last;
         {
label1:
            p=p->next;
            if (p==p2) { // alle durchgesucht.
               p=p->owner;
               continue;
            }
            if ((p->state & sfVisible)
                && cur.x>=p->origin.x
                && cur.x<p->size.x+p->origin.x
                && cur.y>=p->origin.y
                && cur.y<p->size.y+p->origin.y) {
               break; // Cursor wird verdeckt.
            }
            goto label1;
         }
      }
   }
   // no cursor, please.
   {
#ifdef __OS2__
      VIOCURSORINFO &info = TThreads::tiled->cursorInfo;
      info.yStart=2;
      info.cEnd=3;
      info.cx=1;
      info.attr= (short) -1; // hide Cursor
      VioSetCurType(&info, 0);
#else
#ifdef __DOS32__
      {
         REGS r;
         r.h.ah = 1;
         r.w.cx = 0x2000;
         int386(0x10, &r, &r);
      }
#else
#ifdef __NT__
      TThreads::crInfo.bVisible = False;
      TThreads::crInfo.dwSize = 1;
      SetConsoleCursorInfo(TThreads::chandle[cnOutput], &TThreads::crInfo);
#elif defined(__QSINIT__)
      vio_setshape(VIO_SHAPE_NONE);
#else
#error Unknown platform!
#endif  // __NT__
#endif  // __DOS32__
#endif  // __OS2__
   }
}
