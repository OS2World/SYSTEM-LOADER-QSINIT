#include "clib.h"
#include "vio.h"
#include "qsutil.h"

#define textcolor(x) vio_setcolor(x)
#define clrscr vio_clearscr
#define gotoxy(x,y) vio_setpos((y)-1+base_y,(x)-1)
#define timer (tm_counter())

#define SIZE_X     14
#define SIZE_Y     23
#define POS_INF0   62
#define LEVEL_STEP 500

u32t random(u32t range);

typedef u16t fig[4][4];

u16t glass[SIZE_Y][SIZE_X];
u8t  gcol [SIZE_Y][SIZE_X];
int       x,y,rotate,level,
                  shownext = 0;
long        score,nextTIME,
                 prevshore = -1,
                   prevlvl = -1;
u16t        figCUR,figNEXT;
u16t                base_y = 0;     ///< offset from top to make it aligned vertically

u8t color[7] = {0x2,0x6,0x9,0xE,0xD,0x7,0x4};

fig rot[7][4] = {
   {
   {{0,0,1,0},{0,0,1,0},{0,0,1,0},{0,0,1,0}},     /*   @@   */
   {{0,0,0,0},{1,1,1,1},{0,0,0,0},{0,0,0,0}},     /*   @@   */
   {{0,0,1,0},{0,0,1,0},{0,0,1,0},{0,0,1,0}},     /*   @@   */
   {{0,0,0,0},{1,1,1,1},{0,0,0,0},{0,0,0,0}}},    /*   @@   */
   {
   {{0,0,0,0},{0,1,1,0},{0,1,0,0},{0,1,0,0}},     /*   @@@@ */
   {{0,0,0,0},{0,1,0,0},{0,1,1,1},{0,0,0,0}},     /*   @@   */
   {{0,0,1,0},{0,0,1,0},{0,1,1,0},{0,0,0,0}},     /*   @@   */
   {{0,0,0,0},{1,1,1,0},{0,0,1,0},{0,0,0,0}}},    /*   @@   */
   {
   {{0,0,0,0},{0,1,1,0},{0,0,1,0},{0,0,1,0}},     /* @@@@   */
   {{0,0,0,0},{0,1,1,1},{0,1,0,0},{0,0,0,0}},     /*   @@   */
   {{0,1,0,0},{0,1,0,0},{0,1,1,0},{0,0,0,0}},     /*   @@   */
   {{0,0,0,0},{0,0,1,0},{1,1,1,0},{0,0,0,0}}},    /*   @@   */
   {
   {{0,0,0,0},{0,1,0,0},{0,1,1,0},{0,1,0,0}},     /*        */
   {{0,0,0,0},{0,1,0,0},{1,1,1,0},{0,0,0,0}},     /* @@     */
   {{0,0,0,0},{0,1,0,0},{1,1,0,0},{0,1,0,0}},     /* @@@@   */
   {{0,0,0,0},{0,0,0,0},{1,1,1,0},{0,1,0,0}}},    /* @@     */
   {
   {{0,0,0,0},{0,1,0,0},{0,1,1,0},{0,0,1,0}},     /*        */
   {{0,0,0,0},{0,0,1,1},{0,1,1,0},{0,0,0,0}},     /* @@     */
   {{0,0,0,0},{0,1,0,0},{0,1,1,0},{0,0,1,0}},     /* @@@@   */
   {{0,0,0,0},{0,0,1,1},{0,1,1,0},{0,0,0,0}}},    /*   @@   */
   {
   {{0,0,0,0},{0,0,1,0},{0,1,1,0},{0,1,0,0}},     /*        */
   {{0,0,0,0},{0,1,1,0},{0,0,1,1},{0,0,0,0}},     /*   @@   */
   {{0,0,0,0},{0,0,1,0},{0,1,1,0},{0,1,0,0}},     /* @@@@   */
   {{0,0,0,0},{0,1,1,0},{0,0,1,1},{0,0,0,0}}},    /* @@     */
   {
   {{0,0,0,0},{0,0,0,0},{0,1,1,0},{0,1,1,0}},     /*        */
   {{0,0,0,0},{0,0,0,0},{0,1,1,0},{0,1,1,0}},     /*        */
   {{0,0,0,0},{0,0,0,0},{0,1,1,0},{0,1,1,0}},     /* @@@@   */
   {{0,0,0,0},{0,0,0,0},{0,1,1,0},{0,1,1,0}}}     /* @@@@   */
};


/* optimized a bit - to be fast on serial port console ;) */
void print() {
   char prtbuf[32];
   u16t    buf[64], *pb,
           col = VIO_COLOR_WHITE<<8;
   int   ii,jj;
   for (ii=0; ii<SIZE_Y; ii++) {
      pb    = buf;
      *pb++ = col|'º';
      for (jj=0; jj<SIZE_X; jj++) {
         if (glass[ii][jj]) {
           col = (u16t)gcol[ii][jj]<<8;
           *pb++ = col|'°';
           *pb++ = col|'°';
         } else {
           col = VIO_COLOR_WHITE<<8;
           *pb++ = col|' ';
           *pb++ = col|' ';
         }
      }
      col = VIO_COLOR_WHITE<<8;
      *pb++ = col|'º';
      vio_writebuf(9, ii+base_y, SIZE_X*2+2, 1, &buf, 0);
   }
   col = VIO_COLOR_WHITE<<8;
   pb  = buf;
   *pb++ = col|'ß';
   for (ii=0; ii<SIZE_X; ii++) { *pb++ = col|'ß'; *pb++ = col|'ß';}
   *pb++ = col|'ß';
   vio_writebuf(9, SIZE_Y+base_y, SIZE_X*2+2, 1, &buf, 0);

   for (ii=1;ii<=4;ii++)
      for (jj=1;jj<=4;jj++)
         if (rot[figCUR][rotate-1][ii-1][jj-1]) {
            gotoxy((x+jj)*2+9,y+ii);
            textcolor(color[figCUR]);
            vio_strout("ÛÛ");
         }
   textcolor(0x07);
   if (score!=prevshore) {
      snprintf(prtbuf, 32, "Score:%d        ", prevshore = score);
      gotoxy(POS_INF0,10); vio_strout(prtbuf);
   }
   if (level!=prevlvl) {
      snprintf(prtbuf, 32, "Level:%d        ", prevlvl = level);
      gotoxy(POS_INF0,11); vio_strout(prtbuf);
   }
   if (shownext) {
      gotoxy(POS_INF0,2); vio_strout("NEXT:");
      textcolor(color[figNEXT]);
      for (ii=1;ii<=4;ii++) {
         gotoxy(POS_INF0,2+ii);
         for (jj=1;jj<=4;jj++)
            if (rot[figNEXT][1][ii-1][jj-1]) vio_strout("²²");
               else vio_strout("  ");
      }
      textcolor(0x07);
      shownext = 0;
   } else
      gotoxy(POS_INF0+5,2);
}

void GenNew() {
   rotate=1;
   if (figNEXT==0xFF) {
      figNEXT=random(7);
      figCUR =random(7);
   } else {
      figCUR =figNEXT;
      figNEXT=random(7);
   }
   x=5; y=0;
   nextTIME=timer+15/level;
   shownext = 1;
}

void init() {
   key_speed(0,0);
   textcolor(0x07);
   level   = 1;
   figNEXT = 0xFF;
   score=0;
   clrscr();
   memset(glass,0,sizeof(glass));
   GenNew();
   print();
}

int ok() {
   int ii,jj;
   for (ii=1;ii<=4;ii++)
      for (jj=1;jj<=4;jj++)
         if (rot[figCUR][rotate-1][ii-1][jj-1]) {
            if (ii+y>SIZE_Y||jj+x>SIZE_X||jj+x==0) return 0;
            if (glass[ii-1+y][jj-1+x]) return 0;
         }
   print();
   return 1;
}

void run() {
   while (1) {
      if (key_pressed()) {
         u16t ch, chr;
         ch =key_read();
         chr=ch&0xFF;
         if (chr==0||chr==0xE0) ch>>=8; else ch=chr;
         switch (ch) {
         /*up*/
         case 72:
            rotate=rotate%4+1;
            if (!ok()) rotate=(rotate+3)%4;
            break;
         /*left*/
         case 75:
            x--; if (!ok()) { x++; ch=0; }
            break;
         /*rigth*/
         case 77:
            x++; if (!ok()) { x--; ch=0; }
            break;
         /*down*/
         case ' ':
         case 80:
            do y++; while (ok());
            y--;
            nextTIME=0;
            break;
         /*escape*/
         case 27:
            //memStat();
            return;
         default:
            ch=0;
         }
         if (ch) print();
      }

      if (timer>=nextTIME) {
         y++;
         nextTIME=timer+15/level;
         if (!ok()) {
            int count=0,ii,jj;
            y--;
            for (ii=1;ii<=4;ii++)
               for (jj=1;jj<=4;jj++)
                  if (rot[figCUR][rotate-1][ii-1][jj-1]) {
                     glass[ii-1+y][jj-1+x]=1;
                     gcol [ii-1+y][jj-1+x]=color[figCUR];
                  }
            for (ii=1;ii<=SIZE_Y;ii++) {
               int linedone=1;
               for (jj=1;jj<=SIZE_X;jj++)
                  if (!glass[ii-1][jj-1]) { linedone=0; break; }
               if (linedone) {
                  memmove(gcol[1],gcol[0],sizeof(gcol[0])*(ii-1));
                  memmove(glass[1],glass[0],sizeof(glass[0])*(ii-1));
                  memset(glass[0],0,sizeof(glass[0]));
                  count++; ii=1;
               }
            }
            print();
            switch (count) {
               case 0:score+=1; break;
               case 1:score+=10; break;
               case 2:score+=40; break;
               case 3:score+=80; break;
               case 4:score+=110; break;
            }
            level=score/LEVEL_STEP+1;
            GenNew();
            if (!ok()) return;
         }
      }
   }
}

void done() {
}

void main() {
   u32t     cols, lines;
   /* reset to 80x25 if mode too large for us, but allows to play
      in 80x30 (virtual console mode) */
   if (!vio_getmode(&cols, &lines) || cols!=80 || lines>30) vio_resetmode(); else {
      base_y = (lines - 25) / 2;
      clrscr();
   }
   // hide cursor.
   vio_setshape(VIO_SHAPE_NONE);
   init();
   run();
   done();
   // show cursor
   vio_setshape(VIO_SHAPE_WIDE);
}
