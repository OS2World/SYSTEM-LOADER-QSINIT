/* OEMHlp */

#define OEMHLP_GETMEMINFOEX    0x0011

/** write string to debug com port/QSINIT log buffer.

    Normally, data can be written into OEMHLP$ device via DosWrite, but ACPI.PSD
    may override this functionality, so a special IOCTL code added.

    Parameter - dw string_length
    Data      - string buffer */
#define OEMHLP_LOG_WRITE       0x0013

/** query BIOS table physical address and length.

    Function will return ERROR_I24_READ_FAULT if table is missing and 
    ERROR_I24_BAD_COMMAND if loader version is old.

    Bit 0 in Status field signals about broken table (control sum mismatch).

    Parameter - dw table number (OEMHLP_TAB_*)
    Data      - buffer for OEMHLP_BIOSTABLE */
#define OEMHLP_GETBIOSTABLE    0x0014

#define OEMHLP_TAB_ACPI             0  // ACPI root
#define OEMHLP_TAB_SMBIOS           1  // SMBIOS
#define OEMHLP_TAB_SMBIOS3          2  // SMBIOS v.3
#define OEMHLP_TAB_DMI              3  // Legacy DMI
#define OEMHLP_TAB_PNPBIOS          4  // PNP BIOS
#define OEMHLP_TAB_BIOS32           5  // BIOS32 Service Directory
#define OEMHLP_TAB_IRQROUTE         6  // PCI Interrupt Routing
#define OEMHLP_TAB_MP               7  // Intel Multiprocessor Configuration

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

typedef struct _OEMHLP_BIOSTABLE {  /* OEMHLP_GETBIOSTABLE packet */
   unsigned long        Addr;  // physical address
   unsigned long      Length;  // table length
   unsigned short     Status;  // bit 0 = 1 means crc error in table
} OEMHLP_BIOSTABLE;

#pragma pack()
