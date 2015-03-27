//
// QSINIT API
// additional vio functions
//

#ifndef QSINIT_VIO_EXTRA
#define QSINIT_VIO_EXTRA

#ifdef __cplusplus
extern "C" {
#endif
#include "qstypes.h"
#include "vio.h"

#define MSG_GRAY          0x0000
#define MSG_CYAN          0x0001
#define MSG_GREEN         0x0002
#define MSG_LIGHTGREEN    0x0003
#define MSG_BLUE          0x0004
#define MSG_LIGHTBLUE     0x0005
#define MSG_RED           0x0006
#define MSG_LIGHTRED      0x0007
#define MSG_WHITE         0x0008

#define MSG_CENTER        0x0000
#define MSG_LEFT          0x0100
#define MSG_RIGHT         0x0200
#define MSG_JUSTIFY       0x0300

#define MSG_DEF1          0x0000      ///< default is 1st button
#define MSG_DEF2          0x0400      ///< default is 2nd button
#define MSG_DEF3          0x0800      ///< default is 3rd button

#define MSG_OK            0x0010
#define MSG_OKCANCEL      0x0020
#define MSG_YESNO         0x0030
#define MSG_YESNOCANCEL   0x0040

#define MSG_WIDE          0x1000

#define MRES_CANCEL       0x0000
#define MRES_OK           0x0001
#define MRES_YES          0x0002
#define MRES_NO           0x0003

#define KEY_ENTERMBOX     0xFFFF
#define KEY_LEAVEMBOX     0xFFFE

/** callback for vio_msgbox().
    @param key    Pressed key (from key_read()). In addition KEY_ENTERMBOX
                  will be sended on start and KEY_LEAVEMBOX on exit
    @return -1 to continue processing, -2 - to ignore this key, >=0 - to
            stop dialog and return this result code. */
typedef int _std (*vio_mboxcb)(u16t key);

/** shows message box.
    @param header   Header string
    @param text     Message text. Use ^ symbol for manual EOL.
    @param flags    Message box flags (MSG_* above).
    @param cbfunc   Callback function for aditional processing, can be 0.
    @return result code (MRES_*) */
u32t  _std vio_msgbox(const char *header, const char *text, u32t flags,
                      vio_mboxcb *cbfunc);

/** query/set ANSI state.
    @param newstate   0/1 - set state, -1 - return current only
    @return previous ANSI state */
u32t  _std vio_setansi(int newstate);

/// key_getstr() internal data for callback function
typedef struct {
   u32t        line;     ///< screen line
   u32t         col;     ///< screen column of 1st char
   u32t       width;     ///< visible width of editing line
   long         pos;     ///< current position on screen (without col)
   long      scroll;     ///< number of chars scrolled left
   long        clen;     ///< current string length
   long       bsize;     ///< string buffer length (malloc)
   char      *rcstr;     ///< current string
   u16t    defshape;     ///< original cursor shape
   u32t    userdata;     ///< userdata for callback, initialized with 0
} key_strcbinfo;

/** callback for key_getstr().
    @param key        Pressed key (from key_read())
    @param inputdata  Internal key_getstr() data.
    @return -1 to exit key_getstr() with 0 result, <=-2 - to exit key_getstr()
            with current string state, bit 0 set to repaint string, bit 1 set
            to skip key processing by key_getstr() */
typedef int _std (*key_strcb)(u16t key, key_strcbinfo *inputdata);

/** read string from console.
    @param cbfunc     Callback function for aditional processing, can be 0.
    @return string, need to be free() */
char* _std key_getstr(key_strcb cbfunc);

/** read string from console.
    @param cbfunc     Callback function for aditional processing, can be 0.
    @param col        Column of first pos, can be -1 for current pos.
    @param line       Line of first pos, can be -1.
    @param len        Length (until the end of screen max), can be -1.
    @param init       Initial string value, can be 0
    @return string, need to be free() */
char* _std key_getstrex(key_strcb cbfunc, int col, int line, int len, const char *init);

#ifdef __cplusplus
}
#endif
#endif // QSINIT_VIO_EXTRA
