

  hmio.h / hmio.c is example of direct access to PAE ram disk data via IOCTL
  calls to HD4DISK.ADD driver.

  There is no any protection in this method (i.e. you can destroy data on
  mounted RAM disk), with one exception.

  "Exclusive" open mode allow to block any write ops (via IOCTL) for all
  users except the single one who use it.

  Also, HD4DISK.ADD can be instructed to not show "ram disk" at all. With /i
  key - it will stay resident and give access to disk memory only via IOCTL
  calls.

  I.e. all upper memory can be used as plain storage for single app.

  May be, some kind of Ring 3 app/dll can be written to manage this memory,
  like it was with XMS/EMS in MS-DOS, but this is a long story. Feel free to
  implement it ;)

  IOCTL access is MUCH faster than ram disk i/o. Most of time it spend to OS/2
  call and copying itself is very fast.

  But, if size of block to copy exceeds 2Mb-4Kb OR supplied buffer is not page
  aligned - it will gain a large overhead.

  Run "RDTEST" to see results on yout PC.
