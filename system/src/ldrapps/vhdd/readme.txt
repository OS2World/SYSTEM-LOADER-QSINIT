
   This is another one virtual disk, but for QSINIT testing, basically ;)

   VHDD shell command creates some kind of "dynamically expanded HDD image"
for testing of PARTMGR functionality.
   Image can be created on any mounted FAT/FAT32 partition (including PAE
RAM disk).
   Max 4Gb FAT32 file size is well enough for everything (just do not use
"wipe disk" mode in format ;)).

   One good thing you can make with this module - is a backup of your disk
partition structure by "dmgr clone struct" command.

   Example of backup (vhdd disk mounted to hd3):
     dmgr clone hd3 struct hd0 ident sameid

   Example of restoration (as a last chance when you wiped MBR accidentally ;)
     dmgr clone hd0 struct hd3 ident sameid nowipe

   Do not forget NOWIPE here - it prevents boot sectors clearing on all
partitions of target disk!
   SAMEID option disallow generation of new LVM serial numbers and so on.
   IDENT makes sector-to-sector copy of partition data (by default QSINIT
creates partitions in own way - but in the same position).

   Note what when you restore configuration - target disk will be smaller,
than source (hd3) - "dmgr clone" allow this, it checks for the actual end
of partition data, not disk length itself.
