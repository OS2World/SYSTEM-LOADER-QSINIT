/*------------------------------------------------------------*/
/* filename -       exposed.cpp                               */
/*                                                            */
/* function(s)                                                */
/*                  Tview exposed member function             */
/*------------------------------------------------------------*/

/*------------------------------------------------------------*/
/*                                                            */
/*    Turbo Vision -  Version 1.0                             */
/*    Copyright (c) 1991 by Borland International             */
/*    All Rights Reserved.                                    */
/*                                                            */
/*    This file Copyright (c) 1993 by J”rn Sierwald           */
/*                                                            */
/*                                                            */
/*------------------------------------------------------------*/

#define Uses_TView
#define Uses_TGroup
#include <tv.h>


struct StaticVars2 {
   TView        *target;
   short         y;
};

static StaticVars2 staticVars2;

int TView::exposedRec1(short x1, short x2, TView *p) {
   while (1) {
      /*20*/
      p=p->next;
      if (p==staticVars2.target) { // alle durch
         return exposedRec2(x1, x2, p->owner);
      }
      if (!(p->state & sfVisible) || staticVars2.y<p->origin.y) continue;  // keine Verdeckung

      if (staticVars2.y<p->origin.y+p->size.y) {
         // šberdeckung m”glich.
         if (x1<p->origin.x) { // f„ngt links vom Object an.
            if (x2<=p->origin.x) continue; // links vorbei
            if (x2>p->origin.x+p->size.x) {
               if (exposedRec1(x1, p->origin.x, p)) return 1;
               x1=p->origin.x+p->size.x;
            } else
               x2=p->origin.x;
         } else {
            if (x1<p->origin.x+p->size.x) x1=p->origin.x+p->size.x;
            if (x1>=x2) return 0;   // komplett verdeckt.
         }
      }

   } // while

}

int TView::exposedRec2(short x1, short x2, TView *p) {
   if (!(p->state & sfVisible)) return 0;
   if (!p->owner || p->owner->buffer) return 1;    // taken from tvunix_6, 02.02.99

   StaticVars2 savedStatics = staticVars2;

   staticVars2.y += p->origin.y;
   x1 += p->origin.x;
   x2 += p->origin.x;
   staticVars2.target=p;

   TGroup *g=p->owner;
   if (staticVars2.y<g->clip.a.y || staticVars2.y >= g->clip.b.y) {
      staticVars2 = savedStatics;
      return 0;
   }
   if (x1<g->clip.a.x) x1 = g->clip.a.x;
   if (x2>g->clip.b.x) x2 = g->clip.b.x;
   if (x1>=x2) {
      staticVars2 = savedStatics;
      return 0;
   }

   int retValue = exposedRec1(x1, x2, g->last);
   staticVars2 = savedStatics;
   return retValue;
}

Boolean TV_CDECL TView::exposed() {
   if (!(state & sfExposed) || size.x <= 0 || size.y <= 0) return Boolean(0);
   for (short y=0; y<size.y; y++) {
      staticVars2.y=y;
      if (exposedRec2(0, size.x, this)) return Boolean(1);
   }
   return Boolean(0);
}

