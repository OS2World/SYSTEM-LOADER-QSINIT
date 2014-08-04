//
// QSINIT "sysview" module
// binary data editor
//
#include "binedit.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

THexEditor::THexEditor(const TRect& bounds, TScrollBar *aVScrollBar, int varysize) :
  TScroller(bounds,0,aVScrollBar)
{
   MaxLines  = 1;
   setLimit(0,MaxLines);
   showCursor();
   options  |= ofFirstClick;
   growMode  = gfGrowHiY;
   data      = 0;
   Size      = 0;
   changed   = 0;
   overwrite = 1;
   sizeable  = varysize;
}

void THexEditor::setState(ushort aState, Boolean enable) {
   TScroller::setState(aState, enable);

   if (aState!=sfCursorIns && cursor.x>=60 && cursor.x<=75)
      setCursor((cursor.x-60)*3+10,cursor.y);
}

void THexEditor::handleEvent( TEvent& event ) {
   TScroller::handleEvent(event);
   if (event.what==evKeyDown && getState(sfFocused+sfSelected)) {
      ushort kb = event.keyDown.keyCode;
      switch (kb) {
         case kbUp:
            if (cursor.y==0)
               if (delta.y>0) scrollTo(delta.x,delta.y-1);
                  else setCursor(cursor.x,1);
            setCursor(cursor.x,cursor.y-1);
            clearEvent(event);
            break;
         case kbDown:
            if (cursor.y>=size.y-1)
               if (delta.y<MaxLines-size.y) scrollTo(delta.x,delta.y+1);
                  else setCursor(cursor.x,size.y-2);
            setCursor(cursor.x,cursor.y+1);
            clearEvent(event);
            break;
         case kbRight:
            if (cursor.x+1<size.x) setCursor(cursor.x+1,cursor.y);
            clearEvent(event);
            break;
         case kbLeft:
            if (cursor.x-1>=10) setCursor(cursor.x-1,cursor.y);
            clearEvent(event);
            break;
         case kbIns:
            toggleInsMode();
            clearEvent(event);
            break;
         case kbPgUp:
            scrollTo(delta.x,delta.y-size.y+2>=0?delta.y-size.y+2:0);
            setCursor(cursor.x,cursor.y-size.y+2>=0?cursor.y-size.y+2:0);
            clearEvent(event);
            break;
         case kbPgDn:
            scrollTo(delta.x,delta.y+size.y<MaxLines-size.y?
               delta.y+size.y-2:MaxLines-size.y);
            setCursor(cursor.x,cursor.y+size.y-2?cursor.y+size.y-2:size.y-1);
            clearEvent(event);
            break;
         case kbHome:
            setCursor(cursor.x>=60?60:10,cursor.y);
            clearEvent(event);
            break;
         case kbEnd: {
               long mp=Size-(delta.y+cursor.y<<4);
               if (mp>15) mp=15;
               setCursor(cursor.x>=60?60+mp:10+3*mp,cursor.y);
               clearEvent(event);
               break;
            }
         case kbTab: if (cursor.x>=10&&cursor.x<58) {
               setCursor((cursor.x-10)/3+60,cursor.y);
               clearEvent(event);
            }
            break;
         case kbShiftTab: if (cursor.x>=60&&cursor.x<=75) {
               setCursor((cursor.x-60)*3+10,cursor.y);
               clearEvent(event);
            }
            break;
      }
      if (event.what==evNothing) return;

      uchar ch = event.keyDown.charScan.charCode;
      if (cursor.x>=10&&cursor.x<57) {
         long pos=cursor.x-10,
              ofs=(delta.y+cursor.y<<4)+pos/3;

         if (ch>='0'&&ch<='9'||ch>='A'&&ch<='F'||ch>='a'&&ch<='f') {
            char cc[2];
            cc[0]=toupper(ch); cc[1]=0;
            long val=strtol(cc,0,16);

            if ((ofs==Size || !overwrite && pos%3==0) && sizeable) { 
               ++Size; data=realloc(data,Size);
               if (!overwrite) {
                  memmove((uchar*)data+ofs+1, (uchar*)data+ofs, Size-ofs-1);
                  ((uchar*)data)[ofs] = 0;
               } else
                  ((uchar*)data)[Size-1] = 0;
            }
            clearEvent(event);
            if (ofs<Size) {
               if (pos%3==0) *((uchar*)data+ofs)=*((uchar*)data+ofs)&0x0F|val<<4; else
                  *((uchar*)data+ofs)=*((uchar*)data+ofs)&0xF0|val;
               if (pos%3==1) pos++;
               if (++pos/3==16) { 
                  if (cursor.y>=size.y-1) {
                     setLimit(0,MaxLines=(Size+16)>>4);
                     scrollTo(delta.x,delta.y+1);
                  }
                  setCursor(10,cursor.y+1); 
               } else setCursor(pos+10,cursor.y);
               drawView();
            }
         } else
         if (sizeable && (kb==kbBack||kb==kbDel) && Size>1) {
            clearEvent(event);
            int delact = 0;
            // backspace works as it only on first pos
            if (pos%3 && kb==kbBack) kb=kbDel;

            if (kb==kbBack && ofs>0 && ofs<=Size) {
               if (ofs<Size) 
                  memmove((uchar*)data+ofs-1, (uchar*)data+ofs, Size-ofs);
               delact = 1;
            } else
            if (kb==kbDel && ofs>=0 && ofs<Size) {
               if (ofs<Size-1) 
                  memmove((uchar*)data+ofs, (uchar*)data+ofs+1, Size-ofs);
               delact = 1;
            }
            if (delact) {
               Size--;
               pos-=pos%3;
               if (kb==kbDel) setCursor(pos+10,cursor.y); else
               if (pos) setCursor(pos-3+10,cursor.y); else {
                  if (cursor.y==0 && delta.y) scrollTo(delta.x,delta.y-1);
                  if (cursor.y) cursor.y--;
                  setCursor(55,cursor.y); 
               }
               setLimit(0,MaxLines=(Size+16)>>4);
               drawView();
            }
         }
      } else
      if (cursor.x>=60&&cursor.x<77) {
         long pos=cursor.x-60,
              ofs=(delta.y+cursor.y<<4)+pos;

         if (ch>=' '&&ch<=0xFF) {
            if ((ofs==Size || !overwrite) && sizeable) { 
               ++Size; data=realloc(data,Size);
               if (!overwrite)
                  memmove((uchar*)data+ofs+1, (uchar*)data+ofs, Size-ofs-1);
            }
            clearEvent(event);
            if (ofs<Size) {
               *((uchar*)data+ofs)=ch;
               if (++pos==16) { 
                  if (cursor.y>=size.y-1) {
                     setLimit(0,MaxLines=(Size+16)>>4);
                     scrollTo(delta.x,delta.y+1);
                  }
                  setCursor(60,cursor.y+1); 
               } else setCursor(pos+60,cursor.y);
               drawView();
            }
         } else
         if (sizeable && (kb==kbBack||kb==kbDel) && Size>1) {
            clearEvent(event);
            int delact = 0;

            if (kb==kbBack && ofs>0 && ofs<=Size) {
               if (ofs<Size) 
                  memmove((uchar*)data+ofs-1, (uchar*)data+ofs, Size-ofs);
               delact = 1;
            } else
            if (kb==kbDel && ofs>=0 && ofs<Size) {
               if (ofs<Size-1) 
                  memmove((uchar*)data+ofs, (uchar*)data+ofs+1, Size-ofs);
               delact = 1;
            }
            if (delact) {
               Size--;
               if (kb==kbDel) setCursor(pos+60,cursor.y); else
               if (pos) setCursor(pos-1+60,cursor.y); else {
                  if (cursor.y==0 && delta.y) scrollTo(delta.x,delta.y-1);
                  if (cursor.y) cursor.y--;
                  setCursor(75,cursor.y); 
               }
               setLimit(0,MaxLines=(Size+16)>>4);
               drawView();
            }

         }
      }
   }
}

void THexEditor::draw() {
   TDrawBuffer B;
   ushort col=0x1F;//getColor(2);
 
   for (int y=0;y<size.y;y++) {
      B.moveChar(0,' ',col,size.x);
      if (data) {
         long Offset=delta.y+y<<4;
         if (Size>Offset) {
            int Len=min(16,Size-Offset),x;
            char out[384], text[24], *ppos=out;
            
            sprintf(out, "%8.8x:",Offset);
            ppos=out+strlen(out);
            memset(text,' ',16);
            
            for (x=0;x<Len;x++) {
               unsigned char value=*((unsigned char*)data+Offset+x);
               sprintf(ppos," %2.2x",value);
               ppos+=3;
               text[x]=value?(char)value:' ';
            }
            text[16]=0;
            x=-1;
            while (out[++x]) out[x]=toupper(out[x]);
            
            while (x<57) out[x++]=' ';
            out[x++]=' ';
            out[x++]='\xB3';
            out[x++]=' ';
            strcpy(out+x,text);
            B.moveStr(0,out,col);
         }
      }
      writeLine(0,y,size.x,1,B);
   }
}

void THexEditor::toggleInsMode() {
   overwrite = Boolean(!overwrite);
   setState(sfCursorIns, Boolean(!getState(sfCursorIns)));
}

void THexEditor::shutDown() {
//   if (data) free(data); !!!
   data=0;
   Size=0;
   TScroller::shutDown();
}

void THexEditor::getData(void *rec) {
   THexEditorData*dt=(THexEditorData*)rec;
   dt->data=data;
   dt->size=Size;
}

void THexEditor::setData(void *rec) {
   THexEditorData*dt=(THexEditorData*)rec;
   data=dt->data;
   Size=dt->size;
   long sz=(((Size)+0x0F)&0xFFFFFFF0);
   setLimit(0,MaxLines=sz>>4);
   setCursor(10,0);
   drawView();
}
