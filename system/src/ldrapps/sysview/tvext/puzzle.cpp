/*---------------------------------------------------------*/
/*                                                         */
/*   Turbo Vision Puzzle Demo                              */
/*                                                         */
/*---------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#define Uses_TRect
#define Uses_TEvent
#define Uses_TKeys
#define Uses_TDrawBuffer
#define Uses_TView
#define Uses_TWindow
#include <tv.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "tvrand.h"
#include "puzzle.h"

#define cpPuzzlePalette "\x06\x07"

//
// TPuzzleView functions & static variables
//
static char boardStart[16] =
   { 'A', 'B', 'C', 'D',
     'E', 'F', 'G', 'H',
     'I', 'J', 'K', 'L',
     'M', 'N', 'O', ' '};

static char map[15] =
   { 0, 1, 0, 1,
     1, 0, 1, 0,
     0, 1, 0, 1,
     1, 0, 1 };

TPuzzleView::TPuzzleView(TRect& r) : TView(r) {
   randomize();
   options |= ofSelectable;
   memset( board, ' ', sizeof(board) );

   for (int i = 0; i <= 3; i++)
      for (int j = 0; j <= 3; j++)
         board[i][j] = boardStart[i*4+j];

   scramble();
}

void TPuzzleView::draw() {
   char tmp[8];
   char color[2], colorBack;
   TDrawBuffer buf;

   color[0] = color[1] = colorBack = getColor(1);
   if (!solved) color[1] = getColor(2);

   for (short i = 0; i<=3; i++) {
      buf.moveChar(0, ' ', colorBack, 18);
      if (i==1) buf.moveStr(13, "Move", colorBack);
      if (i==2) buf.moveStr(14, itoa(moves, tmp, 10), colorBack);
      for (short j = 0; j<=3; j++) {
         strcpy(tmp, "   ");
         tmp[1] = board[i][j];
         if (board[i][j] == ' ') buf.moveStr((short)(j*3), tmp, color[0]);
            else buf.moveStr( (short)(j*3), tmp, color[map[board[i][j]-'A']]);
      }
      writeLine(0, i, 18, 1, buf);
   }
}

TPalette& TPuzzleView::getPalette() const {
   static TPalette palette( cpPuzzlePalette, sizeof(cpPuzzlePalette)-1 );
   return palette;
}

void TPuzzleView::handleEvent(TEvent& event) {
   TView::handleEvent(event);

   if (solved && (event.what&(evKeyboard|evMouse))) {
      scramble();
      clearEvent(event);
   }

   if (event.what == evMouseDown) {
      moveTile(event.mouse.where);
      clearEvent(event);
      winCheck();
   } else
   if (event.what == evKeyDown) {
      moveKey(event.keyDown.keyCode);
      clearEvent(event);
      winCheck();
   }
}

void TPuzzleView::moveKey(int key) {
   int ii;
   for (ii = 0; ii<=15; ii++)
      if(board[ii/4][ii%4] == ' ') break;

   int x = ii % 4;
   int y = ii / 4;

   switch(key) {
      case kbDown:
         if (y > 0) {
            board[y][x] = board[y-1][x];
            board[y-1][x] = ' ';
            if (moves<1000) moves++;
         }
         break;
      case kbUp:
         if (y < 3) {
            board[y][x] = board[y+1][x];
            board[y+1][x] = ' ';
            if (moves<1000) moves++;
         }
         break;
      case kbRight:
         if (x > 0) {
            board[y][x] = board[y][x-1];
            board[y][x-1] = ' ';
            if (moves<1000) moves++;
         }
         break;
      case kbLeft:
         if (x < 3) {
            board[y][x] = board[y][x+1];
            board[y][x+1] = ' ';
            if (moves<1000) moves++;
         }
         break;
   }
   drawView();
}

void TPuzzleView::moveTile(TPoint p) {
   int ii;
   p = makeLocal(p);

   for (ii = 0; ii<=15; ii++)
      if (board[ii/4][ii%4] == ' ') break;
   int x = p.x / 3;
   int y = p.y;

   switch (y*4 + x - ii) {
      case -4: moveKey(kbDown);  break;
      case -1: moveKey(kbRight); break;
      case  1: moveKey(kbLeft);  break;
      case  4: moveKey(kbUp);    break;
   }
   drawView();
}

void TPuzzleView::scramble() {
   moves = 0;
   solved = 0;
   do {
      switch ((rand()>>4)%4) {
         case 0: moveKey(kbUp);    break;
         case 1: moveKey(kbDown);  break;
         case 2: moveKey(kbRight); break;
         case 3: moveKey(kbLeft);  break;
      }
   } while (moves++<=500);
   moves = 0;
   drawView();
}

static char *solution = "ABCDEFGHIJKLMNO ";

void TPuzzleView::winCheck() {
   int ii;
   for (ii = 0; ii<=15; ii++)
      if (board[ii/4][ii%4] != solution[ii]) break;

   if (ii==16) solved = 1;
   drawView();
}

//
// TPuzzleWindow functions
//
TPuzzleWindow::TPuzzleWindow() :
   TWindow(TRect(1, 1, 21, 7), "Puzzle", wnNoNumber),
   TWindowInit(&TPuzzleWindow::initFrame)
{
    flags &= ~(wfZoom | wfGrow);
    growMode = 0;

    TRect r = getExtent();
    r.grow(-1, -1);
    insert( new TPuzzleView(r) );
}
