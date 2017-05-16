/*------------------------------------------------------------*/
/* filename -       schdrdlg.cpp                              */
/*                                                            */
/* Registeration object for the class TChDirDialog            */
/*------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */

#ifndef NO_TV_STREAMS

#ifdef __WATCOMC__
#pragma off(unreferenced)
#endif

#define Uses_TChDirDialog
#define Uses_TStreamableClass
#include <tv.h>
__link(RDialog)
__link(RButton)
__link(RDirListBox)

TStreamableClass RChDirDialog(TChDirDialog::name,
                              TChDirDialog::build,
                              __DELTA(TChDirDialog)
                             );

#endif  // ifndef NO_TV_STREAMS
