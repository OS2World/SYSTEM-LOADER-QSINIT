#include "clib.h"
#include "vio.h"
#include "qsutil.h"

#define textcolor(x) vio_setcolor(x)
#define clrscr vio_clearscr
#define gotoxy(x,y) vio_setpos((y)-1,(x)-1)
#define timer (tm_counter())

#define SIZE_X     14
#define SIZE_Y     23
#define POS_INF0   62
#define LEVEL_STEP 500

u32t random(u32t range);

typedef u16t fig[4][4];

u16t glass[SIZE_Y][SIZE_X];
u8t  gcol [SIZE_Y][SIZE_X];
int  x,y,rotate,level;
long score,nextTIME;
u16t figCUR,figNEXT;

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


void print() {
   char prtbuf[32];
   int  ii,jj;
   textcolor(0x07);
   for (ii=1;ii<=SIZE_Y;ii++) {
      gotoxy(10,ii);
      vio_charout('º'); // Û
      for (jj=1;jj<=SIZE_X;jj++) {
         if (glass[ii-1][jj-1]) {
           textcolor(0x0|gcol[ii-1][jj-1]);
           vio_strout("±±"); // °±²ß
         } else {
           textcolor(0x07);
           vio_strout("  ");
         }
      }
      textcolor(0x07);
      vio_charout('º');
   }
   for (ii=1;ii<=4;ii++)
      for (jj=1;jj<=4;jj++)
         if (rot[figCUR][rotate-1][ii-1][jj-1]) {
            gotoxy((x+jj)*2+9,y+ii);
            textcolor(color[figCUR]);
            vio_strout("ÛÛ");
         }
   textcolor(0x07);
   gotoxy(10,SIZE_Y+1);
   vio_charout('ß'); //È
   for (ii=1;ii<=SIZE_X;ii++) vio_strout("ßß"); // ÍÍ
   vio_charout('ß'); //¼
   textcolor(0x07);
   snprintf(prtbuf,32,"Score:%d        ",score);
   gotoxy(POS_INF0,10); vio_strout(prtbuf);
   snprintf(prtbuf,32,"Level:%d        ",level);
   gotoxy(POS_INF0,11); vio_strout(prtbuf);
   gotoxy(POS_INF0,2); vio_strout("NEXT:");
   textcolor(color[figNEXT]);
   for (ii=1;ii<=4;ii++) {
      gotoxy(POS_INF0,2+ii);
      for (jj=1;jj<=4;jj++)
         if (rot[figNEXT][1][ii-1][jj-1]) vio_strout("²²");
            else vio_strout("  ");
   }
   textcolor(0x07);
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
}

void init() {
   key_speed(0,0);
   textcolor(0x07);
   level=1;
   figNEXT=0xFF;
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
            x--; if (!ok()) x++;
            break;
         /*rigth*/
         case 77:
            x++; if (!ok()) x--;
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
   u16t oldshape;
   vio_resetmode();
   clrscr();
   // save old shape & hide cursor
   oldshape = vio_getshape();
   vio_setshape(0x20,0);
   init();
   run();
   done();
   // restore cursor
   vio_setshape(oldshape>>8,oldshape&0xFF);
}
