/*------------------------------------------------------------*/
/* filename -       msgbox.cpp                                */
/*                                                            */
/* function(s)                                                */
/*                  messageBox related functions              */
/*------------------------------------------------------------*/

/*------------------------------------------------------------*/
/*                                                            */
/*    Turbo Vision -  Version 1.0                             */
/*                                                            */
/*                                                            */
/*    Copyright (c) 1991 by Borland International             */
/*    All Rights Reserved.                                    */
/*                                                            */
/*------------------------------------------------------------*/


#define Uses_MsgBox
#define Uses_TObject
#define Uses_TDialog
#define Uses_TStaticText
#define Uses_TRect
#define Uses_TButton
#define Uses_TProgram
#define Uses_TInputLine
#define Uses_TDeskTop
#define Uses_TLabel
#include <tv.h>

#include <stdio.h>

#ifdef __IDA__
#define __NOT_ONLY_PRO_FUNCS__
#include <pro.h>
#else
#include <string.h>
#if !defined(__WATCOMC__)&&!defined(__MSVC__)
// if the target system doesn't have snprintf(), use sprintf()
inline void vsnprintf(char *buf, size_t bufsize, const char *format, va_list va) {
   vsprintf(buf, format, va);
}
#else
#include <stdarg.h>
#endif
#endif

static const char *buttonName[] = {
   MsgBoxText::yesText,
   MsgBoxText::noText,
   MsgBoxText::okText,
   MsgBoxText::cancelText
};

static ushort commands[] = {
   cmYes,
   cmNo,
   cmOK,
   cmCancel
};

static const char *Titles[] = {
   MsgBoxText::warningText,
   MsgBoxText::errorText,
   MsgBoxText::informationText,
   MsgBoxText::confirmText
};

ushort messageBoxRect(const TRect &r, const char *msg, ushort aOptions) {
   TDialog *dialog;
   short i, x, buttonCount;
   TView *buttonList[5];
   ushort ccode;

   dialog = new TDialog(r, Titles[aOptions & 0x3]);

   dialog->insert(
      new TStaticText(TRect(3, 2, dialog->size.x - 2, dialog->size.y - 3),
                      msg));

   for (i = 0, x = -2, buttonCount = 0; i < 4; i++) {
      if ((aOptions & (0x0100 << i)) != 0) {
         buttonList[buttonCount] =
            new TButton(TRect(0, 0, 10, 2), buttonName[i], commands[i], bfNormal);
         x += buttonList[buttonCount++]->size.x + 2;
      }
   }

   x = (dialog->size.x - x) / 2;

   for (i = 0; i < buttonCount; i++) {
      dialog->insert(buttonList[i]);
      buttonList[i]->moveTo(x, dialog->size.y - 3);
      x += buttonList[i]->size.x + 2;
   }

   dialog->selectNext(False);

   ccode = TProgram::application->execView(dialog);

   TObject::destroy(dialog);

   return ccode;
}

ushort messageBoxRect(const TRect &r,
                      ushort aOptions,
                      const char *fmt,
                      ...) {
   va_list argptr;
   va_start(argptr, fmt);

   char msg[4096];
   vsnprintf(msg, sizeof(msg), fmt, argptr);
   va_end(argptr);

   return messageBoxRect(r, msg, aOptions);
}

static TRect makeRect() {
   TRect r(0, 0, 40, 9);
   r.move((TProgram::deskTop->size.x - r.b.x) / 2,
          (TProgram::deskTop->size.y - r.b.y) / 2);
   return r;
}

ushort messageBox(const char *msg, ushort aOptions) {
   return messageBoxRect(makeRect(), msg, aOptions);
}

ushort messageBox(ushort aOptions, const char *fmt, ...) {
   va_list argptr;
   va_start(argptr, fmt);

   char msg[4096];
   vsnprintf(msg, sizeof(msg), fmt, argptr);
   va_end(argptr);

   return messageBoxRect(makeRect(), msg, aOptions);
}

ushort inputBox(const char *Title, const char *aLabel, char *s, int limit) {
   ushort len = max(strlen(aLabel) + 9 + limit, strlen(Title) + 11);
   len = min(len, 60);
   len = max(len , 24);
   TRect r(0, 0, len, 8);
   r.move((TProgram::deskTop->size.x - r.b.x) / 2,
          (TProgram::deskTop->size.y - r.b.y) / 2);
   return inputBoxRect(r, Title, aLabel, s, limit);
}

ushort inputBoxRect(const TRect &bounds,
                    const char *Title,
                    const char *aLabel,
                    char *s,
                    int limit) {
   TDialog *dialog;
   TView *control;
   TRect r;
   ushort c;

   dialog = new TDialog(bounds, Title);

   int x = 4 + strlen(aLabel);
   r = TRect(x, 2, min(x + limit + 2, dialog->size.x - 3), 3);
   control = new TInputLine(r, limit);
   dialog->insert(control);

   r = TRect(2, 2, 3 + strlen(aLabel), 3);
   dialog->insert(new TLabel(r, aLabel, control));

   r = TRect(dialog->size.x / 2 - 11, dialog->size.y - 4,
             dialog->size.x / 2 - 1 , dialog->size.y - 2);
   dialog->insert(new TButton(r, MsgBoxText::okText, cmOK, bfDefault));

   r.a.x += 12;
   r.b.x += 12;
   dialog->insert(new TButton(r, MsgBoxText::cancelText, cmCancel, bfNormal));

   r.a.x += 12;
   r.b.x += 12;
   dialog->selectNext(False);
   dialog->setData(s);
   c = TProgram::application->execView(dialog);
   if (c != cmCancel)
      dialog->getData(s);
   TObject::destroy(dialog);
   return c;
}

