/* ------------------------------------------------------------------------*/
/*                                                                         */
/*   MSGBOX.H                                                              */
/*                                                                         */
/*   Copyright (c) Borland International 1991                              */
/*   All Rights Reserved.                                                  */
/*                                                                         */
/*   defines the functions messageBox(), messageBoxRect(),                 */
/*   inputBox(), and inputBoxRect()                                        */
/*                                                                         */
/* ------------------------------------------------------------------------*/


#if defined( Uses_MsgBox ) && !defined( __MsgBox )
#define __MsgBox

#if !defined( __STDARG_H )
#include <stdarg.h>
#endif  // __STDARG_H

#include <tvvo.h>

class TRect;

ushort messageBox(const char *msg, ushort aOptions);
ushort messageBox(ushort aOptions, const char *msg, ...);

ushort messageBoxRect(const TRect &r, const char *msg, ushort aOptions);
ushort messageBoxRect(const TRect &r, ushort aOptions, const char *msg, ...);

ushort inputBox(const char *Title, const char *aLabel, char *s, int limit);

ushort inputBoxRect(const TRect &bounds, const char *title,
                    const char *aLabel, char *s, int limit);

//  Message box classes

// Display a Warning box
#define mfWarning       (0x0000)
// Dispaly a Error box
#define mfError         (0x0001)
// Display an Information Box
#define mfInformation   (0x0002)
// Display a Confirmation Box
#define mfConfirmation  (0x0003)

// Message box button flags
// Put a Yes button into the dialog
#define mfYesButton     (0x0100)
// Put a No button into the dialog
#define mfNoButton      (0x0200)
// Put an OK button into the dialog
#define mfOKButton      (0x0400)
// Put a Cancel button into the dialog
#define mfCancelButton  (0x0800)

const int
mfYesNoCancel  = mfYesButton | mfNoButton | mfCancelButton,
// Standard Yes, No, Cancel dialog
mfOKCancel     = mfOKButton | mfCancelButton;
// Standard OK, Cancel dialog

class MsgBoxText {

public:

   static const char *yesText;
   static const char *noText;
   static const char *okText;
   static const char *cancelText;
   static const char *warningText;
   static const char *errorText;
   static const char *informationText;
   static const char *confirmText;
};

#include <tvvo2.h>

#endif  // Uses_MsgBox


