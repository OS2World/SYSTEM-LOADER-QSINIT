Compilation requires existing
   system\src\ldrapps\partmgr\partmgr.dll 
it takes MBR sector code and FAT boot sector from it.

FatFs code used directly from
   system\src\ldrapps\fsfat\fatfs

To compile for other platform a simple batch can be used:
   set toolprjkey=-oo
   call make.cmd %1 %2 %3
where -oo - OS/2 target, -ow - Win32 target, -od - DOS (DPMI) target
