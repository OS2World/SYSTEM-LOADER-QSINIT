//
// QSINIT "start" module
// message box implementation
//
#include "vioext.h"
#include "classes.hpp"
#include "syslocal.h"
#include "qsvdata.h"                   // for SESN_DETACHED only
#include "qslog.h"
#include "qscon.h"

#define WDT_WIDE    70
#define WDT_NORM    50
#define MAX_BUTTYPE  5

#undef  COLOR_SEL  // dialog color changing by 1..4 + up & down arrows

static u8t mb_color[MSG_WHITE+1][4] = { // box text button selbutton
//   { 0x7F, 0x7F, 0x2A, 0x2E },  // gray
   { 0x7F, 0x7F, 0x8F, 0x2E },  // gray
//   { 0x3B, 0x3F, 0x2F, 0x2E },  // cyan
   { 0x3B, 0x3F, 0x73, 0xFC },  // cyan
//   { 0x2A, 0x2F, 0xA5, 0xFC },  // green
   { 0x2A, 0x2F, 0xAF, 0xEC },  // green
//   { 0xA8, 0xA8, 0x1F, 0x9E },  // light green
   { 0xAE, 0xA0, 0x2A, 0xEC },  // light green
   { 0x1B, 0x1F, 0xF9, 0xEC },  // blue
//   { 0x9F, 0x9F, 0xF1, 0xBC },  // light blue
   { 0x9F, 0x9F, 0xF1, 0xCE },  // light blue
//   { 0x4C, 0x4E, 0x71, 0xF9 },  // red
   { 0x4C, 0x4E, 0x65, 0xFC },  // red
//   { 0xCF, 0xCF, 0x3F, 0xEC },  // light red
   { 0xCF, 0xCF, 0x4C, 0xEC },  // light red
//   { 0xF1, 0xF1, 0x1B, 0xCE }}; // white
   { 0xF1, 0xF1, 0x7F, 0x9C }}; // white

static const char *mb_text[] = { 0, "Ok", "Cancel", "Yes", "No" };

static u32t mb_res[] = { MRES_CANCEL, MRES_OK, MRES_CANCEL, MRES_YES, MRES_NO };
// list of buttons (up to 4 buttons in reverse order)
static u32t mb_list[MAX_BUTTYPE] = { 0, 0x01, 0x0201, 0x0403, 0x020403 };

static u32t mb_buttons[MAX_BUTTYPE] = { 0, 1, 2, 2, 3 };

static void drawkeys(u32t km, u32t ypos, u32t scheme, u32 &selected) {
   u32t len = 0,
         ii = 0, width,
         bt = mb_list[km];
   while (bt) {
      if (len) len+=2;
      len+= strlen(mb_text[bt&0x0F])+2;
      bt>>=8;
   }
   vio_getmode(&width,0);
   bt  = mb_list[km];
   ii  = width-len >> 1;
   len = 0;
   while (bt) {
      const char *str = mb_text[bt&0x0F];
      bt>>=8;
      vio_setpos(ypos, ii);
      vio_setcolor(mb_color[scheme][len++==selected?3:2]);

      vio_charout(' ');
      vio_strout(str);
      vio_charout(' ');
      vio_setcolor(mb_color[scheme][0]&0xF0);
      vio_charout(0xDC);
      u32t btlen = strlen(str)+2;
      vio_setpos(ypos+1, ii+1);
      ii += btlen+2;
      while (btlen--) vio_charout(0xDF);
   }
   vio_setcolor(VIO_COLOR_RESET);
}

static void draw_shadow(u32t x, u32t y, u32t dx, u32t dy) {
   if (dx<3||dx>1024) return;
   u16t *buf = (u16t*)malloc_local(dx*2);
   vio_readbuf(x+2, y+dy, dx-1, 1, buf, 0);
   u32t ii,jj;
   for (ii=0; ii<dx-1; ii++) buf[ii] = buf[ii]&0xFF|0x0800;
   vio_writebuf(x+2, y+dy, dx-1, 1, buf, 0);
   for (ii=1; ii<dy; ii++) {
      vio_readbuf(x+dx, y+ii, 1, 1, buf, 0);
      buf[0] = buf[0]&0xFF|0x0800;
      vio_writebuf(x+dx, y+ii, 1, 1, buf, 0);
   }
   free(buf);
}

static u32t _std vio_msgbox_int(const char *header, const char *text,
                                u32t flags, vio_mboxcb cbfunc)
{
   if (!text) return 0;
   if ((flags>>4&0xF)>=MAX_BUTTYPE) return 0;
   TStrings lst;
   spstr    hdr(header);
   u32t   width = flags&MSG_WIDE?WDT_WIDE:WDT_NORM, viscols, lines, vislines, ii,
        keymode = flags>>4&0xF, borderlines = keymode?6:4, 
            m_x = 80, m_y = 25, shadow = flags&MSG_NOSHADOW?0:1;
   vio_getmode(&m_x, &m_y);
   // add a third of additional width to box width
   if (m_x>80) width += (m_x-80)/3;
   viscols = width - 4;
   /* note, that splittext() takes ANSI in mind, but msgbox ignoring it
      and shows as usual symbols, so ansi seqs in text will cause chaos */
   splittext(text, viscols, lst);
   // drop lines behind maximum height
   if (lst.Count() > m_y-4-borderlines) lst.Delete(m_y-4-borderlines, lst.Count());
   vislines = lst.Count();
   lines    = vislines + borderlines;
   // trim long header
   while (hdr.length()>viscols) hdr.del(viscols,hdr.length()-viscols);

   // align text
   for (ii=0; ii<lst.Count(); ii++) {
      if (lst[ii].length()>viscols) lst[ii].del(viscols, lst[ii].length()-viscols);
         else
      switch (flags&0x300) {
         case MSG_CENTER : {
            int step = 0;
            while (lst[ii].length()<viscols)
               if (step++&1) lst[ii].insert(" ",0); else lst[ii]+=" ";
            break;
         }
         case MSG_RIGHT  :
            while (lst[ii].length()<viscols) lst[ii].insert(" ",0);
            break;
         case MSG_JUSTIFY: if (ii<lst.Max()) { // do not align last line
            u32t wrds = lst[ii].words(), pos;
            if (wrds>1)
               while (lst[ii].length()<viscols) {
                  pos = wrds>2?random(wrds-1)+2:2;
                  lst[ii].insert(" ",lst[ii].wordpos(pos));
               }
            break;
         }
      }
   }
   u32t cs = flags&0x0F, outln, psv_x, psv_y, sel;
   // hide cursor & save its pos
   u16t oldshape = vio_getshape();
   vio_setshape(VIO_SHAPE_NONE);   
   vio_getpos(&psv_y, &psv_x);
   // invalid color scheme number
   if (cs>MSG_WHITE) cs = MSG_GRAY;
   // default button
   sel = (flags&0xC00)>>10;
   if (sel>=mb_buttons[keymode]) sel = 0;
   // calc pos
   m_x = m_x-width>>1;
   m_y = m_y-lines>>1;
   // save screen data
   void *savedata = malloc_local((width+1)*(lines+1)*2);
   vio_readbuf(m_x, m_y, width+shadow, lines+shadow, savedata, 0);
#ifdef COLOR_SEL
   u32t act=0;
drawagain:
#endif
   // draw box
   draw_border(m_x, m_y, width, lines, mb_color[cs][0]);
   if (shadow) draw_shadow(m_x, m_y, width, lines);
   // draw header text
   vio_setcolor(mb_color[cs][0]);
   vio_setpos(m_y, m_x+(width-hdr.length()-2>>1));
   vio_charout(' '); vio_strout(hdr()); vio_charout(' ');

   outln = m_y + 2;
   for (ii=0; ii<lst.Count(); ii++) {
      vio_setpos(outln+ii, m_x+2);
      vio_setcolor(mb_color[cs][1]);
      vio_strout(lst[ii]());
   }
   vio_setcolor(VIO_COLOR_RESET);

   drawkeys(keymode, m_y + 3 + vislines, cs, sel);
   u32t dlgres = MRES_CANCEL;

   int cbrc = -1;
   if (cbfunc)
      if ((cbrc=cbfunc(KEY_ENTERMBOX))>=0) dlgres = cbrc;
   // start common key cycle if no result from callback
   if (cbrc<0)
   while (1) {
      u16t key = key_read();
      // ask callback
      if (cbfunc)
         if ((cbrc=cbfunc(key))>=0) { dlgres = cbrc; break; }
            else
         if (cbrc==-2) continue;
      // process key
      u8t  keyl = key, keyh = key>>8;
      u32t selprev = sel;
#ifdef COLOR_SEL
      if (keyl>='1'&&keyl<='4') act=keyl-'1';
#endif
      if (keyl==27) { // esc
         dlgres = 0;
         break;
      } else
      if (keyl==9) {  // tab
         if (sel<mb_buttons[keymode]-1) sel++; else sel=0;
      } else
      if (key==0x0F00) {  // shift-tab
         if (sel>0) sel--; else sel=mb_buttons[keymode]-1;
      }
      if (keyl==13 || keyl==' ') { // enter/space
         dlgres = mb_res[mb_list[keymode]>>8*sel&0xF];
         break;
      } else
      if (keymode) {
         switch (keyh) {
            case 0x4D: // right
               if (sel<mb_buttons[keymode]-1) sel++;
               break;
            case 0x4B: // left
               if (sel>0) sel--;
               break;
#ifdef COLOR_SEL
            case 0x48:
               mb_color[cs][act]=(mb_color[cs][act]&0xF)+
                  ((mb_color[cs][act]&0xF0)+0x10&0xF0);
               goto drawagain;
               break;
            case 0x50:
               mb_color[cs][act]=(mb_color[cs][act]&0xF0)+
                  ((mb_color[cs][act]&0xF)+0x1&0xF);
               goto drawagain;
               break;
#endif
         }
      }
      if (selprev!=sel) drawkeys(keymode, m_y + 3 + vislines, cs, sel);
   }
#ifdef COLOR_SEL
   log_it(3, "scheme=%04b\n",&mb_color[cs]);
#endif
   if (cbfunc)
      if ((cbrc=cbfunc(KEY_LEAVEMBOX))>=0) dlgres = cbrc;
   // restore screen & cursor state
   vio_writebuf(m_x, m_y, width+shadow, lines+shadow, savedata, 0);
   vio_setpos(psv_y, psv_x);
   vio_setshape(oldshape);
   free(savedata);

   return dlgres;
}

typedef struct {
   const char *header,
                *text;
   u32t         flags;
   vio_mboxcb  cbfunc;
} box_call_data;

static u32t _std msgbox_thread(void *arg) {
   // set name for myself
   get_mtlib()->threadname("Vio Popup");
   u32t        sesno = se_sesno();
   box_call_data *pd = (box_call_data*)arg;

   // create session. vio device is used for detached popups
   qserr err = se_newsession(sesno==SESN_DETACHED?1:0, FFFF, pd->header);
   if (err) return 0;
   /* query device mask (all of them should be foreground) */
   se_stats *sd = se_stat(se_sesno());
   u32t devmask = sesno==SESN_DETACHED?0:sd->devmask;
   free(sd);
   // shows box in new session and return result as thread exit code
   u32t  result = vio_msgbox_int(pd->header, pd->text, pd->flags, pd->cbfunc);
   // switch back to the session, who calls us
   if (devmask) se_switch(sesno, devmask);

   return result;
}


u32t _std vio_msgbox(const char *header, const char *text, u32t flags, vio_mboxcb cbfunc) {
   if (!text) return 0;
   // common way
   if (!in_mtmode || (flags&MSG_POPUP)==0)
      return vio_msgbox_int(header, text, flags&~MSG_POPUP, cbfunc);
   // crazy way ;)
   box_call_data bcd = { header, text, flags, cbfunc};
   mt_waitentry   we = { QWHT_TID, 0};
   qs_mtlib       mt = get_mtlib();
   u32t      rc, sig;

   we.resaddr = &rc;
   we.tid     = mt->createthread(msgbox_thread, 0, 0, &bcd);
   if (!we.tid) return 0;
   /* thread with popup cannot exit while we crawling between createthread &
      waitobject. I.e. waitobject fail mean only some kind of internal error */
   if (mt->waitobject(&we, 1, 0, &sig)) rc = 0;

   return rc;
}

// **************************************************************************************

static u32t  _std vio_showlist_int(const char *header, vio_listref *ref, u32t flags) {
   // check it first!
   if (!ref || !ref->text || ref->size!=sizeof(vio_listref)) return 0;
   TStrings lst, *lna = 0, hdr;
   str_getstrs(ref->text, lst);
   if (!lst.Count()) return 0;
   // deny this too
   if (lst.Count()>ref->items+1) return 0;

   int colcnt = 0, ii, ci, nlines;
   // we have a header
   if (lst.Count()==ref->items+1) {
      hdr.SplitString(lst[0],"|");
      colcnt = -hdr.Count();
      lst.Delete(0);
   }
   nlines = lst.Count();
   if (!nlines) return 0;

   lna = new TStrings[nlines];
   mem_localblock(lna);

   for (ii=0; ii<nlines; ii++) {
      lna[ii].SplitString(lst[ii],"|");
      int wcnt = lna[ii].Count();
      if (colcnt>=0 && colcnt<wcnt) colcnt = wcnt;
   }
   /* we have predefined # of columns from the header string or collect it
      from lines in the loop above */
   if (colcnt<0) colcnt = -colcnt;
   // fill missing columns up to selected colcnt
   for (ii=0; ii<nlines; ii++)
      while (lna[ii].Count()<colcnt) lna[ii].Add(spstr());
   while (hdr.Count()<colcnt) hdr.Add(spstr());
   // calc longest line length
   for (ci=0; ci<colcnt; ci++) hdr.Objects(ci) = 0;
   for (ii=0; ii<nlines; ii++) {
      for (ci=0; ci<colcnt; ci++) {
         u32t len = lna[ii][ci].length();
         if (len>hdr.Objects(ci)) hdr.Objects(ci) = len;
      }
   }
   u32t mx = 80, my = 25, *wdt = (u32t*)malloc_local(colcnt*4),
        px = 0;
   vio_getmode(&mx, &my);
   // take value from the header
   for (ci=0; ci<colcnt; ci++) {
      wdt[ci] = hdr.Objects(ci) + 2; // +2 space borders
      if (hdr[ci].length()) {
         u32t pos = 0;
         while (hdr[ci][pos] && !isdigit(hdr[ci][pos])) pos++;
         u32t value = hdr[ci].Dword(pos);
         if (hdr[ci].lastchar()=='%')
            if (value>=100) value = 0; else value = mx*value/100;
         if (value) wdt[ci] = value; else
            if (pos>=2) wdt[ci] -= 2;
      }
      px += wdt[ci]; 
   }
   /* 2 border, 2 spaces bound to borders, char for submenu indication
      and column split lines (with spaces) */
   px += 2 + 2 + (ref->subm?1:0) + colcnt - 1;
   u32t sx   = px>mx-10 ? mx-10 : px, dlgres = 0, psv_y, psv_x,
        sy   = nlines+2>my-2 ? my-2 : nlines+2, draw = 2, _lp = 0,
        posx = mx-sx>>1, shadow = flags&MSG_NOSHADOW?0:1,
        posy = my-sy>>1,
        cs   = flags&0x0F;
   int  _ls  = 0, draw_p = -1, draw_n,
       sblen = nlines+2<=my-2 ? -1 : (my-6) * (my-4) / nlines;
   // hide cursor & save its pos
   u16t  oldshape = vio_getshape(),
         oldcolor = vio_getcolor();
   vio_setshape(VIO_SHAPE_NONE);   
   vio_getpos(&psv_y, &psv_x);
   // save screen data
   void *savedata = malloc_local((sx+shadow)*(sy+shadow)<<1);
   char *dbuf     = (char*)malloc_local(px+1);
   dbuf[sx] = 0;
   vio_readbuf(posx, posy, sx+shadow, sy+shadow, savedata, 0);

   while (1) {
      if (draw) {
         // 0 - top, 1 - bottom, 2 - mid, 3 - horz. line, 4 - double line
         static const char *sym[5] = { "\xC9\xCD\xD1\xBB", "\xC8\xCD\xCF\xBC", 
            "\xBA \xB3\xBA", "\xC7\xC4\xC5\xB6", "\xCC\xCD\xD8\xB9" };
         for (ii=-1; ii<(int)sy-1; ii++) {
            u32t    lt = 2, dpos;
            int  sbofs = sblen<0? -1 : (my-6) * _lp / nlines;
            if (ii<0) lt=0; else
               if (ii==sy-2) lt=1; else
                  if (lna[ii+_lp][0]=="=") lt=4; else
                     if (lna[ii+_lp][0]=="-") lt=3;
            // draw to & botton during first paint only
            if (lt<2 && draw==1) continue;
            // partial draw
            if (draw_p>=0 && ii+_lp!=draw_p && ii+_lp!=draw_n) continue;

            memset(dbuf, sym[lt][1], sx);
            for (ci=0, dpos=2; ci<colcnt; ci++) {
               if (lt==2) {
                  u32t  len = lna[ii+_lp][ci].length();
                  if (len) {
                     u32t  dofs = 0, sofs = 0;
                     char   ca0 = !hdr[ci]?0:hdr[ci][0];
                     int  caspc = 1;
                     if (ca0) caspc = hdr[ci][1]==ca0?0:1;
                     u32t  wspc = wdt[ci]-caspc*2;
                     
                     if (len<wspc) {
                        if (ca0!='<') {
                           dofs = wspc-len;
                           if (ca0!='>') dofs>>=1;
                        }
                     } else {
                        if (ca0=='>') sofs = len - wspc;
                        len = wspc;
                     }
                     memcpy(dbuf+dpos+caspc+dofs, lna[ii+_lp][ci]()+sofs, len);
                  }
               }
               dpos += wdt[ci];
               if (dpos>=sx-2 || ci==colcnt-1) break;
               dbuf[dpos++] = sym[lt][2];
            }
            if (lt==2 && ref->subm && ref->subm[_lp+ii]) dbuf[sx-2] = '\x10';
            vio_setpos(posy+1+ii, posx);
            vio_setcolor(mb_color[cs][0]);

            char right = sym[lt][3];
            if (sblen>=0 && lt==2)
               if (ii==0) right = 0x1E; else
                  if (ii==sy-3) right = 0x1F; else
                     right = ii-1<sbofs || ii-1>sbofs+sblen?0xB0:0xB2;

            if (lt==2 && _lp+ii==_ls) {
               vio_charout(sym[lt][0]);
               vio_setcolor(mb_color[cs][3]);
               dbuf[sx-1] = 0;
               vio_strout(dbuf+1);
               vio_setcolor(mb_color[cs][0]);
               vio_charout(right);
            } else {
               dbuf[0]    = sym[lt][0];
               dbuf[sx-1] = right;
               vio_strout(dbuf);
            }
            // draw header & shadow during first paint
            if (lt==0) {
               spstr hstr(header);
               if (hstr.length()>sx-4) hstr.del(sx-4, hstr.length()-sx);
               if (hstr.length()) {
                  vio_setpos(posy, posx+(sx-2-hstr.length()>>1));
                  vio_charout(' '); vio_strout(hstr()); vio_charout(' ');
               }
               if (shadow) draw_shadow(posx, posy, sx, sy);
            }
         
         }
         draw   =  0;
         draw_p = -1;
         vio_setcolor(VIO_COLOR_RESET);
      }
      u16t  key = key_read();
      u8t  keyl = key, keyh = key>>8;
      

      if (keyl==27 || keyh==0x0E) { // esc || backspc
         dlgres = 0;
         break;
      } else
      if (keyl=='\r' || keyl==' ') {
         if (ref->subm && ref->subm[_ls]) {
            dlgres = vio_showlist_int(0, ref->subm[_ls], flags&~(MSG_POPUP|MSG_NOSHADOW));
            if (dlgres) break;
         } else {
            dlgres = ref->id ? ref->id[_ls] : _ls+1;
            break;
         }
      } else {
         int prev = _ls, incv=0, prevp = _lp;
         switch (keyh) {
            case 0x49: if (_ls>sy-2) { _ls-=sy-2; incv=-1; break; }
            case 0x47: _ls=0; break;
            case 0x48: _ls--; incv=-1; break;
            case 0x51: _ls+=sy-2; if (_ls<nlines-1) { incv=1; break; }
            case 0x4F: _ls = nlines-1; break;
            case 0x50: _ls++; incv= 1; break;
         }
         // skip split line in menu if this is possible
         if (incv && _ls>=0 && _ls<nlines)
            if (lna[_ls][0]=="-"||lna[_ls][0]=="=") _ls+=incv;
         // check and update menu lines
         if (_ls<0) _ls = nlines-1; else
            if (_ls>=nlines) _ls = 0;
         if (_ls==0) _lp = 0; else
            if (_ls==nlines-1) _lp = nlines - (sy-2); else
               if (incv<0 && _lp>_ls) _lp = _ls; else
                  if (incv>0 && _lp+sy-2<=_ls) _lp = _ls+1 - (sy-2);
         draw   = prev != _ls;
         // partial draw if was no start position movement
         if (draw && _lp==prevp) { draw_p = prev; draw_n = _ls; }
      }
   }
   vio_writebuf(posx, posy, sx+shadow, sy+shadow, savedata, 0);
   vio_setpos(psv_y, psv_x);
   vio_setshape(oldshape);
   vio_setcolor(oldcolor);

   free(savedata);
   delete[] lna;
   free(dbuf);
   free(wdt);

   return dlgres;
}

typedef struct {
   const char *header;
   vio_listref   *ref;
   u32t         flags;
} lst_call_data;

static u32t _std showlist_thread(void *arg) {
   // naming self
   get_mtlib()->threadname("Vio List");
   u32t        sesno = se_sesno();
   lst_call_data *pd = (lst_call_data*)arg;

   // create session. vio device is used for detached popups
   qserr err = se_newsession(sesno==SESN_DETACHED?1:0, FFFF, pd->header);
   if (err) return 0;
   /* query device mask (all of them should be foreground) */
   se_stats *sd = se_stat(se_sesno());
   u32t devmask = sesno==SESN_DETACHED?0:sd->devmask;
   free(sd);
   // shows box in new session and return result as thread exit code
   u32t  result = vio_showlist_int(pd->header, pd->ref, pd->flags);
   // switch back to the session, who calls us
   if (devmask) se_switch(sesno, devmask);

   return result;
}

u32t  _std vio_showlist(const char *header, vio_listref *ref, u32t flags) {
   if (!ref) return 0;
   // common way
   if (!in_mtmode || (flags&MSG_POPUP)==0)
      return vio_showlist_int(header, ref, flags&~MSG_POPUP);
   // crazy way ;)
   lst_call_data lcd = { header, ref, flags };
   mt_waitentry   we = { QWHT_TID, 0};
   qs_mtlib       mt = get_mtlib();
   u32t      rc, sig;

   we.resaddr = &rc;
   we.tid     = mt->createthread(showlist_thread, 0, 0, &lcd);
   if (!we.tid) return 0;
   /* thread with popup cannot exit while we crawling between createthread &
      waitobject. I.e. waitobject fail mean only some kind of internal error */
   if (mt->waitobject(&we, 1, 0, &sig)) rc = 0;

   return rc;
}
