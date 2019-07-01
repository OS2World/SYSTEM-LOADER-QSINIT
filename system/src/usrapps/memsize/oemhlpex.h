/* OEMHlp */

#define OEMHLP_GETMEMINFOEX    0x0011
/** write string to debug com port/QSINIT log buffer.

    Normally, data can be written into OEMHLP$ device via DosWrite, but ACPI.PSD
    may override this functionality, so a special IOCTL code added.

    Parameter - dw string_length
    Data      - string buffer */
#define OEMHLP_LOG_WRITE       0x0013

#pragma pack(1)

typedef struct _OEMHLP_MEMINFO   {  /* OEMHLP_GETMEMINFO packet */
   unsigned short   LowMemKB;  // # of kbs in 1st Mb
   unsigned long   HighMemKB;  // # memory size in kb (between 2nd Mb & 4Gb)
} OEMHLP_MEMINFO;

typedef struct _OEMHLP_MEMINFOEX {  /* OEMHLP_GETMEMINFOEX packet */
   unsigned long     LoPages;  // # of 4k pages below 4Gb border (except 1st mb)
   unsigned long     HiPages;  // # of 4k pages above 4Gb
   unsigned long  AvailPages;  // # of pages, available for OS/2 (except 1st mb)
} OEMHLP_MEMINFOEX;

#pragma pack()
