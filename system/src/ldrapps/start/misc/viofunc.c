//
// QSINIT "start" module
// keyboard functions
//
#include "vioext.h"
#include "stdlib.h"
#include "qsutil.h"
#include "qslog.h"
#include "internal.h"

extern u32t _std NextBeepEnd;

#define TEXTMEM_SEG  0xB800
void _std vio_getmodefast(u32t *cols, u32t *lines);

#if 0
typedef struct {
   u32t        line;     ///< screen line
   u32t         col;     ///< screen column of 1st char
   u32t       width;     ///< visible width of editing line
   long         pos;     ///< current position on screen (without col)
   long      scroll;     ///< number of chars scrolled left
   long        clen;     ///< current string length
   long       bsize;     ///< string buffer length (malloc)
   char      *rcstr;     ///< current string
   u16t    defshape;     ///< original curtor shape
   u32t    userdata;     ///< userdata for callback, initialized with 0
} key_strcbinfo;

typedef int _std (*key_strcb)(u16t key, key_strcbinfo *inputdata);
#endif

char* _std key_getstr(key_strcb cbfunc) {
   return key_getstrex(cbfunc, -1, -1, -1, 0);
}

char* _std key_getstrex(key_strcb cbfunc, int col1, int line1, int len1, const char *init) {
   u32t  mode_x, mode_y;
   char    *buf;
   key_strcbinfo     localdata;
   #define  line     localdata.line
   #define  col      localdata.col      
   #define  width    localdata.width    
   #define  pos      localdata.pos      
   #define  scroll   localdata.scroll   
   #define  clen     localdata.clen     
   #define  bsize    localdata.bsize    
   #define  defshape localdata.defshape 
   #define  rc       localdata.rcstr
   #define  userdata localdata.userdata
   userdata = 0;
   defshape = vio_getshape();
   vio_getpos(&line,&col);
   vio_getmodefast(&mode_x,&mode_y);

   if (col1>=0 && col1<mode_x-1) col=col1;
   if (line1>=0 && line1<mode_y) line=line1;

   width = len1>0?len1:mode_x-1;
   if (col+width+1>=mode_x) width = mode_x-col-1;
   buf   = (char*)malloc(mode_x);

   if (init) {
      rc    = strdup(init);
      bsize = memBlockSize(rc);
      clen  = strlen(rc);

      if (clen>=width) {
         scroll = clen-width+1;
         pos    = clen-scroll;
      } else {
         scroll = 0;
         pos    = clen;
      }
   } else {
      pos=0; bsize=1024; clen=0; scroll=0; 
      rc  = (char*)malloc(bsize);
      *rc = 0;
   }
   //log_printf("pos: x=%d,y=%d,width=%d\n",col,line,width);

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
         if (log_hotkey(key)) continue;

         if (cbfunc) {
            int cbres=cbfunc(key,&localdata);
            if (cbres==-1) { free(rc); rc=0; }
            if (cbres<0) break;
            all = cbres;
         }
      }

      if (all&2) all&=1; 
         else
      if (keyl>=0x20&&keyl<=0x7E) {
         if (clen+2>bsize) rc = (char*)realloc(rc, bsize=(clen+2)*2);
         if (pos+scroll>=clen) rc[clen] = keyl; else {
            memmove(rc+pos+scroll+1, rc+pos+scroll, clen-(pos+scroll));
            rc[pos+scroll] = keyl;
         }
         rc[++clen]=0; mve=1;
      } else
      if (keyl==27) {
         scroll=0; pos=0; all=1; *rc=0; clen=0;
      } else
      if (keyl==13) break; else
      switch (keyh) {
         case 0x47: scroll=0; pos=0; all=1;     break; // home
         case 0x4D: if (pos+scroll<clen) mve=1; break; // right
         case 0x4B: if (pos+scroll>0) mve=-1;   break; // left
         case 0x4F: // end
            scroll=0;
            if (clen<=width) pos=clen; else
               scroll=clen-(pos=width);
            all=1;
            break;
         case 0x0E: // backspace
            if (pos+scroll>0 && clen) {
               if (*rc) memmove(rc+pos+scroll-1, rc+pos+scroll, clen-(pos+scroll)+1);
               if (scroll) { scroll--; all=1; } else mve=-1;
               rc[--clen]=0;
            }
            break;
         case 0x53: // del
            if (pos+scroll>=0 && pos+scroll<clen ) {
               if (*rc) memmove(rc+pos+scroll, rc+pos+scroll+1, clen-(pos+scroll));
               rc[--clen]=0;
               all=1;
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
         vio_setshape(0x20,0);
         vio_setpos(line,col);
         strncpy(buf,rc+scroll,width); buf[width]=0;
         vio_strout(buf);
         memset(buf,0x20,width);
         //log_printf("left space: %d, scroll: %d, clen: %d\n",clen-scroll,scroll,clen);
         if ((clen-scroll)<width) vio_strout(buf+(clen-scroll));
         vio_setpos(line,col+pos);
         vio_setshape(defshape>>8,defshape);
         //         pos       clen   width
         //  |||||------------      |
      }
   } while(1);

   //printf("\n(%s)\n",rc);
   free(buf);
   return rc;
}
#undef line
#undef col
#undef width

static void vio_bufcommon(u32t col, u32t line, u32t width, u32t height,
   void *buf, u32t pitch, int tosrc)
{
   u32t cols, lines;
   vio_getmodefast(&cols,&lines);
   if (line>=lines||col>=cols) return;
   if (!pitch) pitch = width*2;
   if (col+width  > cols ) width  = cols - col;
   if (line+height> lines) height = lines- line;
   if (width && height) {
      u8t *bscr = (u8t*)hlp_segtoflat(TEXTMEM_SEG) + (line*cols + col)*2,
          *bptr = (u8t*)buf;
      for (;height>0;height--) {
         memcpy(tosrc?bscr:bptr,tosrc?bptr:bscr,width*2);
         bscr+=cols*2; bptr+=pitch;
      }
   }
}

/* both functions exported to allow catch MSGBOX calls from outside */

void _std START_EXPORT(vio_writebuf)(u32t col, u32t line, u32t width, u32t height, 
   void *buf, u32t pitch)
{
   vio_bufcommon(col, line, width, height, buf, pitch, 1);
}

void _std START_EXPORT(vio_readbuf)(u32t col, u32t line, u32t width, u32t height, 
   void *buf, u32t pitch)
{
   vio_bufcommon(col, line, width, height, buf, pitch, 0);
}

u8t _std vio_beepactive(void) {
   return NextBeepEnd?1:0;
}

/* this is stub, it will be replaced by CONSOLE.DLL on loading to allow virtual
   console modes */
int _std vio_setmodeex(u32t cols, u32t lines) {
   if (cols!=80) return 0;
   if (lines!=25 && lines!=43 && lines!=50) return 0;
   vio_setmode(lines);
   return 1;
}
