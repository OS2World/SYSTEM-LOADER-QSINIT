//
// QSINIT API
// additional system functions, available from EXTCMD module
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
   /** shows text mode selection dialog.
       @param header        Dialog header
       @param flags         Dialog flags (see vio_showlist() flags).
       @param conload       Make sure, that CONSOLE module is loaded (1/0).
                            Without CONSOLE function will show VGA/UEFI text
                            modes only.
       @return mode_id or 0 if esc was presses */
   u32t       _exicc (*selectmodedlg)(const char *header, u32t flags, int conload);

   /** shows "boot manager" selection dialog.
       @param header        Dialog header
       @param flags         Dialog flags (see vio_showlist() flags). Note,
                            that default position will be overriden to top
                            left corner. Use MSG_POS_CUSTOM value here to
                            force list into the center.
       @param type          Dialog type (see BTDT_* below)
       @param [out] disk    Disk number (on success)
       @param [out] index   Partition index (on success, can be 0 for BTDT_HDD),
                            value of -1 means big floppy or mbr (for BTDT_HDD).
       @return 0 after success selection, E_SYS_UBREAK if Esc was pressed or
               one of disk error codes */
   qserr      _exicc (*bootmgrdlg   )(const char *header, u32t flags, u32t type,
                                      u32t *disk, long *index);

   /** shows "task list".
       @param header        Dialog header, can be 0
       @param flags         Dialog flags (see vio_showlist() flags). Note,
                            that default position will be overriden to top
                            left corner. Use MSG_POS_CUSTOM value here to
                            force list into the center of screen.
       @param opts          task list options (see TLDT_* below)
       @param updatekey     update list key code (session list will be updated
                            when dialog receive this key press), use 0 to ignore.
       @return session id was selected or 0. If 'Del' pressed and no TLDT_NODEL
               option - returns id|0x80000000 */
   u32t       _exicc (*tasklistdlg  )(const char *header, u32t flags, u32t opts,
                                      u16t updatekey);

} _qs_extcmd, *qs_extcmd;

/// @name qs_extcmd->bootmgrdlg type values
//@{
#define BTDT_LVM            0  ///< show all partitions, marked as bootable in LVM
#define BTDT_HDD            1  ///< show HDD list only
#define BTDT_ALL            2  ///< show all partitions in plain list
#define BTDT_DISKTREE       3  ///< show all partitions, but with submenu for every HDD
#define BTDT_NOEMU     0x1000  ///< skip emulated HDDs in enumeration
//@}

/// @name qs_extcmd->tasklistdlg opts flags
//@{
#define TLDT_CDEV      0x0100  ///< show only current device sessions
#define TLDT_NOSHOWALL 0x0200  ///< remove "show all" option when TLDT_CDEV is on
#define TLDT_NOSELF    0x0400  ///< exclude dialog`s session from list
#define TLDT_NODEL     0x0800  ///< no "delete" key handling
#define TLDT_HINTCOL   0x1000  ///< special hint color present 
#define TLDT_HCOLMASK  0x000F  ///< hint color mask (MSG_GRAY ... MSG_WHITE)
//@}

#ifdef __cplusplus
}
#endif

#endif // QSINIT_EXTCMD_SC
