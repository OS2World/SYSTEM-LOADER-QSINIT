/*-------------------------------------------------------*/
/*                                                       */
/*   Turbo Vision 1.0                                    */
/*   Copyright (c) 1991 by Borland International         */
/*                                                       */
/*   Calc.h: Header file for Calc.cpp                    */
/*-------------------------------------------------------*/

#if !defined( __CALC_H )
#define __CALC_H

#include <stdlib.h>
#include <qstypes.h>
#include "diskedit.h"

#define DISPLAYLEN  30      // Length (width) of calculator display

enum TCalcState { csFirst = 1, csValid, csError };

const int cmCalcButton  = 200;

class TCalcDisplay : public TView {
public:
   TCalcDisplay(TRect &r);
   ~TCalcDisplay();
   virtual void handleEvent(TEvent &event);
   virtual TPalette& getPalette() const;
   virtual void draw();
private:
   TCalcState status;
   char      *number;
   char         sign;
   char      operate;           // since 'operator' is a reserved word.
   s64t      operand;
   int       cvtbase;
   s64t       memory;

   void calcKey(unsigned char key);
   void checkFirst();
   void setDisplay(s64t rv);
   void clear();
   void error();
   s64t getDisplay();
};

class TCalculatorWindow : public TAppWindow {
public:
   TCalculatorWindow();
   virtual TPalette &getPalette() const;
};

#endif      // __CALC_H
