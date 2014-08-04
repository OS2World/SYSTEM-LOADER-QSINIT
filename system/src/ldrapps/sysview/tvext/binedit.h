//
// QSINIT "sysview" module
// binary data editor
//
#if !defined( __binedit_h )
#define __binedit_h

#define Uses_TEvent
#define Uses_TRect
#define Uses_TKeys
#define Uses_TDialog
#define Uses_TWindow
#define Uses_TButton
#define Uses_TLabel
#define Uses_TScrollBar
#define Uses_TScroller

#include <tv.h>

typedef struct {
  void         *data;
  unsigned long size;
} THexEditorData;

class THexEditor :public TScroller {
protected:
    int           MaxLines;
    int           sizeable;
    int          overwrite;
    void             *data;
    unsigned char *changed;
    unsigned long     Size;
public:
    THexEditor(const TRect& bounds, TScrollBar *aVScrollBar, int varysize = True);

    virtual void handleEvent(TEvent& event);
    virtual void draw();

    virtual void getData(void *rec);
    virtual void setData(void *rec);
    virtual void toggleInsMode();

    virtual void setState(ushort aState, Boolean enable);

    virtual void shutDown();
};

#endif // __binedit_h
