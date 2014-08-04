#include "vioext.h"
#include "classes.hpp"
#include "internal.h"
#include "stdlib.h"
#include "qslog.h"

#define WDT_WIDE    70
#define WDT_NORM    50
#define HDT_MAX     21

#define MAX_BUTTYPE  5

#undef  COLOR_SEL  // dialog color changing by 1..4 + up & down arrows

static u8t mb_color[MSG_WHITE+1][4] = { // box text button selbutton
   { 0x7F, 0x7F, 0x2A, 0x2E },  // gray
   { 0x3B, 0x3F, 0x2F, 0x2E },  // cyan
   { 0x2A, 0x2F, 0xA5, 0xFC },  // green
   { 0xA8, 0xA8, 0x1F, 0x9E },  // light green
   { 0x1B, 0x1F, 0xF9, 0xEC },  // blue
   { 0x9F, 0x9F, 0xF1, 0xBC },  // light blue
   { 0x4C, 0x4E, 0x71, 0xF9 },  // red
   { 0xCF, 0xCF, 0x3F, 0xEC },  // light red
   { 0xF1, 0xF1, 0x1B, 0xCE }}; // white

static const char *mb_text[] = { 0, "Ok", "Cancel", "Yes", "No" };

static u32t mb_res[] = { MRES_CANCEL, MRES_OK, MRES_CANCEL, MRES_YES, MRES_NO };
// list of buttons (up to 4 numbers of buttons on reverse order)
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
   u16t buf[160];
   if (dx<3||dx>160) return;
   vio_readbuf(x+2,y+dy,dx-1,1,buf,0);
   u32t ii,jj;
   for (ii=0;ii<dx-1;ii++) buf[ii] = buf[ii]&0xFF|0x0800;
   vio_writebuf(x+2,y+dy,dx-1,1,buf,0);
   for (ii=1;ii<dy;ii++) {
      vio_readbuf(x+dx,y+ii,1,1,buf,0);
      buf[0] = buf[0]&0xFF|0x0800;
      vio_writebuf(x+dx,y+ii,1,1,buf,0);
   }
}

u32t _std vio_msgbox(const char *header, const char *text, u32t flags, vio_mboxcb *cbfunc) {
   if (!text) return 0;
   if ((flags>>4&0xF)>=MAX_BUTTYPE) return 0;
   TStrings lst;
   spstr    hdr(header);
   u32t   width = flags&MSG_WIDE?WDT_WIDE:WDT_NORM,
        viscols = width - 4, lines, vislines, ii,
        keymode = flags>>4&0xF, borderlines = keymode?6:4;
   splittext(text, viscols, lst);
   // drop lines behind maximum height
   if (lst.Count()>HDT_MAX-borderlines)
      lst.Delete(lines, lst.Count()-(HDT_MAX-borderlines));
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
   u32t mx=80, my=25, cs = flags&0x0F, outln, psv_x, psv_y, sel;
   // hide cursor & save its pos
   u16t oldshape = vio_getshape();
   vio_setshape(0x20,0);
   vio_getpos(&psv_y, &psv_x);
   // invalid color scheme number
   if (cs>MSG_WHITE) cs = MSG_GRAY;
   // default button
   sel = (flags&0xC00)>>10;
   if (sel>=mb_buttons[keymode]) sel = 0;
   // calc pos
   vio_getmode(&mx, &my);
   mx = mx-width>>1;
   my = my-lines>>1;
   // save screen data
   void *savedata = malloc((width+1)*(lines+1)*2);
   vio_readbuf(mx, my, width+1, lines+1, savedata, 0);
#ifdef COLOR_SEL
   u32t act=0;
drawagain:
#endif
   // draw box
   draw_border(mx, my, width, lines, mb_color[cs][0]);
   draw_shadow(mx, my, width, lines);
   // draw header text
   vio_setcolor(mb_color[cs][0]);
   vio_setpos(my,mx+(width-hdr.length()-2>>1));
   vio_charout(' '); vio_strout(hdr()); vio_charout(' ');

   outln = my+2 + (vislines-lst.Count()>>1);
   for (ii=0; ii<lst.Count(); ii++) {
      vio_setpos(outln+ii, mx+2);
      vio_setcolor(mb_color[cs][1]);
      vio_strout(lst[ii]());
   }
   vio_setcolor(VIO_COLOR_RESET);

   drawkeys(keymode, my+3+vislines, cs, sel);
   u32t dlgres = MRES_CANCEL;

   int cbrc = -1;
   if (cbfunc)
      if ((cbrc=(*cbfunc)(KEY_ENTERMBOX))>=0) dlgres = cbrc;
   // start common key cycle if no result from callback
   if (cbrc<0)
   while (1) {
      u16t key = key_read();
      // check for hotkey
      if (log_hotkey(key)) continue;
      // ask callback
      if (cbfunc)
         if ((cbrc=(*cbfunc)(key))>=0) { dlgres = cbrc; break; }
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
      if (selprev!=sel) drawkeys(keymode, my+3+vislines, cs, sel);
   }
#ifdef COLOR_SEL
   log_it(3, "scheme=%04b\n",&mb_color[cs]);
#endif
   if (cbfunc)
      if ((cbrc=(*cbfunc)(KEY_LEAVEMBOX))>=0) dlgres = cbrc;
   // restore screen & cursor state
   vio_writebuf(mx, my, width+1, lines+1, savedata, 0);
   vio_setpos(psv_y, psv_x);
   vio_setshape(oldshape>>8,oldshape&0xFF);
   free(savedata);

   return dlgres;
}
