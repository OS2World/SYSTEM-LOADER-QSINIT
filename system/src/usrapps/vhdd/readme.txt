
   This is another one virtual disk, but for QSINIT testing, basically ;)

   VHDD shell command creates some kind of "dynamically expanded HDD image"
for testing of PARTMGR functionality.
   Image can be created on any mounted FAT/FAT32 partition (including PAE
RAM disk).
   Max 4Gb FAT32 file size is well enough for everything (just do not use
"wipe disk" mode in format ;)).

   To use this - build it, makefile will add vhdd.dll into QSINIT.LDI.
   Then insert "lm vhdd" to QSSETUP.CMD or just type the same in QSINIT shell
to load module and install VHDD command.
