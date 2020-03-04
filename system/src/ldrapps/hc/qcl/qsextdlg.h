//
// QSINIT API
// additional system functions, provided by EXTCMD module
//
#ifndef QSINIT_EXTCMD_SC
#define QSINIT_EXTCMD_SC

#include "qstypes.h"
#include "qsclass.h"
#include "qsshell.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct qs_extcmd_s {
   /** shows video mode selection dialog.
       Use SMDT_TEXTADV to make sure, that CONSOLE module is loaded. SMDT_TEXT
       will show existing modes (VGA/UEFI text modes if no CONSOLE loaded).

       SMDT_TEXTNATIVE, SMDT_GRFNATIVE and SMDT_NATIVE use CONSOLE in any way
       and return list of native modes only (without emulated graphics console
       modes).

       "mode con gmlist" command is equal to SMDT_GRFNATIVE selection.

       @param header        Dialog header
       @param flags         Dialog flags (see vio_showlist() flags).
       @param type          Dialog type (one of SMDT_* below).

       @return mode_id or 0 if esc was presses */
   u32t       _exicc (*selectmodedlg)(const char *header, u32t flags, u32t type);

   /** shows "boot manager" selection dialog.
       @param header        Dialog header
       @param flags         Dialog flags (see vio_showlist() flags). Note,
                            that default position will be overriden to the top
                            left corner. Use MSG_POS_CUSTOM value here to
                            force list into the center position.
       @param type          Dialog type (see BTDT_* below). Note, that file
                            system filter bits (BTDT_F*) are applied to the
                            selected view and can be inverted to exclude from
                            the list. If no filter bits present - no filtering
                            at all. BTDT_FLTLVM, BTDT_FLTQS, BTDT_QSM and
                            BTDT_NOQSM - are the separate filtering methods.
       @param letters       Drive letter filter (bit 0 for A:, 1 for B:, etc),
                            only with one of BTDT_FLTLVM or BTDT_FLTQS bits.
       @param [out] disk    Disk number (on success)
       @param [out] index   Partition index (on success, can be 0 for BTDT_HDD),
                            value of -1 means big floppy or mbr (for BTDT_HDD).
       @return 0 after success selection, E_SYS_UBREAK if Esc was pressed or
               one of disk error codes */
   qserr      _exicc (*bootmgrdlg   )(const char *header, u32t flags, u32t type,
                                      u32t letters, u32t *disk, long *index);

   /** shows "task list".
       @param header        Dialog header, can be 0
       @param flags         Dialog flags (see vio_showlist() flags). Note,
                            that default position will be overriden to the top
                            left corner. Use MSG_POS_CUSTOM value here to
                            force list into the center position.
       @param opts          session list options (see SLDO_* below)
       @param ukey          fake key code to force list update (list will be
                            updated if received it), use 0 to ignore.
       @param startid       session id to highlight at start (use 0 for the first).
       @return session id was selected or 0. If 'Tab' key pressed, value of
               TLDR_TAB or-ed with id (only with SLDO_PLIST in options) */
   u32t       _exicc (*seslistdlg   )(const char *header, u32t flags, u32t opts,
                                      u16t ukey, u32t startid);

   /** shows "task list".
       @param header        Dialog header, can be 0
       @param flags         Dialog flags (see vio_showlist() flags). Note,
                            that default position will be overriden to the top
                            left corner. Use MSG_POS_CUSTOM value here to
                            force list into the center position.
       @param opts          task list options (see TLDO_* below). TLDO_PLIST
                            allow Tab key with TLDR_TAB or-ed to id, 
                            TLDO_PDEL allow Del with TLDR_DEL or-ed.
       @param ukey          fake key code to force list update (list will be
                            updated if received it), use 0 to ignore.
       @param startpid      pid to highlight at start (use 0 for the first).
       @return process id was selected or 0 with optional TLDR_* bits */
   u32t       _exicc (*tasklistdlg  )(const char *header, u32t flags, u32t opts,
                                      u16t ukey, u32t startpid);
} _qs_extcmd, *qs_extcmd;

/// @name qs_extcmd->selectmodedlg type values
//@{
#define SMDT_TEXT           0  ///< selection from existing text modes 
#define SMDT_TEXTADV        1  ///< same as SMDT_TEXT, but load CONSOLE before
#define SMDT_TEXTNATIVE     3  ///< list only native text modes (CONSOLE used)
#define SMDT_GRFNATIVE      5  ///< list native graphic modes (CONSOLE) + optinal SMDT_GRF* below
#define SMDT_NATIVE         7  ///< list all native modes (CONSOLE used)

#define SMDT_GRFALL         0  ///< include all graphic mode (SMDT_GRFNATIVE)
#define SMDT_GRF8BIT  0x01000  ///< include  8-bit graphic modes (SMDT_GRFNATIVE)
#define SMDT_GRF15BIT 0x02000  ///< include 15-bit graphic modes (SMDT_GRFNATIVE)
#define SMDT_GRF16BIT 0x04000  ///< include 16-bit graphic modes (SMDT_GRFNATIVE)
#define SMDT_GRF24BIT 0x08000  ///< include 24-bit graphic modes (SMDT_GRFNATIVE)
#define SMDT_GRF32BIT 0x10000  ///< include 32-bit graphic modes (SMDT_GRFNATIVE)
//@}

/// @name qs_extcmd->bootmgrdlg type values
//@{
#define BTDT_LVM            0  ///< show all partitions, marked as bootable in LVM
#define BTDT_HDD            1  ///< show HDD list only
#define BTDT_ALL            2  ///< show all partitions in plain list
#define BTDT_DISKTREE       3  ///< show all partitions, but with submenu for every HDD
#define BTDT_FLTLVM     0x400  ///< add QS drive letter column at 1st pos
#define BTDT_FLTQS      0x800  ///< add QS drive letter column at 1st pos
#define BTDT_NOEMU     0x1000  ///< skip emulated HDDs in enumeration
#define BTDT_QSM       0x2000  ///< include only mounted to QSINIT
#define BTDT_NOQSM     0x4000  ///< exclude all mounted to QSINIT
#define BTDT_QSDRIVE   0x8000  ///< add QS drive letter column at 1st pos
#define BTDT_FILTER 0xFFF0000  ///< mask for the filesystem type filter
#define BTDT_FFAT   0x0010000  ///< include FAT12/16
#define BTDT_FFAT32 0x0020000  ///< include FAT32
#define BTDT_FexFAT 0x0040000  ///< include exFAT
#define BTDT_FHPFS  0x0080000  ///< include HPFS
#define BTDT_FJFS   0x0100000  ///< include JFS
#define BTDT_FNTFS  0x0200000  ///< include NTFS
#define BTDT_INVERT 0x8000000  ///< invert filter selection
//@}

/// @name qs_extcmd->seslistdlg options
//@{
#define SLDO_CDEV      0x0100  ///< show only current device sessions
#define SLDO_NOSHOWALL 0x0200  ///< remove "show all" option when TLDO_CDEV is on
#define SLDO_NOSELF    0x0400  ///< exclude dialog`s session from list
#define SLDO_PLIST     0x0800  ///< allow Tab key with TLDR_TAB|result code
#define SLDO_HINTCOL   0x1000  ///< special hint color present 
#define SLDO_NEWSES    0x2000  ///< start new session via F3|n keys
#define SLDO_HCOLMASK  0x000F  ///< hint color mask (MSG_GRAY ... MSG_WHITE)
//@}

/// @name qs_extcmd->tasklistdlg options
//@{
#define TLDO_PDEL      0x0100  ///< allow Del key with TLDR_DEL|result code
#define TLDO_EDEL      0x0200  ///< process Del key internally (process kill dlg)
#define TLDO_PLIST     0x0800  ///< allow Tab key with TLDR_TAB|result code
#define TLDO_NEWSES    0x2000  ///< start new session via F3|n keys
//@}

/// @name tasklistdlg & seslistdlg result masks
//@{
#define TLDR_TAB   0x80000000  ///< Tab pressed
#define TLDR_DEL   0x40000000  ///< Del pressed (tasklistdlg only)
//@}


#ifdef __cplusplus
}
#endif

#endif // QSINIT_EXTCMD_SC
