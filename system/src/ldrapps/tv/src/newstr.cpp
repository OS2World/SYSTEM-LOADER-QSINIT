/*------------------------------------------------------------*/
/* filename -       newstr.cpp                                */
/*                                                            */
/* function(s)                                                */
/*                  newStr member function                    */
/*------------------------------------------------------------*/
/*
 *      Turbo Vision - Version 2.0
 *
 *      Copyright (c) 1994 by Borland International
 *      All Rights Reserved.
 *
 */


#include <tv.h>
#include <string.h>

char *newStr(const char *s) {
   if (s == 0)
      return 0;
   char *temp = new char[ strlen(s)+1 ];
   strcpy(temp, s);
   return temp;
}
