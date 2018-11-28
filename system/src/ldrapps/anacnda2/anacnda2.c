/* 
   code was digged up somewhere over net, it terrible, slow, ugly and wrong.
   but, let work as it is ;)
*/
#include "clib.h"
#include "vio.h"
#include "qsutil.h"

#define textcolor(x) vio_setcolor(x)
#define clrscr vio_clearscr
#define gotoxy(x,y) vio_setpos(y,x)
#define getch() (key_read()&0xFF)
#define kbhit() key_pressed()
#define WHITE      0x7
#define LIGHTGREEN 0xA
#define LIGHTRED   0xC
#define LIGHTCYAN  0xB
#define BROWN      0x6
#define timer (tm_counter())

extern u32t _RandSeed;
#define randomize() _RandSeed=timer
u32t random(u32t range);

int len;
long int SPEED = 4000000;
int cur_score=0;

int getkey(void) {
  int key,lo,hi;
  key=key_read();
  lo=key & 0x00FF;
  hi=(key & 0xFF00)>>8;
  return ((lo==0||lo==0xE0)?hi+256:lo);
}

void start_graphic();
void function(int len0,int x,int y,int xt,int yt,int xf, int yf,
              int px[],int py[],long int speed,char a[]);
int isamong(int c,int c1,int ch[],int ch1[],int n);
void waitfor(long int k);
void wait();

void hline() {
   int i;
   for (i=0;i<80;i++) vio_charout('Ä');
   return;
}
/********************************************************************/

//////////////////////// MAIN STARTS ///////////////////////////////
void main() {
   char a[500],s='O',name[80]; //s=skin
   int px[500],py[500],len0,xt,yt,xf,yf,i,j,r,rx,ry,x,y; //xt=xtemporary  xf=xfood
   int  top=3,bottom=24,left=1,right=79;    //r = range of food //x,y of tail
   u32t cols, lines;
   int  move,current,ans;

   start_graphic();
   clrscr();
   
   for (i=0;i<80;i++) name[i]=' ';
   vio_strout("NOTE:\n\nThe Anaconda is not beautiful... but then... It is not meant to be either...!!!");

newgame:
   vio_setshape(VIO_SHAPE_NONE);

   if (!vio_getmode(&cols, &lines) || cols!=80 || lines>30) vio_resetmode(); else {
      clrscr();
      if (lines - 25 > 1) {
         u32t ii;
         vio_setpos(25, 1);
         for (ii=0; ii<78; ii++) vio_charout(0xCD);
      }
   }
   cur_score=0;
   randomize();

//***************** create and display initial anaconda ****************
   len0=10;  // initial length
   len=len0;
   for (i=0;i<len;i++) { a[i]=s; px[i]=i+5; py[i]=14;}

   for (i=0;i<len;i++) { 
      gotoxy(px[i],py[i]);       
      vio_charout(a[i]); 
   }
   r=4;   rx=74; ry=19;
   xf=random(rx)+r;
   yf=random(ry)+r;
   gotoxy(xf,yf); vio_charout(s);

//************************** start game *******************************
   do {
      move=getkey();
   } while (move!=331 && move!=333 && move!=328 && move!=336 && move!=27);

   //NOTE : BIOS codes for arrowkeys : l,r,u,d : 331,333,328,336

   do {
      switch (move)
      {
         case 333:
            if (current==331) { move=current; break; } //backward not allowed
            current=333;
            do {
               x=px[0]; y=py[0];
               xt=px[len-1]; yt=py[len-1];        // [len-1] represents "head"
               if(xt==right) xt-=(right-left);    // to allow " aar paar " through
               xt++;              // screen
               if(isamong(xt,yt,px,py,len)) {   // self collision
                  gotoxy(1,1); vio_strout("G A M E   O V E R ! ! ! ");
                  goto end;
               }
               function(len0,x,y,xt,yt,xf,yf,px,py,SPEED/2,a);
                          // function shows current frame
               
               if(xt==xf && yt==yf) {            // check and eat food
                  a[len]='O'; px[len]=xt; py[len]=yt; len++;
                  xf=random(rx)+r;
                  yf=random(ry)+r;                // show new food
                  gotoxy(xf,yf); vio_charout('O');
               }
            } while (!kbhit());   // run anaconda till key press
            move=getkey();      // retrieve the key pressed
            break;
         //----------------------------------------------------------
         case 331:
            if(current==333) {move=current; break;}
            current=331;
            do {
               x=px[0]; y=py[0];
               xt=px[len-1]; yt=py[len-1];
               if(xt==left) xt+=(right-left);
               xt--;
               if (isamong(xt,yt,px,py,len)) {
                  gotoxy(1,1); vio_strout("G A M E   O V E R ! ! ! ");
                  goto end;
               }
               
               function(len0,x,y,xt,yt,xf,yf,px,py,SPEED/2,a);
               if(xt==xf && yt==yf) {
                  a[len]='O'; px[len]=xt; py[len]=yt; len++;
                  xf=random(rx)+r;
                  yf=random(ry)+r;
                  gotoxy(xf,yf); vio_charout('O');
               }
            } while (!kbhit());
            move=getkey();
            break;
         //------------------------------------------------------------
         case 336:
     
            if(current==328) {move=current; break;}
            current=336;
            do {
               x=px[0]; y=py[0];
               xt=px[len-1]; yt=py[len-1];
               if(yt==bottom) yt-=(bottom-top);
               yt++;
               if (isamong(xt,yt,px,py,len)) {
                  gotoxy(1,1); vio_strout("G A M E   O V E R ! ! ! ");
                  goto end;
               }
               function(len0,x,y,xt,yt,xf,yf,px,py,SPEED,a);
               if(xt==xf && yt==yf) {
                  a[len]='O'; px[len]=xt; py[len]=yt; len++;
                  xf=random(rx)+r;
                  yf=random(ry)+r;
                  gotoxy(xf,yf); vio_charout('O');
               }
            } while (!kbhit());
            move=getkey();
            break;
         //----------------------------------------------------------
         case 328:
     
            if(current==336) {move=current; break;}
            current=328;
            do {
               x=px[0]; y=py[0];
               xt=px[len-1]; yt=py[len-1];
               if(yt==top) yt+=(bottom-top);
               yt--;
               if(isamong(xt,yt,px,py,len)) {
                  gotoxy(1,1); vio_strout("G A M E   O V E R ! ! ! ");
                  goto end;
               }
               function(len0,x,y,xt,yt,xf,yf,px,py,SPEED,a);
               if(xt==xf && yt==yf) {
                  a[len]='O'; px[len]=xt; py[len]=yt; len++;
                  xf=random(rx)+r;
                  yf=random(ry)+r;
                  gotoxy(xf,yf); vio_charout('O');
               }
            } while(!kbhit());
            move=getkey();
            break;
         //---------------------------------------------------------------
         case 27:
            clrscr();
            vio_strout("Game Aborted !!!                            Press Esc to exit");
            {
               char score[64];
               snprintf(score,64,"\nYour score : %d",(len-len0)*10);
               vio_strout(score);
            }
            goto end;
         default :       // for invalid key presses
            move=current;
            break;
      }
   } while(1);
//*********************** exit options **************************
end:
   vio_setshape(VIO_SHAPE_LINE);
   getch();
   clrscr();
   vio_strout("\n Press ENTER : New GAME");
   vio_strout("\n         ESC : To EXIT");

   ans=getkey();
   if (ans==27) { textcolor(WHITE); clrscr(); }
      else goto newgame;
}
/////////////////////////// MAIN ENDS //////////////////////////////////////

void function (int len0,int x,int y,int xt,int yt,int xf, int yf,
           int px[],int py[],long int speed,char a[])
{
   char score[64];
   int i,j;
   for(j=1;j<len;j++) { px[j-1]=px[j]; py[j-1]=py[j]; }
   px[len-1]=xt; py[len-1]=yt;
   waitfor(speed);

   gotoxy(x,y); vio_charout(' ');
   textcolor(LIGHTGREEN);
   for(i=0;i<len;i++) { gotoxy(px[i],py[i]); vio_charout(a[i]); }
   textcolor(LIGHTRED);
   gotoxy(xf,yf); vio_charout('O');
   textcolor(LIGHTCYAN);
   gotoxy(1,1);
   cur_score=(len-len0)*10;
   snprintf(score,64,"A N A C O N D A                   Top Score : %d        ",cur_score);
   vio_strout(score);
   vio_strout("\n Press Esc to EXIT");
}
//************************************************************
int isamong(int c,int c1,int ch[],int ch1[],int n)
{                                   // for collision check
   int i,t=0;
   for (i=0;i<n;i++) {
      if(ch[i]==c && ch1[i]==c1) { t=1; break;}
   }
   return t;
}
//************************************************************
void start_graphic()          // show starting graphics , messages
{
   char c;
   clrscr();
   vio_strout("Loading...\n\n\n\n\n\n\n\n\n");
   vio_strout("                   A   N   A   C   O   N   D   A");
   wait();
   vio_strout("\n\n               If You Can't Breathe...");
   wait(); wait();
   vio_strout("\n\n               How Will You Scream.....  ?");
   gotoxy(1,1);
   wait(); wait(); wait();
   clrscr();
   vio_strout("\nANACONDA  is behind you...\n");
   hline();
   vio_strout("\nPRESS ANY KEY : TO CONTINUE WITH DEFAULT SETTINGS...");
   vio_strout("\n          'S' : to adjust speed\n\n");
   hline();
   c=getch();
   if (c=='s'||c=='S') {
      char dch = 0;
      clrscr();
      vio_strout("Select Level : \n"
                 "\nEnter 1: Baby"
                 "\n      2: Intermediate ( Default )"
                 "\n      3: Challange"
                 "\n      4: You need to have strong RELATIVITY CONCEPTS !"
                 "\n\nEnter Your Choice : ");

      while (!dch) {
         dch = getch();
         switch (dch) {
            case '1':
              SPEED=5000000; break;
            case '2':
              SPEED=4000000; break;
            case '3':
              SPEED=3000000; break;
            case '4':
              SPEED=2000000; break;
            default:
              dch = 0;
         }
      }
   }
}

/*******************************************************************/

void waitfor(long int k) {
   long int i, cnt=0;

   for (i=0;i<k/1000000;i++) {
      volatile long int ts=timer;
      while (timer-ts==0) cnt++;
   }
}

/********************************************************************/
void wait() {
   long int i;
   for(i=0;i<99999999;i++) ;
}
