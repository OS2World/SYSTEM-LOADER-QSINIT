/*------------------------------------------------------------------------*/
/* filename - drivers.cpp                                                 */
/*                                                                        */
/* function(s)                                                            */
/*        moveBuf  --   moves a buffer of char/attribute pairs            */
/*        moveChar --   sets a buffer with a char/attribute pair          */
/*        moveCStr --   moves a char array into a buffer & adds an        */
/*                      attribute to each char                            */
/*------------------------------------------------------------------------*/

/*------------------------------------------------------------------------*/
/*                                                                        */
/*    Turbo Vision -  Version 1.0                                         */
/*                                                                        */
/*                                                                        */
/*    Copyright (c) 1991 by Borland International                         */
/*    All Rights Reserved.                                                */
/*                                                                        */
/*------------------------------------------------------------------------*/

#define Uses_TDrawBuffer
#include <tv.h>

#ifdef __DOS16__

#include <dos.h>

/*------------------------------------------------------------------------*/
/*                                                                        */
/*  TDrawBuffer::moveBuf                                                  */
/*                                                                        */
/*  arguments:                                                            */
/*                                                                        */
/*      indent - character position within the buffer where the data      */
/*               is to go                                                 */
/*                                                                        */
/*      source - far pointer to an array of character/attribute pairs     */
/*                                                                        */
/*      attr   - attribute to be used for all characters (0 to retain     */
/*               the attribute from 'source')                             */
/*                                                                        */
/*      count   - number of character/attribute pairs to move             */
/*                                                                        */
/*------------------------------------------------------------------------*/

void TDrawBuffer::moveBuf(ushort indent, const void *source,
                          ushort attr, ushort count)

{
   asm {
      MOV     CX,count
      JCXZ    __5
      PUSH    DS
   }

   _ES = FP_SEG(&data[indent]);
   _DI = FP_OFF(&data[indent]);

   _DS = FP_SEG(source);
   _SI = FP_OFF(source);

   asm {
      MOV     AH,[BYTE PTR attr]
      CLD
      OR      AH,AH
      JE      __3
   }
__1:
   asm {
      LODSB
      STOSW
      LOOP    __1
      JMP     __4
   }
__2:
   asm INC     DI
   __3:
   asm {
      STOSB
      LOOP    __2
   }
__4:
   asm POP     DS
   __5:
   ;
}

/*------------------------------------------------------------------------*/
/*                                                                        */
/*  TDrawBuffer::moveChar                                                 */
/*                                                                        */
/*  arguments:                                                            */
/*                                                                        */
/*      indent  - character position within the buffer where the data     */
/*                is to go                                                */
/*                                                                        */
/*      c       - character to be put into the buffer                     */
/*                                                                        */
/*      attr    - attribute to be put into the buffer                     */
/*                                                                        */
/*      count   - number of character/attribute pairs to put into the     */
/*                buffer                                                  */
/*                                                                        */
/*------------------------------------------------------------------------*/

void TDrawBuffer::moveChar(ushort indent, char c, ushort attr, ushort count) {
   asm {
      MOV     CX,count
      JCXZ    __4
   }

   _ES = FP_SEG(&data[indent]);
   _DI = FP_OFF(&data[indent]);

   asm {
      MOV     AL,c
      MOV     AH,[BYTE PTR attr]
      CLD
      OR      AL,AL
      JE      __1
      OR      AH,AH
      JE      __3
      REP     STOSW
      JMP     __4
   }
__1:
   asm MOV     AL,AH
   __2:
   asm INC     DI
   __3:
   asm STOSB
   asm LOOP    __2
   __4:
   ;
}

/*------------------------------------------------------------------------*/
/*                                                                        */
/*  TDrawBuffer::moveCStr                                                 */
/*                                                                        */
/*  arguments:                                                            */
/*                                                                        */
/*      indent  - character position within the buffer where the data     */
/*                is to go                                                */
/*                                                                        */
/*      str     - pointer to a 0-terminated string of characters to       */
/*                be moved into the buffer                                */
/*                                                                        */
/*      attrs   - pair of text attributes to be put into the buffer       */
/*                with each character in the string.  Initially the       */
/*                low byte is used, and a '~' in the string toggles       */
/*                between the low byte and the high byte.                 */
/*                                                                        */
/*------------------------------------------------------------------------*/

void TDrawBuffer::moveCStr(ushort indent, const char *str, ushort attrs) {
   asm {
      PUSH    DS
      LDS     SI,str
      CLD
   }

   _ES = FP_SEG(&data[indent]);
   _DI = FP_OFF(&data[indent]);

   asm {
      MOV     BX,attrs
      MOV     AH,BL
   }
__1:
   asm {
      LODSB
      CMP     AL,'~'
      JE      __2
      CMP     AL,0
      JE      __3
      STOSW
      JMP     __1
   }
__2:
   asm XCHG    AH,bH
   asm JMP     __1
   __3:
   asm POP     DS
}

/*------------------------------------------------------------------------*/
/*                                                                        */
/*  TDrawBuffer::moveStr                                                  */
/*                                                                        */
/*  arguments:                                                            */
/*                                                                        */
/*      indent  - character position within the buffer where the data     */
/*                is to go                                                */
/*                                                                        */
/*      str     - pointer to a 0-terminated string of characters to       */
/*                be moved into the buffer                                */
/*                                                                        */
/*      attr    - text attribute to be put into the buffer with each      */
/*                character in the string.                                */
/*                                                                        */
/*------------------------------------------------------------------------*/

void TDrawBuffer::moveStr(ushort indent, const char *str, ushort attr) {
   asm {
      PUSH    DS
      LDS     SI,str
      CLD
   }

   _ES = FP_SEG(&data[indent]);
   _DI = FP_OFF(&data[indent]);

   asm {
      MOV     BX,attr
      MOV     AH,BL
   }
__1:
   asm {
      LODSB
      CMP     AL,0
      JE      __2
      STOSW
      JMP     __1
   }
__2:
   asm POP     DS
}

//-----------------------------------------------------------------------
//      End of MS DOS version
//

#else // __DOS16__

//-----------------------------------------------------------------------
//      System independent version
//

//-----------------------------------------------------------------------
void TDrawBuffer::moveBuf(ushort indent, const void *source,
                          ushort attr, ushort count) {
   if (attr) {
      for (unsigned int i=0; i<count; i++) {
         data[indent+i]= (((unsigned char *) source)[i] & 0xFF)+(attr << 8);
      }
   } else {
      for (unsigned int i=0; i<count; i++) {
         data[indent+i]= ((unsigned char *) source)[i]+(data[indent+i] & 0xFF00);
      }
   }
}

//-----------------------------------------------------------------------
void TDrawBuffer::moveChar(ushort indent, char c, ushort attr, ushort count) {
   if (attr && c) {
      for (unsigned int i=0; i<count; i++) {
         data[indent+i]= ((unsigned char) c)+(attr << 8);
      }
   } else if (c) {
      for (unsigned int i=0; i<count; i++) {
         data[indent+i]= ((unsigned char) c)+(data[indent+i] & 0xFF00);
      }
   } else { // change attribute byte only
      for (unsigned int i=0; i<count; i++) {
         data[indent+i]= (attr << 8)+(data[indent+i] & 0x00FF);
      }
   }
}

//-----------------------------------------------------------------------
void TDrawBuffer::moveCStr(ushort indent, const char *str, ushort attrs) {
   int offset=0;
   for (int i=0; str[i]; i++) {
      if (str[i]=='~') attrs=(attrs>>8)+((attrs & 0xFF)<<8);
      else data[indent+offset++]= ((unsigned char)(str[i])) + ((attrs&0xFF)<<8);
   }
}

//-----------------------------------------------------------------------
void TDrawBuffer::moveStr(ushort indent, const char *str, ushort attr) {
   attr <<= 8;
   for (int i=0; *str != '\0'; i++)
      data[indent+i]= (unsigned char)(*str++) + attr;
}

#endif  // __DOS16__
