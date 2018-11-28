#define HELP_LASTHLPIDX  2160  // must be the last

const char *Hints[HELP_LASTHLPIDX - 2000 + 1]= {
  "Type kernel file name here",
  "Preload all basedev and psd files before initing ACPI (OS/4)",
  "Call boot time kernel CONFIG.SYS editor (OS/4)",
  "Turn off kernel boot logo",
  "Turn off kernel revision string",
  "Load original IBM messages instead of own ones",
  "Call memory viewer before going to kernel",
  "Turn off hard disk Advanced Format aligment during boot i/o",
  "Reset all MTRR changes, was done in QSINIT",
  "Manual selection of OS/2 boot drive letter (optional)",
  "Allow Ctrl-C check by kernel debugger (slow down the debug kernel)",
  "COM port link cable with hardware flow control support",
  "Verbose kernel log (internal)",
  "Push service key for OS/2 kernel",
  "Debug COM port address", "Debug COM port baud rate",
  "Alternative kernel SYM file name (up to 11 chars)",
  "Import kernel file from REV_ARCH.ZIP in the root of boot disk",
  "Select optional memory limit for OS/2, in megabytes",
  "",
  "Load file as OS2LDR, not a kernel",
  "Exit this loader and run spicified one",
  "Switch to kernel selection dialog",
  "","",
  "Type command to run",
  "Echo is on by default",
  "Pause on exit",
  "Browse QSINIT apps to select file name",
  "Execute command in QSINIT shell",
  "View QSINIT internal log (also available via COM port)",
  "Power OFF this PC",
  "Select HDD and boot from it without system reset",
  "View CPU model info and available features",
  "Save QSINIT internal log to a file on mounted partition",
  "Replace master boot record (MBR) code on disk",
  "Create a new text file in a new edit window",
  "Locate and open a text file in an edit window",
  "Save the file in active edit window",
  "Save the current file under a different name",
  "Search for text",
  "Search for text and replace it with new text",
  "Repeat the last Find or Replace command",
  "Undo the previous editor operation",
  "Remove the selected text and put it in the clipboard",
  "Copy the selected text into the clipboard",
  "Insert text from the clipboard at the cursor position",
  "Change the size or position of the active window",
  "Make the next window active",
  "Make the previous window active",
  "Close the active window",
  "Open the clipboard window",
  "Delete the selected text",
  "Arrange windows on desktop by tiling",
  "Arrange windows on desktop by cascading",
  "Close all windows on the desktop",
  "Redraw the screen",
  "Enlarge or restore the size of the active window",
  "Show the list of all open windows",
  "Show menu with IBM Boot manager bootable partitions",
  "Write MBR code and empty partition table to the empty disk",
  "Raw disk data editor (physical sector view)",
  "Go to specified sector/memory location in binary editor window",
  "Find binary data on physical disk or in memory",
  "Repeat last binary Find command",
  "Calendar window",
  "View current physical sector as partition table",
  "Exit back to QSINIT",
  "Open \"Window\" menu",
  "Save pure log text to a file (without time marks)",
  "View current sector as boot sector BPB",
  "Save sector(s) to a binary file on disk",
  "Copy binary file contents to sector(s) on disk",
  "Copy sector(s) to other location on any of system disks",
  "Select disk to manage",
  "Select partition or free space to make action on it",
  "Select action here",
  "Manage hard disk partitions",
  "Select action for entire disk here",
  "Select new partition parameters",
  "Type size in megabytes or percents here (ex.: 400000 or 50%)",
  "Force rescanning of all disks (commonly not required)",
  "View disk sector as LVM DLAT sector",
  "The name assigned to the disk containing this sector",
  "Disk serial number (hex dword)",
  "System boot disk serial number (hex dword)",
  "Install Flags (used by the Install program)",
  "Number of cylinders on disk (assumed by LVM)",
  "Number of heads (assumed by LVM)",
  "Sector per track on disk (assumed by LVM)",
  "Used to keep track of reboots initiated by install",
  "The serial number of the volume",
  "The serial number of this partition",
  "The starting sector of the partition",
  "The size of the partition, in sectors",
  "Volume/partition is on the Boot Manager Menu",
  "Installable volume",
  "The name assigned to the volume by the user",
  "The name assigned to the partition",
  "The drive letter assigned to the partition",
  "Calculator window",
  "Edit this PC memory",
  "Go to specified sector in disk data editor",
  "Press arrow down to select one of known partition type GUIDs",
  "Enter partition type GUID string manually here",
  "Press arrow down to select available OS/2 drive letter",
  "Press arrow down to select available QSINIT drive letter",
  "Edit binary file in hex mode",
  "Press arrow down to select file system type",
  "Quick format",
  "Long format, with checking for bad sectors",
  "Wipe format, clearing disk space",
  "Clone entire disk to empy another one",
  "Select suitable empty disk",
  "Clone partition structure only",
  "Clone entire disk (partition structure and data)",
  "Clone disk unique IDs (LVM serial, windows id, GPT GUIDs)",
  "Copy MBR boot code",
  "Make identical clone of MBR areas (sector to sector copy)",
  "Ignore BIOS CHS mimatch",
  "Wipe system sectors on target disk (structure only copying)",
  "Clone one partition`s data to another",
  "Select target disk here",
  "Select target partition",
  "",
  "Select codepage for file i/o and HPFS format",
  "Change default file name of boot loader file",
  "Put QSINIT binaries to the root (FAT volumes only)",
  "Update boot code on partition",
  "Put QSINIT binaries and update partition bootstrap for it",
  "Svae a block of memory to file on disk",
  "Create and edit a new binary file",
  "Locate and open a binary file in an edit window",
  "Type memory position here in hex form",
  "The size of memory block to save, in bytes (use 0x prefix for hex)",
  "Type position in this file here, in hex form",
  "",
  "Truncate binary file at the current position",
  "Press arrow down to select codepage from the list of available",
  "",
  "Select kernel boot log size, in kilobytes",
  "Select EXTension instead of SYS for boot CONFIG.SYS",
  "QSINIT batch file name to execute before launching kernel",
  "Level of QSINIT log message to copy into kernel log",
  "Start command shell in a new session",
  "Show session list",
  "Switch to the selected session",
  "System process listing and information",
  "",
  "Switch to the selected process",
  "Terminate selected process",
  "Terminate process and any processes which were started by it",
  "","","","","","","","",""
  };

const char *EmptyStr="";
