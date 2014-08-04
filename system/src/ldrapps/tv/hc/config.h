/* ------------------------------------------------------------------------*/
/*                                                                         */
/*   CONFIG.H                                                              */
/*                                                                         */
/*   Copyright (c) Borland International 1991                              */
/*   All Rights Reserved.                                                  */
/*                                                                         */
/*   miscellaneous system-wide configuration parameters                    */
/*   FOR INTERNAL USE ONLY                                                 */
/*                                                                         */
/* ------------------------------------------------------------------------*/

#if !defined( __CONFIG_H )
#define __CONFIG_H

#define eventQSize         (16)
#ifdef __DOS16__
#define maxCollectionSize  ((int)((65536uL - 16)/sizeof( void * )))
#define maxTerminalSize    (32000)
#else
#define maxCollectionSize  (0x7FFFFFFF)
#define maxTerminalSize    (0x7FFFFFFF)
#endif
#define maxViewWidth       (512)
#define maxFindStrLen      (80)
#define maxReplaceStrLen   (80)

#endif  // __CONFIG_H
