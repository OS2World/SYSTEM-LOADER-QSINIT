/*------------------------------------------------------------*/
/* filename -       tgrmv.cpp                                 */
/*                                                            */
/* function(s)                                                */
/*                  TGroup removeView member function         */
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

#define Uses_TGroup
#include <tv.h>



void TGroup::removeView(TView *p) {
   if (last) {
      TView *cur=last;
      while (1) {
         if (p==cur->next) {
            cur->next=p->next;
            if (last==p) {
               if (cur->next==p) last=0;
               else last=cur;
               break;
            }
         }
         if (cur->next==last) break;
         cur=cur->next;
      }
   } // endif
}
