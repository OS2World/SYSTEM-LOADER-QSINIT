/*------------------------------------------------------------*/
/* filename -       tvtext1.cpp                               */
/*                                                            */
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

#define Uses_TScreen
#define Uses_TRadioButtons
#define Uses_TMenuBox
#define Uses_TFrame
#define Uses_TIndicator
#define Uses_THistory
#define Uses_TColorSelector
#define Uses_TMonoSelector
#define Uses_TColorDialog
#define Uses_TInputLine
#define Uses_TStatusLine
#define Uses_TCheckBoxes
#define Uses_TScrollBar
#define Uses_TButton
#define Uses_TDirListBox
#define Uses_TFileEditor
#include <tv.h>

#ifdef __DOS16__
#include <dos.h>
#endif

static unsigned getCodePage() {
#ifdef __DOS16__
   //  get version number, in the form of a normal number
   unsigned ver = (_version >> 8) | (_version << 8);
   if (ver < 0x30C)
      return 437; // United States code page, for all versions before 3.3

   _AX = 0x6601;   // get code page
   geninterrupt(0x21);
   return _BX;
#else   // __DOS16__
   return 437;
#endif
}

void TDisplay::updateIntlChars() {
   if (getCodePage() != 437)
      TFrame::frameChars[30] = 'Í';
}

extern const uchar specialChars[] = {
#ifdef __RUS__
   '<', '>', 26, 27, ' ', ' '
#else
   175, 174, 26, 27, ' ', ' '
#endif
};

const char *TRadioButtons::button = " ( ) ";

const char *TMenuBox::frameChars = " \332\304\277  \300\304\331  \263 \263  \303\304\264 ";

const char TFrame::initFrame[19] =
   "\x06\x0A\x0C\x05\x00\x05\x03\x0A\x09\x16\x1A\x1C\x15\x00\x15\x13\x1A\x19";

char TFrame::frameChars[33] =
   "   À ³ÚÃ ÙÄÁ¿´ÂÅ   È ºÉÇ ¼ÍÏ»¶Ñ "; // for UnitedStates code page

const char *TFrame::closeIcon = "[~\xFE~]";
const char *TFrame::zoomIcon = "[~\x18~]";
const char *TFrame::unZoomIcon = "[~\x12~]";
const char *TFrame::dragIcon = "~ÄÙ~";

const char TIndicator::dragFrame = '\xCD';
const char TIndicator::normalFrame = '\xC4';

const char *THistory::icon = "\xDE~\x19~\xDD";

const char TColorSelector::icon = '\xDB';

const char *TMonoSelector::button = " ( ) ";
const char *TMonoSelector::normal = "Normal";
const char *TMonoSelector::highlight = "Highlight";
const char *TMonoSelector::underline = "Underline";
const char *TMonoSelector::inverse = "Inverse";

const char *TColorDialog::colors = "Colors";
const char *TColorDialog::groupText = "~G~roup";
const char *TColorDialog::itemText = "~I~tem";
const char *TColorDialog::forText = "~F~oreground";
const char *TColorDialog::bakText = "~B~ackground";
const char *TColorDialog::textText = "Text ";
const char *TColorDialog::colorText = "Color";
const char *TColorDialog::okText = "O~K~";
const char *TColorDialog::cancelText = "Cancel";

const char TInputLine::rightArrow = '\x10';
const char TInputLine::leftArrow = '\x11';

const char *TStatusLine::hintSeparator = "\xB3 ";

const char *TCheckBoxes::button = " [ ] ";

TScrollChars TScrollBar::vChars = {30, 31, 177, 254, 178};
TScrollChars TScrollBar::hChars = {17, 16, 177, 254, 178};

const char *TButton::shadows = "\xDC\xDB\xDF";
const char *TButton::markers = "[]";

const char *TDirListBox::pathDir   = "ÀÄÂ";
const char *TDirListBox::firstDir  =   "ÀÂÄ";
const char *TDirListBox::middleDir =   " ÃÄ";
const char *TDirListBox::lastDir   =   " ÀÄ";
const char *TDirListBox::drives = "Drives";
const char *TDirListBox::graphics = "ÀÃÄ";

const char *TFileEditor::backupExt = ".BAK";

