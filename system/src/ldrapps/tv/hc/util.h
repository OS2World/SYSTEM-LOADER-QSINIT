/* ------------------------------------------------------------------------*/
/*                                                                         */
/*   UTIL.H                                                                */
/*                                                                         */
/*   defines various utility functions used throughout Turbo Vision        */
/*                                                                         */
/* ------------------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#if !defined( __UTIL_H )
#define __UTIL_H

#ifndef __IBMCPP__
inline int min(int a, int b) {
   return (a>b) ? b : a;
}

inline int max(int a, int b) {
   return (a<b) ? b : a;
}
#else
#include <minmax.h>
#endif

void fexpand(char *);

unsigned long getTicks();
// returns a value that can be used as a substitute for the DOS Ticker at [0040:006C]
unsigned char getShiftState();
// returns a value that can be used as a substitute for the shift state at [0040:0017]

#ifndef __BORLANDC__
int fnsplit(const char *__path,
            char *__drive,
            char *__dir,
            char *__name,
            char *__ext);
void fnmerge(char *__path,
             const char *__drive,
             const char *__dir,
             const char *__name,
             const char *__ext);
#define WILDCARDS 0x01
#define EXTENSION 0x02
#define FILENAME  0x04
#define DIRECTORY 0x08
#define DRIVE     0x10

int getdisk(void);
int getcurdir(int __drive, char *buffer);

#endif

char hotKey(const char *s);
ushort ctrlToArrow(ushort);
char getAltChar(ushort keyCode);
ushort getAltCode(char ch);
char getCtrlChar(ushort);
ushort getCtrlCode(uchar);

ushort historyCount(uchar id);
const char *historyStr(uchar id, int index);
void historyAdd(uchar id, const char *);

int cstrlen(const char *);

class TView;
void *message(TView *receiver, ushort what, ushort command, void *infoPtr);
Boolean lowMemory();

char *newStr(const char *);

Boolean driveValid(char drive);

Boolean isDir(const char *str);

Boolean pathValid(const char *path);

Boolean validFileName(const char *fileName);

void getCurDir(char *dir);

Boolean isWild(const char *f);


#endif  // __UTIL_H
