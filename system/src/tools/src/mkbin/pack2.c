#include "sp_defs.h"

#define TEXTSEARCH  768  // Max strings to search in text file - smaller -> Faster compression
#define BINSEARCH   192  // Max strings to search in binary file
#define TEXTNEXT     64  // Max search at next character in text file - Smaller -> better compression
#define BINNEXT      16  // Max search at next character in binary file
#define MAXFREQ    2000  // Max frequency count before table reset
#define MINCOPY       3  // Shortest string COPYING length
#define MAXCOPY      66  // Longest string COPYING length
#define SHORTRANGE    3  // Max distance range for shortest length COPYING
#define COPYRANGES    7  // Number of string COPYING distance bit ranges @@@

static const int CopyBits[COPYRANGES]={4,6,8,10,12,14,14}; // Distance bits

#define CODESPERRANGE (MAXCOPY - MINCOPY + 1)
#define CODESPERRANGE_SHIFT   6            //(MAXCOPY - MINCOPY + 1)

#define NUL          -1  // End of linked list marker
#define HASHSIZE  32768  // Number of entries in hash table}(*Don't change*)
#define HASHMASK (HASHSIZE-1)  // Mask for hash key wrap

// Adaptive Huffman variables
#define TERMINATE   256  // EOF code
#define FIRSTCODE   257  // First code for COPYING lengths
#define SIX_MAXCHAR (FIRSTCODE+COPYRANGES*CODESPERRANGE-1) // 704
#define SUCCMAX     (SIX_MAXCHAR+1)                        // 705
#define TWICEMAX  (2*SIX_MAXCHAR+1)                        // 1409
#define ROOT          1
#define MAXBUF     4096

/* Bit packing routines */


typedef int Copy_Type[COPYRANGES]; // Distance bits

static const Copy_Type CopyMin = {0,16,80,320,1024,4096,16384};
static const Copy_Type CopyMax = {15,79,319,1023,4095,16383,32768-MAXCOPY};

#define MaxDistance  (32768-MAXCOPY)
#define MaxSize       32768
#define MaxSizeMask  0x7FFF

typedef l        HashType[HASHSIZE];
typedef l                 *Hash_Ptr;
typedef l       ListType[MaxSize+1];
typedef l                 *List_Ptr;
typedef b    Buffer_Type[MaxSize+1]; // Convenient typecast.
typedef b               *Buffer_Ptr;
typedef l HTree_Type[SIX_MAXCHAR+1];
typedef l   THTree_Type[TWICEMAX+1];
typedef l       WDBufType[MAXBUF*4];
typedef WDBufType         *WDBufPtr;


static l          Distance,Insrt;
static l        DictFile, Binary;
static Hash_Ptr       Head, Tail;     // Hash table
static List_Ptr       Next, Prev;     // Double linked lists
static Buffer_Ptr         Buffer;     // Text buffer
static HTree_Type  LeftC, RightC;     // Huffman tree
static THTree_Type   Parent,Freq;
static b*                 CurBuf;
static l         Input_Bit_Count;
static l        Input_Bit_Buffer;
static l        Output_Bit_Count;
static d       Output_Bit_Buffer;

static void Initialize() {
   l ii;
   for (ii=2;ii<=TWICEMAX;ii++) { Parent[ii]=ii>>1; Freq[ii]=1; }
   for (ii=1;ii<=SIX_MAXCHAR;ii++) { LeftC[ii]=ii<<1;RightC[ii]=(ii<<1)+1; }
}

/* Flush any remaining bits to output file before closing file */
static void Flush_Bits() {
   if (Output_Bit_Count>0) {
      Output_Bit_Buffer<<=32-Output_Bit_Count;
      *(d*)CurBuf=Output_Bit_Buffer; CurBuf+=4;
      Output_Bit_Count=0;
   }
}

/* Update frequency counts from leaf to root */
static void Update_Freq(l A,l B) {
   do {
      Freq[Parent[A]]=Freq[A]+Freq[B];
      A=Parent[A];
      if (A!=ROOT)
         B=LeftC[Parent[A]]==A?RightC[Parent[A]]:LeftC[Parent[A]];
   } while (A!=ROOT);
   if (Freq[ROOT]==MAXFREQ)
      for (A=1;A<=TWICEMAX;A++) Freq[A]>>=1;
}

/* Update Huffman model for each character code */
static void FAST Update_Model(l Code) {
   l A=Code+SUCCMAX,B,C,ua,Uua;
 
   ++Freq[A];
   if (Parent[A]!=ROOT) {
      ua=Parent[A];
      Update_Freq(A,LeftC[ua]==A?RightC[ua]:LeftC[ua]);
      do {
         Uua=Parent[ua];
         B  =LeftC[Uua]==ua?RightC[Uua]:LeftC[Uua];
         if (Freq[A]>Freq[B]) {
            if (LeftC[Uua]==ua) RightC[Uua]=A; else LeftC[Uua]=A;
            if (LeftC[ua]==A) { LeftC[ua]=B; C=RightC[ua]; } else
              { RightC[ua]=B; C=LeftC[ua]; }
            Parent[B]=ua;
            Parent[A]=Uua;
            Update_Freq(B,C);
            A=B;
         }
         A =Parent[A];
         ua=Parent[A];
      } while (ua!=ROOT);
   }
}

#define Check32() {                    \
   if (++Output_Bit_Count==32) {       \
      *(d*)CurBuf=Output_Bit_Buffer;   \
      CurBuf+=4;                       \
      Output_Bit_Count=0;              \
   }                                   \
}

static void FAST Output_Code(l Bits,d Code) {
   while (Bits--) {                         
      Output_Bit_Buffer<<=1;                 
      Output_Bit_Buffer|=Code&1;
      Check32();
      Code>>=1;
   }                                        
}

/* Compress a character code to output stream */
static void _Compress(l Code) {
   l A=Code+SUCCMAX, _sp=0, Stack[50];
   do {
      Stack[_sp++]=RightC[Parent[A]]==A; A=Parent[A];
   } while (A!=ROOT);
   do {
      Output_Bit_Buffer<<=1;
      if (Stack[--_sp]) Output_Bit_Buffer|=1;
      Check32();
   } while (_sp);
   Update_Model(Code);
}

/** Hash table linked list string search routines **/

#define CalcHash(Node) ((Buffer[Node]^(Buffer[Node+1&MaxSizeMask]<<4)^  \
    (Buffer[Node+2&MaxSizeMask]<<8))&HASHMASK)

static void Add_Node(l N) {
   l Key=CalcHash(N);
   if (Head[Key]==NUL) {
      Tail[Key]=N;
      Next[N]  =NUL;
   } else {
      Next[N]=Head[Key];
      Prev[Head[Key]]=N;
   }
   Head[Key]=N;
   Prev[N]  =NUL;
}

/* Delete node from tail of list */
static void Delete_Node(l Node) {
   l Key=CalcHash(Node);
   if (Head[Key]==Tail[Key]) Head[Key]=NUL; else {
      Next[Prev[Tail[Key]]]=NUL;
      Tail[Key]=Prev[Tail[Key]];
   }
}

/* Find longest string matching lookahead buffer string */
static l Match(l N,l Depth) {
   l  ii, jj, Index, Key, Dist, Len, Best=0, Count=0;
   if (N==MaxSize) N=0;
   Key  =CalcHash(N);
   Index=Head[Key];
   while (Index!=NUL) {
      if (++Count>Depth) break;
      if (Buffer[N+Best&MaxSizeMask]==Buffer[Index+Best&MaxSizeMask]) {
         Len=0; ii=N; jj=Index;
         while (Buffer[ii]==Buffer[jj]&&Len<MAXCOPY&&jj!=N&&ii!=Insrt) {
            Len++;
            if (++ii==MaxSize) ii=0;
            if (++jj==MaxSize) jj=0;
         }
         Dist=N-Index;
         if (Dist<0) Dist+=MaxSize;
         Dist-=Len;
         if (DictFile&&Dist>CopyMax[0]) break;
         if (Len>Best&&Dist<=MaxDistance) {
            if (Len>MINCOPY||Dist<=CopyMax[SHORTRANGE+Binary]) {
               Best=Len;
               Distance=Dist;
            }
         }
      }
      Index=Next[Index];
   }
   return Best;
}

/** Finite Window compression routines **/
#define IDLE     0   // Not processing a COPYING
#define COPYING  1   // Currently processing COPYING

static void Dictionary() {
   l ii=0, jj=0, kk, count=0;
   while (++jj<MINCOPY+MAXCOPY) {
      if (Buffer[jj-1]==10) {
         kk=jj;
         while (Buffer[ii]==Buffer[kk]) { ii++;kk++;count++; }
         ii=jj;
      }
   }
   if (count>(MINCOPY+MAXCOPY>>2)) DictFile=1;
}

#define CompressFinite() {                                       \
   _Compress(TERMINATE);                                         \
   Flush_Bits();                                                 \
   free(Head); free(Tail); free(Next); free(Prev); free(Buffer); \
   return CurBuf-Destin;                                         \
}

l Compress(l Length, b *Source, b *Destin) {
   l cc=0, ii, nn=MINCOPY, Addpos=0, Len=0, Full=0, State=IDLE, StLen=Length;
 
   DictFile=0; Binary=0; Input_Bit_Count=0; Input_Bit_Buffer=0;
   Output_Bit_Count=0; Output_Bit_Buffer=0; CurBuf=Destin;

   Initialize();
   Head  = malloc(sizeof(HashType));
   Tail  = malloc(sizeof(HashType));
   Next  = malloc(sizeof(ListType));
   Prev  = malloc(sizeof(ListType));
   Buffer= malloc(sizeof(Buffer_Type));
 
   for (ii=0;ii<HASHSIZE;ii++) Head[ii]=NUL;
 
   for (ii=0;ii<MINCOPY;ii++) {
      cc=*Source++;
      if (--Length==0) CompressFinite();
      _Compress(cc);
      Buffer[ii]=cc;
   }
 
   Insrt=MINCOPY;
   for (ii=0;ii<MAXCOPY;ii++) {
      cc=*Source++;
      if (--Length==0) break;
      Buffer[Insrt++]=cc;
      if (cc>127) Binary=1;
   }
   Binary=1;
//   Dictionary();
 
   while (nn!=Insrt) {
/*     if (DictFile&&(StLen-Length)%MAXCOPY==0)
      if ((StLen-Length)/(CurBuf-Destin)<2) DictFile=0;*/
      
      if (Full) Delete_Node(Insrt);
      Add_Node(Addpos);
      
      if (State==COPYING) { if (--Len==1) State=IDLE; } else {
         l NextLen=Match(nn+1,Binary?  BINNEXT:  TEXTNEXT);
         Len      =Match(nn  ,Binary?BINSEARCH:TEXTSEARCH);
         if (Len>=MINCOPY&&Len>=NextLen) {
            State=COPYING;
            for (ii=0;ii<COPYRANGES;ii++) {
               if (Distance<=CopyMax[ii]) {
                  _Compress(FIRSTCODE-MINCOPY+Len+ii*CODESPERRANGE);
                  Output_Code(CopyBits[ii],Distance-CopyMin[ii]);
                  break;
               }
            }
         } else _Compress(Buffer[nn]);
      }
      
      if (++nn==MaxSize) nn=0;
      if (++Addpos==MaxSize) Addpos=0;
      
      if (Length>0) {
         Buffer[Insrt++]=*Source++; Length--;
         if (Insrt==MaxSize) { Insrt=0; Full=1; }
      } else Full=0;
   }
   CompressFinite();
}
