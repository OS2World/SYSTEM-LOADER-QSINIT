//
// QSINIT "sysview" module
// huge cluster-based data binary editor
//
#include "longedit.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

// emulate bcmp for PC build
#ifndef __QSINIT__
unsigned long __bcmp(const void *s1, const void *s2, unsigned long length);
#define bcmp __bcmp
#else
#include <qslog.h>
#endif

#define cpLongEditor "\x06\x07"

TLongEditor::TLongEditor(const TRect& bounds) : TView(bounds) {
   memset(&uinfo, 0, sizeof(uinfo));
   changed = 0; vcl = 0; clstart = 0; clcount = 0; clline = 0; vcflags = 0;
   pos10 = 10; pos60 = 60; clshift = 0;

   options   |= ofSelectable;
   eventMask |= evBroadcast;
   growMode   = gfGrowHiY;
}

void TLongEditor::shutDown() {
   TView::shutDown();
   freeVcl();
}

void TLongEditor::changeBounds(const TRect &bounds) {
   setBounds(bounds);
   updateVcl();
   drawView();
}

TPalette &TLongEditor::getPalette() const {
   static TPalette palette(cpLongEditor, sizeof(cpLongEditor)-1);
   return palette;
}

void TLongEditor::handleEvent(TEvent &event) {
   TView::handleEvent(event);
   
   if (event.what==evKeyDown && getState(sfFocused+sfSelected)) {
      int dcx=0, dcy=0;

      switch (event.keyDown.keyCode) {
         case kbUp: 
            processKey(posUp,0);
            clearEvent(event);
            break;
         case kbDown: 
            processKey(posDown,0);
            clearEvent(event);
            break;
         case kbRight:
            if (cursor.x+1<size.x) setCursor(cursor.x+1,cursor.y);
            clearEvent(event);
            break;
         case kbLeft:
            if (cursor.x-1>=pos10) setCursor(cursor.x-1,cursor.y);
            clearEvent(event);
            break;
         case kbBack:
            processKey(posUndo,0);
            clearEvent(event);
            break;
         case kbPgUp:
            processKey(posPageUp,0);
            clearEvent(event);
            break;
         case kbPgDn:
            processKey(posPageDown,0);
            clearEvent(event);
            break;
         case kbHome:
            setCursor(cursor.x>=pos60?pos60:pos10,cursor.y);
            clearEvent(event);
            break;
         case kbEnd: {
            setCursor(cursor.x>=pos60?pos60+15:pos10+3*15,cursor.y);
            clearEvent(event);
            break;
         }
         case kbCtrlHome:
            processKey(posStart,0);
            clearEvent(event);
            break;
         case kbCtrlEnd:
            processKey(posEnd,0);
            clearEvent(event);
            break;
         case kbTab: 
            if (cursor.x>=pos10&&cursor.x<pos60-2) {
               setCursor((cursor.x-pos10)/3+pos60,cursor.y);
               clearEvent(event);
               break;
            } else {
               owner->selectNext(False);
               clearEvent(event);
               if (owner->current!=this || cursor.x<pos60) break;
            }
         case kbShiftTab:
            if (cursor.x>=pos60&&cursor.x<=pos60+15) {
               setCursor((cursor.x-pos60)*3+pos10,cursor.y);
               clearEvent(event);
            }
            break;
         default: 
            {
               uchar ch=event.keyDown.charScan.charCode;

               le_cluster_t clc;
               int line = cursorLine(&clc);

               // edit actual data           
               if (line>=0 && (vcflags[clc-clstart] || !uinfo.noreaderr)) {
                  // cluster data
                  if (!changed) {
                     changed = (uchar*)malloc(clcount<<clshift);
                     memcpy(changed, vcl, clcount<<clshift);
                  }
                  uchar *src = changed + ((clc-clstart)<<clshift);

                  if (cursor.x>=pos10 && cursor.x<pos60-3) {
                     ch = toupper(ch);
                     if (ch>='0'&&ch<='9'||ch>='A'&&ch<='F') {
                        long pos=cursor.x-pos10;
                        char cc[2];
                        cc[0]=ch; cc[1]=0;
                        long val=strtol(cc,0,16),
                             ofs=(line<<4)+pos/3;
                       
                        if (pos%3==0) src[ofs] = src[ofs]&0x0F|val<<4; 
                           else src[ofs] = src[ofs]&0xF0|val;
                        if (pos%3==1) pos++;
                  
                        if (++pos/3==16) {
                           processKey(posDown);
                           setCursor(pos10, cursor.y);
                        } else 
                           setCursor(pos+pos10, cursor.y);
                        drawView();
                        clearEvent(event);
                     }
                  } else
                  if (cursor.x>=pos60 && cursor.x<pos60+17) {
                     if (ch>=' '&&ch<=0xFF) {
                        long pos=cursor.x-pos60;
                        long ofs=(line<<4)+pos;
                        
                        src[ofs] = ch;
                        if (++pos==16) {
                           processKey(posDown);
                           setCursor(pos60, cursor.y);
                        } else 
                           setCursor(pos+pos60, cursor.y);
                        drawView();
                        clearEvent(event);
                     }
                  }
               }
            }
            break;
      }
      if (event.what==evNothing) return;
   }
}

void TLongEditor::draw() {
   TDrawBuffer db;
   ushort col=0x1F, chcol=0x1E, chcol2=0x1C;//getColor(2);
   char   buf[384], fmt[12];
   le_cluster_t clc = clstart;
   int  cll = clline, 
        eod = clstart>=uinfo.clusters,
      lines = uinfo.clustersize>>4;  // number of lines

   sprintf(fmt, "%%%03uLX:", pos10-2);

   for (int y=0;y<size.y;y++) {
      db.moveChar(0,' ',col,size.x);
  
      if (!eod) {
         if (cll<0) {
            if (uinfo.showtitles) {
               buf[0] = 0;
               uinfo.gettitle(uinfo.userdata, clc, buf);
               db.moveStr(3, buf, chcol);
            }
         } else {
            unsigned long offs = (unsigned long)cll<<4, xx;
            char text[24], *ppos=buf;

            if (!uinfo.showtitles)
               ppos = buf + sprintf(buf, fmt, offs + (clc<<clshift));
            else
               ppos = buf + sprintf(buf, "%08X:", offs);

            memset(text,' ',16);
            // cluster data
            char *ccld = (char*)vcl + ((clc-clstart)<<clshift),
                  *src = changed?(char*)changed + ((clc-clstart)<<clshift):ccld;

            if (uinfo.noreaderr && !vcflags[clc-clstart]) {
               for (xx=0; xx<16; xx++) ppos+=sprintf(ppos," ??");
            } else {
               for (xx=0; xx<16; xx++) {
                  uchar value = src[offs+xx];
                  ppos    += sprintf(ppos," %02X",value);
                  text[xx] = value?(char)value:' ';
               }
            }
            text[16]=0;
            xx = ppos - buf;
            while (xx<pos60-3) buf[xx++]=' ';
            buf[xx++]=' ';
            buf[xx++]='\xB3';
            buf[xx++]=' ';
            strcpy(buf+xx,text);
            db.moveStr(0,buf,col);
            // set yellow for changes bytes
            if (changed)
               for (xx=0; xx<16; xx++)
                  if (src[offs+xx]!=ccld[offs+xx]) {
                     db.putAttribute(pos60+xx,chcol2);
                     db.putAttribute(pos10+xx*3,chcol2);
                     db.putAttribute(pos10+xx*3+1,chcol2);
                  }
         }
      }
      writeLine(0,y,size.x,1,db);
      // next cluster
      if (++cll==lines) {
         cll = uinfo.showtitles?-1:0;
         eod = ++clc>=uinfo.clusters;
      }
   }
}

size_t TLongEditor::dataSize() {
   return sizeof(TLongEditorData);
}

void TLongEditor::getData(void *rec) {
   *(TLongEditorData*)rec = uinfo;
}

void TLongEditor::setData(void *rec) {
   uinfo = *(TLongEditorData*)rec;
   freeVcl();
   // calc bit shift (ulgy method instead of signle bsf64() call
   if (uinfo.clustersize) {
      unsigned long ii = uinfo.clustersize;
      clshift = 0;
      while (ii>1) { ii>>=1; clshift++; }
      if (uinfo.clustersize != 1<<clshift)
         memset(&uinfo, 0, sizeof(uinfo));
   }
   if (!uinfo.showtitles) {
      pos10 = 10 + (TScreen::screenWidth>80 ? 2 : 1);
      pos60 = pos10 + 50;
   } else {
      pos10 = 10; 
      pos60 = 60;
   }
   clline = uinfo.showtitles?-1:0;
   setCursor(pos10,-clline);
   updateVcl();
   drawView();
}

/// query line number under cursor
int TLongEditor::cursorLine(le_cluster_t *cluster) {
   if (!uinfo.clustersize) return -1;
   le_cluster_t clc = clstart;
   int        lines = uinfo.clustersize>>4,  // number of lines per cluster
             cll1st = clline, rc;
   if (uinfo.showtitles) { lines++; cll1st++; }
   // number of visible lines in 1st cluster
   cll1st = lines - cll1st;

   if (cursor.y < cll1st) {
      rc = clline + cursor.y;
   } else {
      rc = cursor.y - cll1st;
      clc++;
      while (rc >= lines) { rc-=lines; clc++; } 
      if (uinfo.showtitles) rc--;
   }
   if (cluster) *cluster = clc;

   //log_it(2," cursorLine = %d of %d\n", rc, clc);

   return rc;
}

void TLongEditor::freeVcl() {
   if (vcflags) { free(vcflags); vcflags = 0; }
   if (changed) { free(changed); changed = 0; }
   if (vcl) { free(vcl); vcl = 0; }
   clstart = 0; clcount = 0; clline = 0;
   hideCursor();
}

int TLongEditor::rwAction(le_cluster_t cluster, void *data, int write) {
   if (!uinfo.write) {
      messageBox("\x03""Data update is not supported!",mfError+mfOKButton);
      return 0;
   }
   int rc = write?uinfo.write(uinfo.userdata, cluster, data):
                  uinfo.read (uinfo.userdata, cluster, data);
   if (!rc && (!uinfo.noreaderr || write)) {
      char message[192];
      if (uinfo.gettitle) {
         char title[128];
         uinfo.gettitle(uinfo.userdata, cluster, title);
         sprintf(message,"\x03%s error!\n\x03[%s]",write?"Write":"Read",title);
      } else
         sprintf(message,"\x03Unable to %s data at offset %08LX!",
            write?"write":"read", cluster<<clshift);

      messageBox(message,mfError+mfOKButton);
   }
   return rc;
}

int TLongEditor::isChanged() {
   if (!uinfo.clustersize) return 0;
   if (!changed) return 0;
   return bcmp(changed, vcl, clcount<<clshift)?1:0;
}

void TLongEditor::commitChanges() {
   if (isChanged()) processKey(posNullOp);
}

Boolean TLongEditor::valid(ushort command) {
   if (command == cmValid)
      return uinfo.clustersize?True:False;
   else {
      if (isChanged()) {
         commitChanges();
         return isChanged()?False:True;
      }
   }
   return True;
}

// need to call freeVcl() before this to read full clcount number of clusters
void TLongEditor::updateVcl() {
   if (!uinfo.clustersize) return;
   int lines = uinfo.clustersize>>4;  // number of lines
   if (uinfo.showtitles) lines++;
   // number of clusters in visible area
   int csvis = (size.y+lines*2-1)/lines;
   if (clstart+csvis>uinfo.clusters) csvis = uinfo.clusters-clstart;

   //log_it(2," update = %d < %d\n", clcount, csvis);

   if (clcount<csvis) {
      vcl     = (uchar*)realloc(vcl, csvis<<clshift);
      vcflags = (uchar*)realloc(vcflags, csvis);
      if (changed) 
         changed = (uchar*)realloc(changed, csvis<<clshift);

      while (clcount<csvis) {
         // zero buffer for read error
         memset(vcl + (clcount<<clshift), 0, uinfo.clustersize);
         // read it
         vcflags[clcount] = rwAction(clstart+clcount, vcl + (clcount<<clshift), 0);
         // copy data to "changed" array
         if (changed) memcpy(changed + (clcount<<clshift),
            vcl + (clcount<<clshift), uinfo.clustersize);
         clcount++;
      }
   }
   showCursor();
}

// return 0 if cluster is not in visible area
uchar *TLongEditor::changedArray(le_cluster_t cluster) {
   if (!uinfo.clustersize) return 0;
   if (cluster<clstart || cluster>=clstart+clcount) return 0;
   if (!changed) {
      changed = (uchar*)malloc(clcount<<clshift);
      memcpy(changed, vcl, clcount<<clshift);
   }
   return (uchar*)changed+(cluster-clstart<<clshift);
}

void TLongEditor::dataChanged() {
   if (!uinfo.clustersize) return;
   unsigned long clsave = clstart;
   freeVcl();
   processKey(posSet, clsave);
}

unsigned long TLongEditor::clusterSize() {
   return uinfo.clustersize;
}

void TLongEditor::posToCluster(le_cluster_t cluster, unsigned long posin) {
   if (!uinfo.clustersize || cluster>=uinfo.clusters) return;
   if (posin>=uinfo.clustersize) posin = uinfo.clustersize - 1;
   processKey(posSet, cluster);

   int needline = posin>>4, 
          lines = uinfo.clustersize>>4,   // number of lines per cluster
           less = 0;
   // cursor now must be on the first line of cluster
   if (cursor.y + needline >= size.y) {
      less    = cursor.y + needline + 1 - size.y;
      clline += less;
      updateVcl();
      drawView();
   }
   setCursor(pos10 + 3 * (posin & 15), cursor.y + needline - less);
}

void TLongEditor::processKey(posmoveType mv, le_cluster_t cluster) {
   if (!uinfo.clustersize) return;
   int lines = uinfo.clustersize>>4;   // number of lines per cluster
   if (uinfo.showtitles) lines++;
   int csvis = (size.y+lines-1)/lines; // number of clusters in visible area

   le_cluster_t  clstart_n = clstart, cl_cur;
   int            clline_n = clline,
                clline_min = uinfo.showtitles?-1:0,
                  easy_nav = 0,
                  line_cur = cursorLine(&cl_cur),
                      draw = 0;
   TPoint         cursor_n = cursor;

   if (mv==posPageUp && clstart <= csvis)
      if (clline-clline_min + lines * clstart < size.y) mv=posStart;

   if (mv==posStart) { cluster = 0; mv = posSet; } else
   if (mv==posEnd) { cluster = uinfo.clusters-1; mv = posSet; }
   if (mv==posSet && cluster>=uinfo.clusters) return;

   switch (mv) {
      case posUndo: {
         long pos = cursor.x>=pos60?cursor.x-pos60:(cursor.x-pos10)/3;
         if (cursor.x<pos60 && (cursor.x-pos10)%3) pos++;

         if (line_cur<0) {
            if (cl_cur==0) break;
            cursor_n.x = cursor.x>=pos60 ? pos60+15 : pos10+3*15; 
         } else 
         if (line_cur==0 && pos==0) {
            cursor_n.x = cursor.x>=pos60? pos60 : pos10;
         } else {
            if (line_cur>=0 && changed) {
               uchar *dst = changed + (cl_cur-clstart<<clshift),
                     *src = vcl + (cl_cur-clstart<<clshift);
               dst[line_cur*16+pos-1] = src[line_cur*16+pos-1];
               draw |= 1;
            }
            if (--pos>=0) {
               cursor_n.x = cursor.x>=pos60 ? pos60+pos : pos10+3*pos; 
               easy_nav = 1;
               break;
            } else
               cursor_n.x = cursor.x>=pos60 ? pos60+15 : pos10+3*15; 
         } // no break here!
      } 
      case posUp:
         if (cursor.y==0) {
            if (clline>clline_min) {
               clline_n--; 
               easy_nav = clline_n>=0;
            } else 
            if (clstart>0) {                        
               clstart_n--;                         // next cluster
               clline_n = lines-1+clline_min;
            } else                                  // cluster 0 - no action
               return;
         } else {
            cursor_n.y--;                           // pos in visible area
            easy_nav = line_cur>0;                  // no a first line of cluster?
         }
         break;
      case posDown: {
         int lline_max = (uinfo.clustersize>>4)-1;  // # of last line in cluster
         if (cursor.y>=size.y-1) {
            if (cl_cur<uinfo.clusters-1 || line_cur<lline_max) {
               if (clline_n>=lline_max) {
                  clstart_n++; 
                  clline_n = clline_min;
               } else {
                  clline_n++;
               }
               // do not update until reach end of cluster
               easy_nav = line_cur<lline_max;
            }
         } else {
            cursor_n.y++;
            easy_nav = line_cur<lline_max;          // not a last line?
         }
         break;
      }
      case posPageUp:
         clline_n -= (int)size.y;
         while (clline_n<clline_min) { clline_n+=lines; clstart_n--; }
         break;
      case posPageDown: {
         // maximum possible pos
         le_cluster_t  lc = uinfo.clusters - size.y/lines;
         int     l_clline = clline_min;
         if (size.y%lines) { lc--; l_clline += lines - size.y%lines; }
         // nest start pos
         if (uinfo.showtitles) clline_n++;
         clline_n += (int)size.y;
         while (clline_n>=lines) { clline_n-=lines; clstart_n++; }
         if (uinfo.showtitles) clline_n--;
         // compare it with maximum possible
         if (clstart_n==lc)
            if (l_clline < clline_n) clstart_n++;
         if (clstart_n>lc) {
            clstart_n  = lc;
            clline_n   = l_clline;
            cursor_n.x = pos10 + 15*3;
            cursor_n.y = size.y - 1;
         }
         break;
      }
      case posSet:
         clstart_n  = cluster;
         clline_n   = uinfo.showtitles?-1:0;
         cursor_n.x = pos10;
         cursor_n.y = -clline_n;
         // position at the end of data
         if (clstart_n + csvis > uinfo.clusters) {
            // data is smaller whan screen - use offset 0
            if (clstart_n < csvis) clstart_n = 0; else {
               clstart_n  = uinfo.clusters - csvis;
               //clline_n   = lines /*- 1*/ - size.y % lines;

               clline_n   = lines==1 ? 0 : lines /*- 1*/ - size.y % lines;

               if (uinfo.showtitles) clline_n--;
               cursor_n.y = size.y - lines;
               if (uinfo.showtitles) cursor_n.y++;
            }
         }
         break;
   }

   if (changed && !easy_nav) {
      unsigned long dpos = bcmp(changed, vcl, clcount<<clshift);

      if (dpos) {
         le_cluster_t     actcl = clstart + (dpos>>clshift);
         unsigned char *actdata = changed + (actcl-clstart<<clshift);
         int             update = 1;

         if (uinfo.askupdate) {
            update = uinfo.askupdate(uinfo.userdata, actcl, actdata);
            if (update ==-1) return; else
            if (update == 0) {
               memcpy(actdata, vcl + (actcl-clstart<<clshift),
                  uinfo.clustersize);
               draw |= 1;
            }
         }
         if (update==1)
            if (rwAction(actcl, actdata, 1)) {
               // swap vcl array to changed
               free(vcl); vcl = changed; changed = 0;
               draw |= 1;
            } else
               return;
      }
   }
   draw |= clstart_n!=clstart || clline!=clline_n;

   if (clstart_n!=clstart) {
      freeVcl();
      clstart = clstart_n;
      clline  = clline_n;
      updateVcl();
   } else
   if (clline!=clline_n) {
      clline = clline_n;
      updateVcl();
   }
   if (cursor_n!=cursor) setCursor(cursor_n.x, cursor_n.y);

   if (draw) drawView();
}
