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
#include "qsshell.h"
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

#define MSG_WIDE          0x1000      ///< wide message box
#define MSG_POPUP         0x2000
#define MSG_NOSHADOW      0x4000      ///< no shadow on the bottom and right borders

#define MRES_CANCEL       0x0000
#define MRES_OK           0x0001
#define MRES_YES          0x0002
#define MRES_NO           0x0003

#define KEY_ENTERMBOX     0xFFFF
#define KEY_LEAVEMBOX     0xFFFE

/** callback for vio_msgbox().
    Note, that in MT mode, with MSG_POPUP flag - this function is called from
    another thread.

    @param key    Pressed key (from key_read()). In addition KEY_ENTERMBOX
                  will be sended on start and KEY_LEAVEMBOX on exit
    @return -1 to continue processing, -2 - to ignore this key, >=0 - to
            stop dialog and return this result code. */
typedef int _std (*vio_mboxcb)(u16t key);

/** shows message box.
    With MSG_POPUP flag function acts as normal message box in non-MT mode and
    as "vio popup" in MT mode. This mean - it showed in the separate session
    (from a special thread) and can be used by detached processes.

    @param header   Header string
    @param text     Message text. Use ^ symbol for manual EOL.
    @param flags    Message box flags (MSG_* above).
    @param cbfunc   Callback function for aditional processing, can be 0.
    @return result code (MRES_*) */
u32t  _std vio_msgbox(const char *header, const char *text, u32t flags,
                      vio_mboxcb cbfunc);

/** query/set ANSI state.
    ANSI state is unique for every process. Child inherits current
    value from its parent process.

    @param newstate   0/1 - set state, -1 - return current only
    @return previous ANSI state */
u32t  _std vio_setansi(int newstate);

#define ANSI_RESET   "\x1B[0m"        ///< reset color to gray

/// @name ANSI text color seqiences
//@{
#define ANSI_BLACK     "\x1B[0;30m"   ///< black
#define ANSI_RED       "\x1B[0;31m"   ///< red
#define ANSI_GREEN     "\x1B[0;32m"   ///< green
#define ANSI_BROWN     "\x1B[0;33m"   ///< brown
#define ANSI_BLUE      "\x1B[0;34m"   ///< blue
#define ANSI_MAG       "\x1B[0;35m"   ///< magenta
#define ANSI_CYAN      "\x1B[0;36m"   ///< cyan
#define ANSI_GRAY      "\x1B[0;37m"   ///< gray

#define ANSI_DGRAY     "\x1B[1;30m"   ///< dark gray
#define ANSI_LRED      "\x1B[1;31m"   ///< red
#define ANSI_LGREEN    "\x1B[1;32m"   ///< green
#define ANSI_YELLOW    "\x1B[1;33m"   ///< yellow
#define ANSI_LBLUE     "\x1B[1;34m"   ///< light blue
#define ANSI_LMAG      "\x1B[1;35m"   ///< light magenta
#define ANSI_LCYAN     "\x1B[1;36m"   ///< light cyan
#define ANSI_WHITE     "\x1B[1;37m"   ///< white
//@}

/// @name ANSI background color seqiences
//@{
#define ANSI_BG_BLACK  "\x1B[0;40m"   ///< black
#define ANSI_BG_RED    "\x1B[0;41m"   ///< red
#define ANSI_BG_GREEN  "\x1B[0;42m"   ///< green
#define ANSI_BG_BROWN  "\x1B[0;43m"   ///< brown
#define ANSI_BG_BLUE   "\x1B[0;44m"   ///< blue
#define ANSI_BG_MAG    "\x1B[0;45m"   ///< magenta
#define ANSI_BG_CYAN   "\x1B[0;46m"   ///< cyan
#define ANSI_BG_GRAY   "\x1B[0;47m"   ///< gray

#define ANSI_BG_DGRAY  "\x1B[1;40m"   ///< dark gray
#define ANSI_BG_LRED   "\x1B[1;41m"   ///< red
#define ANSI_BG_LGREEN "\x1B[1;42m"   ///< green
#define ANSI_BG_YELLOW "\x1B[1;43m"   ///< yellow
#define ANSI_BG_LBLUE  "\x1B[1;44m"   ///< light blue
#define ANSI_BG_LMAG   "\x1B[1;45m"   ///< light magenta
#define ANSI_BG_LCYAN  "\x1B[1;46m"   ///< light cyan
#define ANSI_BG_WHITE  "\x1B[1;47m"   ///< white
//@}

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

typedef struct _vio_list {
   u32t        size;     ///< struct size
   u32t       items;     ///< # of items in the menu
   str_list   *text;     ///< text lines
   u32t         *id;     ///< id (menu result) for every line (if 0, then result is line number)
   struct
   _vio_list **subm;     ///< submenus (if present, ptr can be 0, members too)
} vio_listref;

/** execute listbox selection (with possible submenus).
    Text can be splitted into columns in the following way:
       "1.|column1|column2|column3"

    If text list has an additional line, then 1st line acts as information
    header in such form:
       "<12|>30%|40"
    where '<' - left alignment, '>' right, number - column width and number% -
    width in percents of current screen width. Double char '>>' puts string
    next to border line, single '>' adds a space ' ' between the string and the
    border.

    Single '-' char in line means horisontal line, '=' - double line.

    @param ref        Data to show
    @param flags      Subset of vio_msgbox() flags accepted here: MSG_POPUP
                      and color scheme MSG_values.
    @return 0 if ESC was pressed or value from the ref->id array (from one of
       submenus too) or (if ref->id==0) - just a one-based line number of the
       selected item */
u32t  _std vio_showlist(const char *header, vio_listref *ref, u32t flags);

#ifdef __cplusplus
}
#endif
#endif // QSINIT_VIO_EXTRA
