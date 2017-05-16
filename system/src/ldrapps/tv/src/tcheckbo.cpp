/*------------------------------------------------------------*/
/* filename -       tcheckbo.cpp                              */
/*                                                            */
/* function(s)                                                */
/*          TCheckBox member functions                        */
/*------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#define Uses_TCheckBoxes
#include <tv.h>

void TCheckBoxes::draw() {
   drawBox(button, 'X');
}

Boolean TCheckBoxes::mark(int item) {
   return Boolean((value & (1 <<  item)) != 0);
}

void TCheckBoxes::press(int item) {
   value = value^(1 << item);
}

#ifndef NO_TV_STREAMS
TStreamable *TCheckBoxes::build() {
   return new TCheckBoxes(streamableInit);
}

TCheckBoxes::TCheckBoxes(StreamableInit) : TCluster(streamableInit) {
}
#endif  // ifndef NO_TV_STREAMS
