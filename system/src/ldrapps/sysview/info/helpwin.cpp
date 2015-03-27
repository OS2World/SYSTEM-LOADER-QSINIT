#define Uses_TApplication
#define Uses_TDeskTop
#include <tv.h>
#include "helpwin.h"

TAppHlpWindow::TAppHlpWindow(THelpFile *hFile, ushort context) :
   TAppWindow (SysApp.deskTop->getExtent(), "Help", TWin_Help),
   TWindowInit(initFrame)
{
   TRect r(SysApp.deskTop->getExtent());
   options = (options | ofCentered);
   r.grow(-2,-1);

   hwc = new THelpViewer(r, standardScrollBar(sbHorizontal | sbHandleKeyboard),
      standardScrollBar(sbVertical | sbHandleKeyboard), hFile, context);
   insert(hwc);
}

void TAppHlpWindow::goBack() {
}

void TAppHlpWindow::goTopic(ushort topic) {
   hwc->switchToTopic(topic);
}

TPalette &TAppHlpWindow::getPalette() const {
   static TPalette palette(cHelpWindow, sizeof(cHelpWindow)-1);
   return palette;
}
