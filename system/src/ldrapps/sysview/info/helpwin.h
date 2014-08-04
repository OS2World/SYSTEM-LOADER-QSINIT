//
// QSINIT "sysview" module
// help support header
//
#ifndef QS_SYSVIEW_HELPWIN
#define QS_SYSVIEW_HELPWIN

#include "diskedit.h"
#include "tvhelp.h"

// THelpWindow
class TAppHlpWindow: public TAppWindow {
   THelpViewer  *hwc;
public:
   TAppHlpWindow(THelpFile *hFile, ushort topic);
   void goBack();
   void goTopic(ushort topic);

   virtual TPalette &getPalette() const;

   static THelpFile *HFile;
};

#endif // QS_SYSVIEW_HELPWIN
