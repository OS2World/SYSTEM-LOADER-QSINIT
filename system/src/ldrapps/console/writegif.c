#include "writegif.h"
#include <stdlib.h>

#define LzwTableSize 4095
int BitMask[17]={0x0000,0x0001,0x0003,0x0007,0x000F,0x001F,0x003F,0x007F,
                 0x00FF,0x01FF,0x03FF,0x07FF,0x0FFF,0x1FFF,0x3FFF,0x7FFF,0xFFFF};

typedef struct {
   union {
      struct { short int Code,Previous; } d1;
      long int d2;
   } dd;
} CodeTableEntry;

typedef struct {
   int            CBits;
   int            HiBit;
   int            bsize;
   u32t       BitBuffer;
   u8t           *rcbuf;
   u32t          rcsize;
   u32t        usedsize;
   u8t          *prevst;
} BitBufferStruct;

#define Table_Size   5021

typedef CodeTableEntry CodeTable[LzwTableSize+1];
typedef int  SaveTable[Table_Size+1];
typedef u8t  CharTable[Table_Size+1];

static int PutBits(int bb,BitBufferStruct *bs) {
   bs->BitBuffer=bs->BitBuffer|bb<<bs->HiBit;
   bs->HiBit+=bs->CBits;
   while (bs->HiBit>8) {
      if (bs->usedsize-bs->rcsize==0) {
         u8t *newbuf=(u8t*)malloc(bs->rcsize*2);
         if (!newbuf) return 0;
         bs->rcsize*=2;
         memcpy(newbuf,bs->rcbuf,bs->usedsize);
         bs->prevst=newbuf+(bs->prevst-bs->rcbuf);
         free(bs->rcbuf);
         bs->rcbuf=newbuf;
      }
      bs->usedsize++;
      bs->prevst[bs->bsize++]=(u8t)bs->BitBuffer;
      bs->HiBit-=8; bs->BitBuffer>>=8;
      if (bs->bsize>255) { 
         *(bs->prevst+=0x100)=0xFF;
         bs->bsize=1;
         bs->usedsize++;
      }
   }
   return 1;
}

void *WriteGIF(int *ressize,          // result size buffer
               void *srcdata,         // source data
               int x,                 // x size
               int y,                 // y size
               GIFPalette *Palette,   // palette
               int trcolor,           // transparent color (-1 to ignore)
               const char *Copyright  // copyright (GIF89 only, can be 0)
              )
{
   if (!ressize) return 0;
   *ressize=0;
   if (!srcdata||!Palette||x<=0||y<=0) return 0; else {
      int rcsize=x*y+1024,xx,yy,result=0,
           GIF89=Copyright||trcolor>=0&&trcolor<256;
      CodeTableEntry *Table=0;
      u8t *rcbuf=(u8t*)malloc(rcsize),
           *bptr=rcbuf,
            *src=(u8t*)srcdata,
         *PixBuf=0,
         *Append=0;
      int  *Code=0,
         *Prefix=0,
           CBits=0;
      if (!bptr) return 0;
      memcpy(bptr,"GIF87a",6);
      if (GIF89) bptr[4]='9';
      bptr[6]=x&0xFF; bptr[7]=x>>8;
      bptr[8]=y&0xFF; bptr[9]=y>>8;
      
      for (xx=0,yy=0;xx<x*y;xx++)
         if (src[xx]>yy) yy=src[xx];
      xx=9; while (yy<1<<xx) xx--; if (xx<0) xx=0;
      bptr[10]=0x80|(xx&7)<<4|xx&7;
      bptr[11]=0; bptr[12]=0;
      yy=3<<(CBits=(xx&7)+1);
      memcpy(bptr+=13,Palette,yy);
      bptr+=yy;
      
      if (GIF89&&Copyright) {
         int len=strlen(Copyright);
         if (len>255) len=255;
         *bptr++=0x21; *bptr++=0xFE;
         *bptr++=len;
         strcpy((char*)bptr,Copyright);
         bptr+=len+1;
      }
      if (GIF89&&trcolor>=0) {
         *bptr++=0x21; *bptr++=0xF9; *bptr++=0x04; *bptr++=0x05;
         *bptr++=0x20; *bptr++=0x00; *bptr++=trcolor; *bptr++=0x00;
      }
      
      *bptr++=0x2C;
      bptr[0]=0; bptr[1]=0; bptr[2]=0; bptr[3]=0;
      bptr[4]=x&0xFF; bptr[5]=x>>8;
      bptr[6]=y&0xFF; bptr[7]=y>>8; bptr[8]=0;
      bptr+=9;
      
      Table =(CodeTableEntry*)malloc(sizeof(CodeTable));
      PixBuf=(u8t*) malloc(LzwTableSize+2);
      Code  =(int*) malloc(sizeof(SaveTable));
      Prefix=(int*) malloc(sizeof(SaveTable));
      Append=(u8t*) malloc(sizeof(CharTable));
      
      *bptr++=CBits;
      
      if (Table&&PixBuf&&Code&&Prefix&&Append) {
         int  CCode,OldCode;
         int    CC=1<<CBits,
                   EOI=CC+1,
           CTableSize=EOI+1;
         u8t         *S=src;
         int   Index,Offset;
         u32t    Length=x*y;
         BitBufferStruct bs;
         bs.CBits     = CBits+1;
         bs.HiBit     = 0;
         bs.bsize     = 1;
         bs.BitBuffer = 0;
         bs.rcbuf     = rcbuf;
         bs.rcsize    = rcsize;
         bs.usedsize  = bptr-rcbuf+1;
         bs.prevst    = bptr;
         
         for (xx=0;xx<=Table_Size;xx++) Code[xx]=-1;
         *bptr++=0xFF; 
         
         do {
            if (!PutBits(CC,&bs)) break;
            OldCode=*S++; Length--;
            while (Length) {
               CCode=*S++; Length--; Index=CCode<<4^OldCode;
               Offset =Index?Table_Size-Index:1;
               while (1) {
                  if (Code[Index]==-1) break;
                  if (Prefix[Index]==OldCode&&Append[Index]==CCode) break;
                  Index-=Offset;
                  if (Index<0) Index+=Table_Size;
               }
               if (Code[Index]!=-1) OldCode=Code[Index]; else {
                  if (!PutBits(OldCode,&bs)) break;
                  if (CTableSize<=4095) {
                     Code[Index]  = (short)CTableSize++;
                     Prefix[Index]= (short)OldCode;
                     Append[Index]= (u8t)CCode;  
                     if ((CTableSize-1)>>bs.CBits) bs.CBits++;
                  } else {
                     if (!PutBits(CC,&bs)) break; 
                     CTableSize=EOI+1; bs.CBits=CBits+1;
                     for (xx=0;xx<=Table_Size;xx++) Code[xx]=-1;
                  }
                  OldCode=CCode;
               }
            }
            if (!PutBits(OldCode,&bs)) break;
            if (!PutBits(EOI,&bs)) break;
            bs.prevst[bs.bsize]=(u8t)bs.BitBuffer; *bs.prevst=(u8t)bs.bsize++;
            bs.prevst[bs.bsize]=0;
            bptr=bs.prevst+bs.bsize+1;
            rcbuf=bs.rcbuf;
            *bptr++=0x3B;
            result=1;
         } while (0);
      }
      if (Table ) free(Table );
      if (PixBuf) free(PixBuf);
      if (Code  ) free(Code  );
      if (Prefix) free(Prefix);
      if (Append) free(Append);
      
      if (!result) {
         if (rcbuf) free(rcbuf);
         return 0;
      } else {
         *ressize=bptr-rcbuf;
         return rcbuf;
      }
   }
}
