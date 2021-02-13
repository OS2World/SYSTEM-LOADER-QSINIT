
   This is another one virtual disk, but for QSINIT testing, basically ;)

   VHDD shell command creates some kind of "dynamically expanded HDD image"
for testing of PARTMGR functionality.
   Image can be created on any mounted writeable (FAT*) partition (including
PAE RAM disk).
   Max 4Gb FAT32 file size is well enough for everything (just do not use
"wipe disk" mode in format ;)). Using exFAT allows to make a larger file,
but FatFS`s exFAT implementation is terribly slow for huge files.

   One good thing you can make with this module - is a backup of your disk
partition structure by "dmgr clone struct" command.

   Example of backup (vhdd disk mounted to hd3):
     dmgr clone hd3 struct hd0 ident sameid

   Example of restoration (as a last chance when you wiped MBR accidentally ;)
     dmgr clone hd0 struct hd3 ident sameid nowipe

   Do not forget NOWIPE here - it prevents clearing boot sectors on all
partitions of target disk!
   SAMEID option disallow generation of new LVM/GPT serial numbers.
   IDENT makes sector-to-sector copy of partition data (by default QSINIT
creates partitions in own way - but in the same position).

   Note, what when you restore configuration - target disk can be smaller,
than source (hd3) - "dmgr clone" allow this, it checks for the actual end
of partition data, not for disk length itself.

   p.s. fixed size VHD support was added in rev.463. Nothing special, just
a plain read/write & image creation.

   p.p.s. plain binary file support added in rev.526. Nothing special again,
but with sector size support and some mounting options.

   An example what you can do with the binary file mounting:
   * get Arca OS UEFI boot ISO, open it in binary file editor in sysview
     and search for string MSDOS5.0 (FAT boot sector signature).
   * remember file offset of this line, for example 1CA1800 
   * type 
       vhdd mount iso_file_path offset=0x1CA1800
     and then mount this new disk as a big floppy (in the disk management in
     SysView).
   * now you have full access to FAT image of UEFI boot partition, used to
     boot from this ISO.
