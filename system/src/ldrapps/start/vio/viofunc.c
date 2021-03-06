//
// QSINIT "start" module
// additional VIO functions
//
#include "syslocal.h"
#include "qsutil.h"
#include "qslog.h"

#define ANSI_BUF_LEN   32
static const char *wordcharset = " \t\n-;,.=+*/<>|\\/:`&";

char* _std key_getstr(key_strcb cbfunc) {
   return key_getstrex(cbfunc, -1, -1, -1, 0, 0);
}

/** print string to the current cursor position with ^A..^_ special support.
    Function shows x01..x1F as *green* A.._ characters.

    @param   str  string to print.
    @return actual number of new lines (both \n and the end of screen) occured
            during the source string output. */
u32t _std vio_strout_spec(const char *str) {
   u32t  len = 0,
        spec = 0;
   // check that string has 01-1F ...
   while (1) {
      char ch = str[len];
      if (!ch) break; else len++;
      if (ch<' ') spec++;
   }
   if (!spec) return vio_strout(str); else {
      u32t ii, lcnt = 0;
      u16t defcol = vio_getcolor();

      for (ii=0; ii<len; ii++) {
         char ch = str[ii];
         if (ch<' ') {
            vio_setcolor(VIO_COLOR_GREEN);
            spec += vio_charout(ch+'@');
            vio_setcolor(defcol);
         } else
            spec += vio_charout(ch);
      }
      return lcnt;
   }
}

char* _std key_getstrex(key_strcb cbfunc, int col1, int line1, int len1,
                        const char *init, u32t userdata)
{
   u32t  mode_x, mode_y;
   char            *buf;
   key_strcbinfo     ld;
   // very bad idea, which makes huge troubles for code below this func ;)
   #define  line     ld.line
   #define  col      ld.col      
   #define  width    ld.width    
   #define  pos      ld.pos      
   #define  scroll   ld.scroll   
   #define  clen     ld.clen     
   #define  bsize    ld.bsize    
   ld.userdata = userdata;
   ld.defshape = vio_getshape();
   vio_getpos(&line,&col);
   vio_getmodefast(&mode_x,&mode_y);

   if (col1>=0 && col1<mode_x-1) col=col1;
   if (line1>=0 && line1<mode_y) line=line1;

   width = len1>0?len1:mode_x-1;
   if (col+width+1>=mode_x) width = mode_x-col-1;
   buf   = (char*)malloc_local(mode_x);

   if (init) {
      ld.rcstr = strdup(init);
      bsize    = mem_blocksize(ld.rcstr);
      clen     = strlen(ld.rcstr);

      if (clen>=width) {
         scroll = clen-width+1;
         pos    = clen-scroll;
      } else {
         scroll = 0;
         pos    = clen;
      }
   } else {
      pos=0; bsize=128; clen=0; scroll=0;
      ld.rcstr  = (char*)malloc_local(bsize);
      *ld.rcstr = 0;
   }
   //log_printf("pos: x=%d,y=%d,width=%d\n",col,line,width);

   if (width)
   do {
      u16t key;
      u8t  keyl, keyh;
      int  mve=0, all=0;

      if (init) {
         all  = 3;
         init = 0;
      } else {
         key  = key_read();
         keyl = key;
         keyh = key>>8;

         if (cbfunc) {
            int cbres = cbfunc(key,&ld);
            if (cbres==-1) { free(ld.rcstr); ld.rcstr = 0; }
            if (cbres<0) break;
            all = cbres;
         }
      }

      if (all&2) all&=1; 
         else
      if (keyl==27 && keyh!=0x1A) {  // filter out Ctrl-[
         scroll=0; pos=0; all=1; *ld.rcstr=0; clen=0;
      } else
      if (keyl==13) break; else
      if (keyl>0 && keyl<=0x7E && keyh!=0x0E) {
         if (clen+2>bsize) ld.rcstr = (char*)realloc(ld.rcstr, bsize=(clen+2)*2);
         if (pos+scroll>=clen) ld.rcstr[clen] = keyl; else {
            memmove(ld.rcstr+pos+scroll+1, ld.rcstr+pos+scroll, clen-(pos+scroll));
            ld.rcstr[pos+scroll] = keyl;
         }
         ld.rcstr[++clen]=0; mve=1;
      } else
      switch (keyh) {
         case 0x47: scroll=0; pos=0; all=1;     break; // home
         case 0x4D: if (pos+scroll<clen) mve=1; break; // right
         case 0x4B: if (pos+scroll>0) mve=-1;   break; // left
         case 0x73: // ctrl-left
            if (scroll+pos==0) break;
         case 0x74: // ctrl-right
            if (*ld.rcstr) {
               int  cp = pos+scroll, np;
               if (keyh==0x73) cp = -cp;
               // ctrl-right at the last char acts as "end" key
               if (cp<0 || cp<clen-1) {
                  np = getwordpos(ld.rcstr, wordcharset, cp);
                  if (np<scroll) { scroll=np; pos=0; } else
                     if (np-scroll>=width) scroll=np-(pos=width-1); else
                        pos=np-scroll;
                  all=1;
                  break;
               }
            } else
               break;
            // no break here!
         case 0x4F: // end
            scroll=0;
            if (clen<=width) pos=clen; else scroll=clen-(pos=width);
            all=1;
            break;
         case 0x0E: // (ctrl-)backspace (0E7F - ctrl-bkspc)
            if (pos+scroll>0 && clen) {
               int cp = pos+scroll,
                   np = keyl==0x7F?getwordpos(ld.rcstr,wordcharset,-cp):cp-1;
               if (np<cp) {
                  memmove(ld.rcstr+np, ld.rcstr+cp, clen-cp+1);
                  np    = cp-np;
                  clen -= np;
                  ld.rcstr[clen] = 0;
                  if (scroll) {
                     if ((scroll-=np)<0) { pos+=scroll; scroll=0; }
                     all=1;
                  } else mve=-np;
               }
            }
            break;
         case 0x53: // del
         case 0x93: // ctrl-del
            if (pos+scroll>=0 && pos+scroll<clen ) {
               int cp = pos+scroll,
                   ep = keyh==0x93?getwordpos(ld.rcstr,wordcharset,cp):cp+1;
               if (ep>cp) {
                  memmove(ld.rcstr+cp, ld.rcstr+ep, clen-ep+1);
                  clen -= ep-cp;
                  ld.rcstr[clen] = 0;
                  all = 1;
               }
            }
            break;
      }

      if (mve) {
         if (mve>0) {
            if (pos+mve+scroll<=clen) {
               if (pos+mve>width) scroll+=pos+mve-width; else pos+=mve;
            }
         } else {
            if (pos+mve<0) scroll+=mve; else pos+=mve;
            if (scroll<0) scroll=0;
         }
         all=1;
      }
      if (all) {
         vio_setshape(VIO_SHAPE_NONE);
         vio_setpos(line, col);
         strncpy(buf, ld.rcstr+scroll, width); buf[width]=0;
         vio_strout_spec(buf);
         memset(buf, 0x20, width);
         //log_printf("left space: %d, scroll: %d, clen: %d\n",clen-scroll,scroll,clen);
         if ((clen-scroll)<width) vio_strout_spec(buf+(clen-scroll));
         vio_setpos(line, col+pos);
         vio_setshape(ld.defshape);
         //         pos       clen   width
         //  |||||------------      |
      }
   } while(1);

   //printf("\n(%s)\n",rc);
   free(buf);
   return ld.rcstr;
}
#undef line
#undef col
#undef width
#undef pos
#undef scroll
#undef bsize
#undef clen

// ---------------------------------------------------------------------------
typedef struct {
   u32t    in_use;
   u32t       seq;
   u32t       svx,
              svy;
   int       wrap;
   u16t     color;
   char       str[ANSI_BUF_LEN];
} ansi_state;

// searches and allocates ANSI state buffer
static ansi_state *ansi_getstate(void) {
   process_context *pq = mod_context();
   if (!pq) return 0;
   if (pq->rtbuf[RTBUF_ANSIBUF]) return (ansi_state*)pq->rtbuf[RTBUF_ANSIBUF]; else {
      process_context *spq = pq;
      ansi_state       *rc = 0;
      // allocate new state buffer and copy ON state if possible
      rc = (ansi_state*)malloc(sizeof(ansi_state));
      while (spq && !spq->rtbuf[RTBUF_ANSIBUF]) spq = spq->pctx;
      rc->in_use = spq?((ansi_state*)spq->rtbuf[RTBUF_ANSIBUF])->in_use:1;
      rc->seq    = 0;
      rc->str[0] = 0;
      rc->svx    = 0;
      rc->svy    = 0;
      rc->wrap   = 0;
      rc->color  = VIO_COLOR_WHITE;
      pq->rtbuf[RTBUF_ANSIBUF] = (u32t)rc;
      return rc;
   }
}

u32t _std vio_setansi(int newstate) {
   ansi_state *state = ansi_getstate();
   if (!state) return 0; else {
      u32t rc = state->in_use;
      if (newstate>=0) {
         state->in_use = newstate?1:0;
         if (!newstate) {
            state->seq    = 0;
            state->str[0] = 0;
            state->wrap   = 0;
            state->color  = VIO_COLOR_WHITE;
            vio_setcolor(VIO_COLOR_RESET);
         }
      }
      return rc;
   }
}

static char *ansi_out(ansi_state *st, char *src) {
   char  *cs = 0;
   u32t   mx, my, px, py;
   int    ps;

   if (st->seq) {
      strncpy(st->str+st->seq, src, ANSI_BUF_LEN-st->seq-1);
      st->str[ANSI_BUF_LEN-1] = 0;
      cs = st->str;
   } else 
   if (*src!=27) {
      vio_charout(*src);
      return src+1;
   } else
   if (src[1]==0) {
      st->str[st->seq++] = 27;
      return src+1;
   } else
   if (src[1]!='[') {
      vio_charout(*src);
      return src+1;
   } else
   if (src[2]==0) {
      st->str[st->seq++] = 27;
      st->str[st->seq++] = '[';
      return src+2;
   } else 
      cs = src;

   vio_getmodefast(&mx, &my);
   vio_getpos(&py, &px);

   ps = 3;

   switch (cs[2]) {
      case 's': // saves the current cursor position
         st->svy = py;
         st->svx = px;
         break;
      case 'u': // restores cursor to last position saved by [s
         vio_setpos(st->svy, st->svx);
         break;
      case 'K': { // erases to the end of the current line
         u32t wrlen = py-mx;
         u16t   *mb = (u16t*)malloc(wrlen*2);
         memsetw(mb, st->color<<8|0x20, wrlen);
         vio_writebuf(px, py, wrlen, 1, mb, wrlen*2);
         free(mb);

         vio_setpos(py,0);
         if (!st->wrap) vio_charout('\n');
         break;
      }
      case '=': // sets screen width and mode
         if (!cs[3] || !cs[4]) ps=0; else {
            if (cs[3]=='7') st->wrap = cs[4]=='h';
            ps+=2;
         }
         break;
      default:
         // req. next character if 3rd is digit
         if (cs[2]>='0' && cs[2]<='9' && !cs[3]) ps=0; else
         // 2J => erases the screen and homes the cursor
         if (cs[2]=='2' && cs[3]=='J') { vio_clearscr(); ps++; } else
         /* [6n => outputs the current line and column in the form:
            \x1b[row;clmR (ignored) */
         if (cs[2]=='6' && cs[3]=='n') ps++; else {
            u32t values[32], vp=0;
            ps--;
            while (vp<32) {
               u32t  stpos = ps;
               while (cs[ps]>='0' && cs[ps]<='9') ps++;
               // action char required after digit sequence
               if (ps>stpos && !cs[ps]) { ps=0; break; }
               values[vp] = str2long(cs+stpos);

               if (cs[ps]==';') { vp++; ps++; } else {
                  char cc = cs[ps++];
                  if (!cc) { ps=0; break; }
                  if (cc!='m') {
                     if (values[0]==0) values[0]=1;
                     if (values[1]==0) values[1]=1;
                  }
                  switch (cc) {
                     case 'A': {
                        int newy = (int)py-(int)values[0];
                        if (newy<0) newy=0;
                        if (py!=newy) vio_setpos(py=newy, px);
                        break;
                     }
                     case 'B': {
                        u32t newy = py+values[0];
                        if (newy>=my) newy = my-1;
                        if (py!=newy) vio_setpos(py=newy, px);
                        break;
                     }
                     case 'C':
                        while (px<mx-1 && values[0]--) px++;
                        vio_setpos(py, px);
                        break;
                     case 'D':
                        while (px&&values[0]--) px--;
                        vio_setpos(py, px);
                        break;
                     case 'H': case 'f': {
                        u32t ppx = px;
                        px = values[1]-1; 
                        py = values[0]-1;
                        if (px>=mx) px = mx-1;
                        if (py>=my) py = my-1;
                        vio_setpos(py, px);
                        break;
                     }
                     case 'm': {
                        u32t ll;
                        for (ll=0;ll<=vp;ll++) {
                           u32t vv = values[ll];
                           switch (vv) {
                              case 0: st->color = VIO_COLOR_WHITE; break;
                              case 1: st->color|= 0x08; break;
                              case 5: st->color|= 0x80; break;
                              case 7: st->color = VIO_COLOR_WHITE<<8; break;
                              case 8: st->color = 0; break;
                              default: {
                                 static u8t col_rec[8] = {VIO_COLOR_BLACK, VIO_COLOR_RED,
                                    VIO_COLOR_GREEN, VIO_COLOR_BROWN, VIO_COLOR_BLUE,
                                    VIO_COLOR_MAGENTA, VIO_COLOR_CYAN, VIO_COLOR_WHITE};
                              
                                 if (vv>=30 && vv<38)
                                    st->color = st->color&0xF8|col_rec[vv-30];
                                 else
                                 if (vv>=40 && vv<48)
                                    st->color = st->color&0x8F|col_rec[vv-40]<<4;
                              }
                           }
                        }
                        vio_setcolor(st->color);
                        break;
                     }
                  }
                  break;
               }
            }
         }
   }
   // sequence is not finished, more of chars required
   if (!ps) {
      if (cs==src) {
         st->seq = strlen(src);
         memcpy(st->str, src, st->seq);
         src    += st->seq;
      }
   } else {
      if (cs!=src) {
         ps     -= st->seq;
         st->seq = 0;
      }
      src += ps;
   }
   return src;
}

void ansi_strout(char *str) {
   ansi_state *st = ansi_getstate();
   u32t       thf = mt_tlsget(QTLS_FORCEANSI);
   // thread local forced state?
   if (thf) thf--; else thf = st->in_use;
   // ansi off? nice!
   if (!thf) vio_strout(str); else {
      // flush unfinished ESC sequence
      if (st->seq) str = ansi_out(st, str);
      if (!*str) return;

      if (!st->wrap && !strchr(str,27)) vio_strout(str); else {
         u32t  mx;
         if (st->wrap) vio_getmodefast(&mx,0);
      
         while (*str) {
            char cc = *str;
            if (cc==27) str = ansi_out(st, str); else {
               str++;
               if (st->wrap) {
                  u32t  px,py;
                  vio_getpos(&py, &px);
                  // wrap output at the right border of screen
                  if (px==mx-1 && cc!=10 && cc!=13) {
                     u16t  pv = cc|st->color<<8;
                     vio_writebuf(px, py, 1, 1, &pv, 2);
                     vio_setpos(py, 0);
                     continue;
                  }
               }
               vio_charout(cc);
            }
         }
      }
   }
}

/** calculate string length without embedded ANSI sequences.
    @param str      source string
    @return string length without ANSI sequences in it */
u32t _std str_length(const char *str) {
   if (!str) return 0; else {
      u32t  len = 0;
      char  *cs = (char*)str, *cp;

      while ((cp = strchr(cs, '\x1B'))) {
         char *se = strpbrk(cp,"ABCDJHKfhlmnsu");
         len += cp-cs;
         if (se) cs = se+1; else 
            return len; // truncated ANSI seq, ignore it
      }
      return len + strlen(cs);
   }
}

void _std vio_fillrect(u32t col, u32t line, u32t width, u32t height, char ch, u16t color) {
   u32t mx, my;
   vio_getmodefast(&mx, &my);
   if (col>=mx || line>=my) return;
   if (col+width>mx) width = mx - col;
   if (line+height>my) height = my - line;

   if (!width || !height) return; else {
      u16t *vb = (u16t*)malloc_th(width*2);
      if (!ch) ch = ' ';
      memsetw(vb, color<<8|ch, width);
      while (height--) vio_writebuf(col, line++, width, 1, vb, 0);
      free(vb);
   }
}
