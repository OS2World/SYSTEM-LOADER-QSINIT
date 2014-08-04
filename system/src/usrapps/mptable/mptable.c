/*
 * Copyright (c) 1996, by Steve Passe
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the developer may NOT be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      $Id: mptable.c,v 1.5 1997/06/23 20:23:44 fsmp Exp $
 */

/*
 * mptable.c
 */

#define VMAJOR                  2
#define VMINOR                  0
#define VDELTA                  12

/*
 * this will cause the raw mp table to be dumped to /tmp/mpdump
 *
#define RAW_DUMP
 */

#define MP_SIG                  0x5f504d5f      /* _MP_ */
#define EXTENDED_PROCESSING_READY
#define OEM_PROCESSING_READY_NOT

#include <stdio.h>

#ifdef __QSINIT__
#include <qsutil.h>
#include <qspage.h>
#include <qsshell.h>
#include <errno.h>
typedef u8t  u_char;
typedef u16t u_short;
typedef u32t u_int;
typedef u32t u_long;
typedef u64t u_int64_t;
/* physical address */
typedef u32t vm_offset_t;

#define fprintf(x,...)      cmd_printf(__VA_ARGS__)
#define printf(...)         cmd_printf(__VA_ARGS__)
#define fflush(x)
#define system(x)
#define stderr 0
#define stdout 0

#else // __QSINIT__

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>

#include <machine/types.h>

#ifdef __OS2__
#include <stdint.h>
typedef uint8_t u_char;
typedef uint16_t u_short;
typedef uint32_t u_int;
typedef uint32_t u_long;
typedef uint64_t u_int64_t;

/* physical address */
typedef uint32_t vm_offset_t;

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#endif // __OS2__
#endif

/* physical memory access */
typedef struct MemFile *HMemFile;
static HMemFile memOpen();
static ssize_t memRead(HMemFile hmf, void *buf, size_t nbyte);
static off_t memSeek(HMemFile hmf, off_t offset, int whence);
static void memClose(HMemFile hmf);

#ifdef __OS2__
/* OS/2 specific implementation with memory map calls */
#define INCL_DOSDEVICES
#define INCL_DOSDEVIOCTL
#define INCL_DOSERRORS
#define INCL_DOSFILEMGR
#include <os2.h>
//#include <ssm.h>

/* SSM$ memory access IOCTL and its parameter packet */
#define  SSMDD_MEM_CATEGORY 0x0080
#define  SSMDD_LOCKMEM   0x0046
typedef struct
{
   PVOID pBuffer;
   ULONG ulBufSize;
   PVOID plockh;
   ULONG ulFlags;
   ULONG rc;
} SsmLockmemParm;

/* secret names for SSM$ memory mapping functions. They map physical memory into private process space */
#define SSM_MAPRAM   0x0100 /* SSM_LOCKMEM_RESERVED1 */
#define SSM_UNMAPRAM 0x0200 /* SSM_LOCKMEM_RESERVED2 */

struct MemFile
{
    HFILE hSSM;
    uint8_t *pMapped;    /* pointer to area where memory is mapped */
    vm_offset_t mapBase; /* physical address for beginning of the map */
    size_t mapSize;      /* size of mapped region */
    vm_offset_t mmfOff;        /* current position in emulated file */
};

static HMemFile memOpen()
{
    static const char SSM_NAME[] = "\\DEV\\SSM$";

    HMemFile hmf = (HMemFile)malloc(sizeof(*hmf));

    ULONG ulActionTaken;
    APIRET rc = DosOpen(
        (PSZ)SSM_NAME, &hmf->hSSM, &ulActionTaken, 0L, FILE_SYSTEM,
        OPEN_ACTION_OPEN_IF_EXISTS | OPEN_ACTION_FAIL_IF_NEW, OPEN_ACCESS_READWRITE |
        OPEN_FLAGS_NOINHERIT | OPEN_FLAGS_FAIL_ON_ERROR | OPEN_SHARE_DENYNONE, NULL);
    if (rc != NO_ERROR)
    {
        fprintf(stderr, "FATAL: Can't open %s. Error %u. Do you have ssmdd.sys loaded?\n", SSM_NAME, (unsigned)rc);
        errno = EACCES;
        free(hmf);
        return NULL;
    }

    /* map first 1 MiB for beginning */
    hmf->pMapped = NULL;
    hmf->mapBase = 0;
    hmf->mapSize = 0x100000;
    hmf->mmfOff = 0;

    SsmLockmemParm mapParam;
    mapParam.pBuffer    = (PVOID)(hmf->mapBase);
    mapParam.ulBufSize  = hmf->mapSize - hmf->mapBase;
    mapParam.plockh     = &hmf->pMapped;
    mapParam.ulFlags    = SSM_MAPRAM;
    ULONG ulParmSize    = sizeof(mapParam);
    rc = DosDevIOCtl(hmf->hSSM, SSMDD_MEM_CATEGORY, SSMDD_LOCKMEM,
                     &mapParam, ulParmSize, &ulParmSize,
                     &mapParam, ulParmSize, &ulParmSize);

    if (rc != NO_ERROR)
    {
        fprintf(stderr, "FATAL: SSMDD_LOCKMEM RC is %u\n", (unsigned)rc);
        errno = EACCES;
        memClose(hmf);
        return NULL;
    }

    if (!hmf->pMapped)
    {
        fprintf(stderr, "FATAL: SSMDD_LOCKMEM returned zero pointer to the mapped area\n");
        errno = EACCES;
        memClose(hmf);
        return NULL;
    }

    if (mapParam.rc)
    {
        fprintf(stderr, "FATAL: SSMDD_LOCKMEM internal RC is %u\n", (unsigned)mapParam.rc);
        errno = EACCES;
        memClose(hmf);
        return NULL;
    }

    return hmf;
}

static ssize_t memRead(HMemFile hmf, void *buf, size_t nbyte)
{
    vm_offset_t mapEnd = hmf->mapBase + hmf->mapSize;
    if ((hmf->mmfOff < hmf->mapBase) || (hmf->mmfOff > mapEnd))
    {
        /* we can move mapping but for now lets just return EOF */
        fprintf(stderr, "WARNING: access to physical address 0x%0X which is outside of the mapped area\n", (unsigned)hmf->mmfOff);
        return 0;
    }

    vm_offset_t newOff = hmf->mmfOff + nbyte;
    if (newOff >= mapEnd)
    {
        /* crossing map area end. Reduce the read block */
        nbyte -= newOff - mapEnd;
        newOff = mapEnd;
    }

    memcpy(buf, hmf->pMapped + hmf->mmfOff - hmf->mapBase, nbyte);
    hmf->mmfOff = newOff;
    return nbyte;
}

static off_t memSeek(HMemFile hmf, off_t offset, int whence)
{
    switch (whence)
    {
    case SEEK_SET:
        hmf->mmfOff = offset;
        break;
    case SEEK_CUR:
        hmf->mmfOff += offset;
        break;
    case SEEK_END:
        hmf->mmfOff = (vm_offset_t)0x80000000 + offset;
        break;
    default:
        errno = EINVAL;
        return (off_t)(-1);
    }

    return hmf->mmfOff;
}

static void memClose(HMemFile hmf)
{
    /* unmap last used region */
    SsmLockmemParm mapParam;
    mapParam.pBuffer   = hmf->pMapped;
    mapParam.ulBufSize = 0;
    mapParam.ulFlags   = SSM_UNMAPRAM;
    ULONG ulParmSize   = sizeof(mapParam);
    DosDevIOCtl(hmf->hSSM, SSMDD_MEM_CATEGORY, SSMDD_LOCKMEM,
                &mapParam, ulParmSize, &ulParmSize,
                &mapParam, ulParmSize, &ulParmSize);

    DosClose(hmf->hSSM);
    free(hmf);
}

#elif defined(__QSINIT__)
/* QSINIT specific implementation over /dev/mem */
struct MemFile
{
    u32t pos;
};

static HMemFile memOpen()
{
    HMemFile hmf = (HMemFile)malloc(sizeof(struct MemFile));
    hmf->pos = 0;
    return hmf;
}

static ssize_t memRead(HMemFile hmf, void *buf, size_t nbyte)
{
    if (nbyte) {
       if (hmf->pos>_1MB || hmf->pos+nbyte>_1MB) {
          void *addr;
          if (hmf->pos+nbyte < hmf->pos) nbyte = FFFF - hmf->pos + 1;
       
          addr = pag_physmap(hmf->pos, nbyte, 0);
          if (!addr) return 0;
          memcpy(buf, addr, nbyte);
          pag_physunmap(addr);
       } else {
          memcpy(buf, (u8t*)hlp_segtoflat(hmf->pos>>4) + (hmf->pos&0xF), nbyte);
       }
       hmf->pos+=nbyte;
    }
    return nbyte;
}

static off_t memSeek(HMemFile hmf, off_t offset, int whence)
{
    switch (whence)
    {
    case SEEK_SET:
        hmf->pos = offset;
        break;
    case SEEK_CUR:
        hmf->pos += offset;
        break;
    case SEEK_END:
        hmf->pos = (vm_offset_t)FFFF + offset;
        break;
    default:
        errno = EINVAL;
        return (off_t)(-1);
    }

    return hmf->pos;
}

static void memClose(HMemFile hmf)
{
}
#else
/* BSD specific implementation over /dev/mem */
struct MemFile
{
    int pfd;
}

static HMemFile memOpen()
{
    HMemFile hmf = (HMemFile)malloc(sizeof(MemFile));
    hmf->pfd = open( "/dev/mem", O_RDONLY );
    if (hmf->pfd == -1)
    {
        free(hmf);
        return NULL;
    }

    return hmf;
}

static ssize_t memRead(HMemFile hmf, void *buf, size_t nbyte)
{
    return read(hmf->pfd, buf, nbyte);
}

static off_t memSeek(HMemFile hmf, off_t offset, int whence)
{
    return lseek(hmf->pfd, offset, whence);
}

static void memClose(HMemFile hmf)
{
    close(hmf->pfd);
}
#endif

#define SEP_LINE \
"\n-------------------------------------------------------------------------------\n"

#define SEP_LINE2 \
"\n===============================================================================\n"

/* EBDA is @ 40:0e in real-mode terms */
#define EBDA_POINTER            0x040e          /* location of EBDA pointer */

/* CMOS 'top of mem' is @ 40:13 in real-mode terms */
#define TOPOFMEM_POINTER        0x0413          /* BIOS: base memory size */

#define DEFAULT_TOPOFMEM        0xa0000

#define BIOS_BASE               0xf0000
#define BIOS_BASE2              0xe0000
#define BIOS_SIZE               0x10000
#define ONE_KBYTE               1024

#define GROPE_AREA1             0x80000
#define GROPE_AREA2             0x90000
#define GROPE_SIZE              0x10000

#define PROCENTRY_FLAG_EN       0x01
#define PROCENTRY_FLAG_BP       0x02
#define IOAPICENTRY_FLAG_EN     0x01

#define MAXPNSTR                132

enum busTypes {
    CBUS = 1,
    CBUSII = 2,
    EISA = 3,
    ISA = 6,
    PCI = 13,
    XPRESS = 18,
    MAX_BUSTYPE = 18,
    UNKNOWN_BUSTYPE = 0xff
};

typedef struct BUSTYPENAME {
    u_char      type;
    char        name[ 7 ];
} busTypeName;

static busTypeName busTypeTable[] =
{
    { CBUS,             "CBUS"   },
    { CBUSII,           "CBUSII" },
    { EISA,             "EISA"   },
    { UNKNOWN_BUSTYPE,  "---"    },
    { UNKNOWN_BUSTYPE,  "---"    },
    { ISA,              "ISA"    },
    { UNKNOWN_BUSTYPE,  "---"    },
    { UNKNOWN_BUSTYPE,  "---"    },
    { UNKNOWN_BUSTYPE,  "---"    },
    { UNKNOWN_BUSTYPE,  "---"    },
    { UNKNOWN_BUSTYPE,  "---"    },
    { UNKNOWN_BUSTYPE,  "---"    },
    { PCI,              "PCI"    },
    { UNKNOWN_BUSTYPE,  "---"    },
    { UNKNOWN_BUSTYPE,  "---"    },
    { UNKNOWN_BUSTYPE,  "---"    },
    { UNKNOWN_BUSTYPE,  "---"    },
    { UNKNOWN_BUSTYPE,  "---"    },
    { UNKNOWN_BUSTYPE,  "---"    }
};

char* whereStrings[] = {
    "Extended BIOS Data Area",
    "BIOS top of memory",
    "Default top of memory",
    "BIOS",
    "Extended BIOS",
    "GROPE AREA #1",
    "GROPE AREA #2"
};

typedef struct TABLE_ENTRY {
    u_char      type;
    u_char      length;
    char        name[ 32 ];
} tableEntry;

tableEntry basetableEntryTypes[] =
{
    { 0, 20, "Processor" },
    { 1,  8, "Bus" },
    { 2,  8, "I/O APIC" },
    { 3,  8, "I/O INT" },
    { 4,  8, "Local INT" }
};

tableEntry extendedtableEntryTypes[] =
{
    { 128, 20, "System Address Space" },
    { 129,  8, "Bus Hierarchy" },
    { 130,  8, "Compatibility Bus Address" }
};

/* MPTable structures. They have to be defined without dependency from
   compiler's structure packing */
#pragma pack(1)

/* MP Floating Pointer Structure */
typedef struct MPFPS {
    char        signature[ 4 ];
    u_long      pap;
    u_char      length;
    u_char      spec_rev;
    u_char      checksum;
    u_char      mpfb1;
    u_char      mpfb2;
    u_char      mpfb3;
    u_char      mpfb4;
    u_char      mpfb5;
} mpfps_t;

/* MP Configuration Table Header */
typedef struct MPCTH {
    char        signature[ 4 ];
    u_short     base_table_length;
    u_char      spec_rev;
    u_char      checksum;
    u_char      oem_id[ 8 ];
    u_char      product_id[ 12 ];
    u_long      oem_table_pointer;
    u_short     oem_table_size;
    u_short     entry_count;
    u_long      apic_address;
    u_short     extended_table_length;
    u_char      extended_table_checksum;
    u_char      reserved;
} mpcth_t;


typedef struct PROCENTRY {
    u_char      type;
    u_char      apicID;
    u_char      apicVersion;
    u_char      cpuFlags;
    u_long      cpuSignature;
    u_long      featureFlags;
    u_long      reserved1;
    u_long      reserved2;
} ProcEntry;

typedef struct BUSENTRY {
    u_char      type;
    u_char      busID;
    char        busType[ 6 ];
} BusEntry;

typedef struct IOAPICENTRY {
    u_char      type;
    u_char      apicID;
    u_char      apicVersion;
    u_char      apicFlags;
    u_long      apicAddress;
} IOApicEntry;

typedef struct INTENTRY {
    u_char      type;
    u_char      intType;
    u_short     intFlags;
    u_char      srcBusID;
    u_char      srcBusIRQ;
    u_char      dstApicID;
    u_char      dstApicINT;
} IntEntry;


/*
 * extended entry type structures
 */

typedef struct SASENTRY {
    u_char      type;
    u_char      length;
    u_char      busID;
    u_char      addressType;
    u_int64_t   addressBase;
    u_int64_t   addressLength;
} SasEntry;


typedef struct BHDENTRY {
    u_char      type;
    u_char      length;
    u_char      busID;
    u_char      busInfo;
    u_char      busParent;
    u_char      reserved[ 3 ];
} BhdEntry;


typedef struct CBASMENTRY {
    u_char      type;
    u_char      length;
    u_char      busID;
    u_char      addressMod;
    u_int       predefinedRange;
} CbasmEntry;

#pragma pack()


static void apic_probe( vm_offset_t* paddr, int* where );

static void MPConfigDefault( int featureByte );

static void MPFloatingPointer( vm_offset_t paddr, int where, mpfps_t* mpfps );
static void MPConfigTableHeader( u_long pap );

static int readType( void );
static void seekEntry( vm_offset_t addr );
static void readEntry( void* entry, int size );

static void processorEntry( void );
static void busEntry( void );
static void ioApicEntry( void );
static void intEntry( void );

static void sasEntry( void );
static void bhdEntry( void );
static void cbasmEntry( void );

static void doOptionList( void );
static void doDmesg( void );
static void pnstr( char* s, int c );

/* global data */
HMemFile hPhysMem = 0;

int     busses[ 16 ];
int     apics[ 16 ];

int     ncpu;
int     nbus;
int     napic;
int     nintr;

int     dmesg = 0;
int     grope = 0;
int     verbose = 0;

static void
usage( void )
{
    fprintf( stderr, "\nusage: mptable [-help][-dmesg][-verbose]\n" );
    fprintf( stderr, "where:\n" );
    fprintf( stderr, "  '-dmesg' includes a dmesg dump\n" );
    fprintf( stderr, "  '-grope' looks in areas it shouldn't NEED to\n" );
    fprintf( stderr, "  '-help'  prints this message and exits\n" );
    fprintf( stderr, "  '-verbose' prints extra info\n" );
#ifdef __QSINIT__
    fprintf( stderr, "  '-log'   copy out to system log\n" );
#endif
    exit( 0 );
}

/*
 * 
 */
int
main( int argc, char *argv[] )
{
    vm_offset_t paddr;
    int         where;
    mpfps_t     mpfps;
    int         defaultConfig;

#ifndef __WATCOMC__
    /* this should be already defined in unistd.h */
    extern char* optarg;
    extern int  optind, optreset;
#endif
    int         ch;

    /* announce ourselves */
    puts( SEP_LINE2 );

    printf( "MPTable, version %d.%d.%d\n", VMAJOR, VMINOR, VDELTA );

#ifdef __QSINIT__
    {
        int ii = 1,
           log = 0;
        while (ii<argc) {
           if (stricmp(argv[ii], "-dmesg")==0) dmesg = 1; else
           if (stricmp(argv[ii], "-grope")==0) grope = 1; else
           if (stricmp(argv[ii], "-log"  )==0) log   = 2; else
           if (stricmp(argv[ii], "-verbose")==0) verbose = 1; else
              usage();
           ii++;
        }
        cmd_printseq(0, 1|log, 0);
    }
#else
    while ((ch = getopt(argc, argv, "d:g:h:v:")) != EOF) {
        switch(ch) {
        case 'd':
            if ( strcmp( optarg, "mesg") == 0 )
                dmesg = 1;
            else
                dmesg = 0;
            break;
        case 'h':
            if ( strcmp( optarg, "elp") == 0 )
                usage();
            break;
        case 'g':
            if ( strcmp( optarg, "rope") == 0 )
                grope = 1;
            break;
        case 'v':
            if ( strcmp( optarg, "erbose") == 0 )
                verbose = 1;
            break;
        default:
            usage();
        }
        argc -= optind;
        argv += optind;
#ifndef __WATCOMC__
        /* seems as FreeBSD specific. No such definition in SUS */
        optreset = 1;
#endif
        optind = 0;
    }
#endif
    /* open physical memory for access to MP structures */
    hPhysMem = memOpen();
    if ( !hPhysMem ) {
        perror( "mem open" );
        exit( 1 );
    }

    /* probe for MP structures */
    apic_probe( &paddr, &where );
    if ( where <= 0 ) {
        fprintf( stderr, "\n MP FPS NOT found,\n" );
        fprintf( stderr, " suggest trying -grope option!!!\n\n" );
        return 1;
    }

    if ( verbose )
        printf( "\n MP FPS found in %s @ physical addr: 0x%08x\n",
              whereStrings[ where - 1 ], paddr );

    puts( SEP_LINE );

    /* analyze the MP Floating Pointer Structure */
    MPFloatingPointer( paddr, where, &mpfps );

    puts( SEP_LINE );

    /* check whether an MP config table exists */
    if ( defaultConfig = mpfps.mpfb1 )
        MPConfigDefault( defaultConfig );
    else
        MPConfigTableHeader( mpfps.pap );

    /* build "options" entries for the kernel config file */
    doOptionList();

    /* do a dmesg output */
    if ( dmesg )
        doDmesg();

    puts( SEP_LINE2 );

    return 0;
}


/*
 * set PHYSICAL address of MP floating pointer structure
 */
#define NEXT(X)         ((X) += 4)
static void
apic_probe( vm_offset_t* paddr, int* where )
{
    /*
     * c rewrite of apic_probe() by Jack F. Vogel
     */

    int         x;
    u_short     segment;
    vm_offset_t target;
    u_int       buffer[ BIOS_SIZE / sizeof( int ) ];

    if ( verbose )
        printf( "\n" );

    /* search Extended Bios Data Area, if present */
    if ( verbose )
        printf( " looking for EBDA pointer @ 0x%04x, ", EBDA_POINTER );
    seekEntry( (vm_offset_t)EBDA_POINTER );
    readEntry( &segment, 2 );
    if ( segment ) {                /* search EBDA */
        target = (vm_offset_t)segment << 4;
        if ( verbose )
            printf( "found, searching EBDA @ 0x%08x\n", target );
        seekEntry( target );
        readEntry( buffer, ONE_KBYTE );

        for ( x = 0; x < ONE_KBYTE / sizeof ( unsigned int ); NEXT(x) ) {
            if ( buffer[ x ] == MP_SIG ) {
                *where = 1;
                *paddr = (x * sizeof( unsigned int )) + target;
                return;
            }
        }
    }
    else {
        if ( verbose )
            printf( "NOT found\n" );
    }

    /* read CMOS for real top of mem */
    seekEntry( (vm_offset_t)TOPOFMEM_POINTER );
    readEntry( &segment, 2 );
    --segment;                                          /* less ONE_KBYTE */
    target = segment * 1024;
    if ( verbose )
        printf( " searching CMOS 'top of mem' @ 0x%08x (%dK)\n",
                target, segment );
    seekEntry( target );
    readEntry( buffer, ONE_KBYTE );

    for ( x = 0; x < ONE_KBYTE / sizeof ( unsigned int ); NEXT(x) ) {
        if ( buffer[ x ] == MP_SIG ) {
            *where = 2;
            *paddr = (x * sizeof( unsigned int )) + target;
            return;
        }
    }

    /* we don't necessarily believe CMOS, check base of the last 1K of 640K */
    if ( target != (DEFAULT_TOPOFMEM - 1024)) {
        target = (DEFAULT_TOPOFMEM - 1024);
        if ( verbose )
            printf( " searching default 'top of mem' @ 0x%08x (%dK)\n",
                    target, (target / 1024) );
        seekEntry( target );
        readEntry( buffer, ONE_KBYTE );

        for ( x = 0; x < ONE_KBYTE / sizeof ( unsigned int ); NEXT(x) ) {
            if ( buffer[ x ] == MP_SIG ) {
                *where = 3;
                *paddr = (x * sizeof( unsigned int )) + target;
                return;
            }
        }
    }

    /* search the BIOS */
    if ( verbose )
        printf( " searching BIOS @ 0x%08x\n", BIOS_BASE );
    seekEntry( BIOS_BASE );
    readEntry( buffer, BIOS_SIZE );

    for ( x = 0; x < BIOS_SIZE / sizeof( unsigned int ); NEXT(x) ) {
        if ( buffer[ x ] == MP_SIG ) {
            *where = 4;
            *paddr = (x * sizeof( unsigned int )) + BIOS_BASE;
            return;
        }
    }

    /* search the extended BIOS */
    if ( verbose )
        printf( " searching extended BIOS @ 0x%08x\n", BIOS_BASE2 );
    seekEntry( BIOS_BASE2 );
    readEntry( buffer, BIOS_SIZE );

    for ( x = 0; x < BIOS_SIZE / sizeof( unsigned int ); NEXT(x) ) {
        if ( buffer[ x ] == MP_SIG ) {
            *where = 5;
            *paddr = (x * sizeof( unsigned int )) + BIOS_BASE2;
            return;
        }
    }

    if ( grope ) {
        /* search additional memory */
        target = GROPE_AREA1;
        if ( verbose )
            printf( " groping memory @ 0x%08x\n", target );
        seekEntry( target );
        readEntry( buffer, GROPE_SIZE );

        for ( x = 0; x < GROPE_SIZE / sizeof( unsigned int ); NEXT(x) ) {
            if ( buffer[ x ] == MP_SIG ) {
                *where = 6;
                *paddr = (x * sizeof( unsigned int )) + GROPE_AREA1;
                return;
            }
        }

        target = GROPE_AREA2;
        if ( verbose )
            printf( " groping memory @ 0x%08x\n", target );
        seekEntry( target );
        readEntry( buffer, GROPE_SIZE );

        for ( x = 0; x < GROPE_SIZE / sizeof( unsigned int ); NEXT(x) ) {
            if ( buffer[ x ] == MP_SIG ) {
                *where = 7;
                *paddr = (x * sizeof( unsigned int )) + GROPE_AREA2;
                return;
            }
        }
    }

    *where = 0;
    *paddr = (vm_offset_t)0;
}


/*
 * 
 */
static void
MPFloatingPointer( vm_offset_t paddr, int where, mpfps_t* mpfps )
{

    /* read in mpfps structure*/
    seekEntry( paddr );
    readEntry( mpfps, sizeof( mpfps_t ) );

    /* show its contents */
    printf( "MP Floating Pointer Structure:\n\n" );

    printf( "  location:\t\t\t", where );
    switch ( where )
    {
    case 1:
        printf( "EBDA\n" );
        break;
    case 2:
        printf( "BIOS base memory\n" );
        break;
    case 3:
        printf( "DEFAULT base memory (639K)\n" );
        break;
    case 4:
        printf( "BIOS\n" );
        break;
    case 5:
        printf( "Extended BIOS\n" );
        break;

    case 0:
        printf( "NOT found!\n" );
        exit( 1 );
    default:
        printf( "BOGUS!\n" );
        exit( 1 );
    }
    printf( "  physical address:\t\t0x%08x\n", paddr );

    printf( "  signature:\t\t\t'" );
    pnstr( mpfps->signature, 4 );
    printf( "'\n" );

    printf( "  length:\t\t\t%d bytes\n", mpfps->length * 16 );
    printf( "  version:\t\t\t1.%1d\n", mpfps->spec_rev );
    printf( "  checksum:\t\t\t0x%02x\n", mpfps->checksum );

    /* bits 0:6 are RESERVED */
    if ( mpfps->mpfb2 & 0x7f ) {
        printf( " warning, MP feature byte 2: 0x%02x\n" );
    }

    /* bit 7 is IMCRP */
    printf( "  mode:\t\t\t\t%s\n", (mpfps->mpfb2 & 0x80) ?
            "PIC" : "Virtual Wire" );

    /* MP feature bytes 3-5 are expected to be ZERO */
    if ( mpfps->mpfb3 )
        printf( " warning, MP feature byte 3 NONZERO!\n" );
    if ( mpfps->mpfb4 )
        printf( " warning, MP feature byte 4 NONZERO!\n" );
    if ( mpfps->mpfb5 )
        printf( " warning, MP feature byte 5 NONZERO!\n" );
}


/*
 * 
 */
static void
MPConfigDefault( int featureByte )
{
    printf( "  MP default config type: %d\n\n", featureByte );
    switch ( featureByte ) {
    case 1:
        printf( "   bus: ISA, APIC: 82489DX\n" );
        break;
    case 2:
        printf( "   bus: EISA, APIC: 82489DX\n" );
        break;
    case 3:
        printf( "   bus: EISA, APIC: 82489DX\n" );
        break;
    case 4:
        printf( "   bus: MCA, APIC: 82489DX\n" );
        break;
    case 5:
        printf( "   bus: ISA+PCI, APIC: Integrated\n" );
        break;
    case 6:
        printf( "   bus: EISA+PCI, APIC: Integrated\n" );
        break;
    case 7:
        printf( "   bus: MCA+PCI, APIC: Integrated\n" );
        break;
    default:
        printf( "   future type\n" );
        break;
    }

    switch ( featureByte ) {
    case 1:
    case 2:
    case 3:
    case 4:
        nbus = 1;
        break;
    case 5:
    case 6:
    case 7:
        nbus = 2;
        break;
    default:
        printf( "   future type\n" );
        break;
    }

    ncpu = 2;
    napic = 1;
    nintr = 16;
}


/*
 * 
 */
static void
MPConfigTableHeader( u_long pap )
{
    vm_offset_t paddr;
    mpcth_t     cth;
    int         x, y;
    int         count, c;
    unsigned    type;
    vm_offset_t poemtp;
    void*       oemdata;

    if ( pap == 0 ) {
        printf( "MP Configuration Table Header MISSING!\n" );
        exit( 1 );
    }

    /* convert physical address to virtual address */
    paddr = (vm_offset_t)pap;

    /* read in cth structure */
    seekEntry( paddr );
    readEntry( &cth, sizeof( cth ) );

    printf( "MP Config Table Header:\n\n" );

    printf( "  physical address:\t\t0x%08x\n", pap );

    printf( "  signature:\t\t\t'" );
    pnstr( cth.signature, 4 );
    printf( "'\n" );

    printf( "  base table length:\t\t%d\n", cth.base_table_length );

    printf( "  version:\t\t\t1.%1d\n", cth.spec_rev );
    printf( "  checksum:\t\t\t0x%02x\n", cth.checksum );

    printf( "  OEM ID:\t\t\t'" );
    pnstr( cth.oem_id, 8 );
    printf( "'\n" );

    printf( "  Product ID:\t\t\t'" );
    pnstr( cth.product_id, 12 );
    printf( "'\n" );

    printf( "  OEM table pointer:\t\t0x%08x\n", cth.oem_table_pointer );
    printf( "  OEM table size:\t\t%d\n", cth.oem_table_size );

    printf( "  entry count:\t\t\t%d\n", cth.entry_count );

    printf( "  local APIC address:\t\t0x%08x\n", cth.apic_address );

    printf( "  extended table length:\t%d\n", cth.extended_table_length );
    printf( "  extended table checksum:\t%d\n", cth.extended_table_checksum );

    count = cth.entry_count;

    puts( SEP_LINE );

    printf( "MP Config Base Table Entries:\n\n" );

    /* initialze tables */
    for ( x = 0; x < 16; ++x ) {
        busses[ x ] = apics[ x ] = 0xff;
    }

    ncpu = 0;
    nbus = 0;
    napic = 0;
    nintr = 0;

    /* process all the CPUs */
    printf( "--\nProcessors:\tAPIC ID\tVersion\tState"
            "\t\tFamily\tModel\tStep\tFlags\n" );
    c = count;
    while (c) {
        if ( readType() != 0 )
            break;
        processorEntry();
        --c;
    }

    /* process all the buses */
    printf( "--\nBus:\t\tBus ID\tType\n" );
    while (c) {
        if ( readType() != 1 )
            break;
        busEntry();
        --c;
    }

    /* process all the apics */
    printf( "--\nI/O APICs:\tAPIC ID\tVersion\tState\t\tAddress\n" );
    while (c) {
        if ( readType() != 2 )
            break;
        ioApicEntry();
        --c;
    }

    /* process all the I/O Ints */
    printf( "--\nI/O Ints:\tType\tPolarity    Trigger\tBus ID\t IRQ\tAPIC ID\tINT#\n" );
    while (c) {
        if ( readType() != 3 )
            break;
        intEntry();
        --c;
    }

    /* process all the Local Ints */
    printf( "--\nLocal Ints:\tType\tPolarity    Trigger\tBus ID\t IRQ\tAPIC ID\tINT#\n" );
    while (c) {
        if ( readType() != 4 )
            break;
        intEntry();
        --c;
    }

    /* If table is correct current position is on cth.base_table_length offset
       after the header's start. It will be good to check it */

#if defined( EXTENDED_PROCESSING_READY )
    /* process any extended data */
    if ( cth.extended_table_length ) {
        int totalSize = cth.extended_table_length;

        puts( SEP_LINE );

        printf( "MP Config Extended Table Entries:\n\n" );

        while ( totalSize > 0 ) {
            switch ( type = readType() ) {
            case 0x80:
                sasEntry();
                break;
            case 0x81:
                bhdEntry();
                break;
            case 0x82:
                cbasmEntry();
                break;
            default:
                printf( "Extended Table record type is 0x%x and it is unknown\n", type );
                printf( "Extended Table HOSED!\n" );
                exit( 1 );
            }

            totalSize -= extendedtableEntryTypes[ type-0x80 ].length;
        }
    }
#endif  /* EXTENDED_PROCESSING_READY */

    /* process any OEM data */
    if ( cth.oem_table_pointer && (cth.oem_table_size > 0) ) {
#if defined( OEM_PROCESSING_READY )
# error your on your own here!
        /* convert OEM table pointer to virtual address */
        poemtp = (vm_offset_t)cth.oem_table_pointer;

        /* read in oem table structure */
        if ( (oemdata = (void*)malloc( cth.oem_table_size )) == NULL ) {
            perror( "oem malloc" );
            exit( 1 );
        }

        seekEntry( poemtp );
        readEntry( oemdata, cth.oem_table_size );

        /** process it */

        free( oemdata );
#else
        printf( "\nyou need to modify the source to handle OEM data!\n\n" );
#endif  /* OEM_PROCESSING_READY */
    }

    fflush( stdout );

#if defined( RAW_DUMP )
{
    int         ofd;
    u_char      dumpbuf[ 4096 ];

    ofd = open( "/tmp/mpdump", O_CREAT | O_RDWR );
    seekEntry( paddr );
    readEntry( dumpbuf, 1024 );
    write( ofd, dumpbuf, 1024 );
    close( ofd );
}
#endif /* RAW_DUMP */
}


/*
 * 
 */
static int
readType( void )
{
    u_char      type;

    if ( memRead( hPhysMem, &type, sizeof( u_char ) ) != sizeof( u_char ) ) {
        perror( "type read" );
        fflush( stderr );
        exit( 1 );
    }

    if ( memSeek( hPhysMem, -1, SEEK_CUR ) < 0 ) {
        perror( "type seek" );
        exit( 1 );
    }

    return (int)type;
}


/*
 * 
 */
static void
seekEntry( vm_offset_t addr )
{
    if ( memSeek( hPhysMem, (off_t)addr, SEEK_SET ) < 0 ) {
        perror( "/dev/mem seek" );
        exit( 1 );
    }
}


/*
 * 
 */
static void
readEntry( void* entry, int size )
{
    if ( memRead( hPhysMem, entry, size ) != size ) {
        perror( "readEntry" );
        exit( 1 );
    }
}


static void
processorEntry( void )
{
    ProcEntry   entry;

    /* read it into local memory */
    readEntry( &entry, sizeof( entry ) );

    /* count it */
    ++ncpu;

    printf( "\t\t%2d", entry.apicID );
    printf( "\t 0x%2x", entry.apicVersion );

    printf( "\t %s, %s",
            (entry.cpuFlags & PROCENTRY_FLAG_BP) ? "BSP" : "AP",
            (entry.cpuFlags & PROCENTRY_FLAG_EN) ? "usable" : "unusable" );

    printf( "\t %d\t %d\t %d",
            (entry.cpuSignature >> 8) & 0x0f,
            (entry.cpuSignature >> 4) & 0x0f,
            entry.cpuSignature & 0x0f );

    printf( "\t 0x%04x\n", entry.featureFlags );
}


/*
 * 
 */
static int
lookupBusType( char* name )
{
    int x;

    for ( x = 0; x < MAX_BUSTYPE; ++x )
        if ( strcmp( busTypeTable[ x ].name, name ) == 0 )
            return busTypeTable[ x ].type;

    return UNKNOWN_BUSTYPE;
}


static void
busEntry( void )
{
    int         x;
    char        name[ 8 ];
    char        c;
    BusEntry    entry;

    /* read it into local memory */
    readEntry( &entry, sizeof( entry ) );

    /* count it */
    ++nbus;

    printf( "\t\t%2d", entry.busID );
    printf( "\t " ); pnstr( entry.busType, 6 ); printf( "\n" );

    for ( x = 0; x < 6; ++x ) {
        if ( (c = entry.busType[ x ]) == ' ' )
            break;
        name[ x ] = c;
    }
    name[ x ] = '\0';
    busses[ entry.busID ] = lookupBusType( name );
}


static void
ioApicEntry( void )
{
    IOApicEntry entry;

    /* read it into local memory */
    readEntry( &entry, sizeof( entry ) );

    /* count it */
    ++napic;

    printf( "\t\t%2d", entry.apicID );
    printf( "\t 0x%02x", entry.apicVersion );
    printf( "\t %s",
            (entry.apicFlags & IOAPICENTRY_FLAG_EN) ? "usable" : "unusable" );
    printf( "\t\t 0x%x\n", entry.apicAddress );

    apics[ entry.apicID ] = entry.apicID;
}


char* intTypes[] = {
    "INT", "NMI", "SMI", "ExtINT"
};

char* polarityMode[] = {
    "conforms", "active-hi", "reserved", "active-lo"
};
char* triggerMode[] = {
    "conforms", "edge", "reserved", "level"
};

static void
intEntry( void )
{
    IntEntry    entry;

    /* read it into local memory */
    readEntry( &entry, sizeof( entry ) );

    /* count it */
    if ( (int)entry.type == 3 )
        ++nintr;

    printf( "\t\t%s", intTypes[ (int)entry.intType ] );

    printf( "\t%9s", polarityMode[ (int)entry.intFlags & 0x03 ] );
    printf( "%12s", triggerMode[ ((int)entry.intFlags >> 2) & 0x03 ] );

    printf( "\t %5d", (int)entry.srcBusID );
    if ( busses[ (int)entry.srcBusID ] == PCI )
        printf( "\t%2d:%c", 
                ((int)entry.srcBusIRQ >> 2) & 0x1f,
                ((int)entry.srcBusIRQ & 0x03) + 'A' );
    else
        printf( "\t %3d", (int)entry.srcBusIRQ );
    printf( "\t %6d", (int)entry.dstApicID );
    printf( "\t %3d\n", (int)entry.dstApicINT );
}


static void
sasEntry( void )
{
    SasEntry    entry;

    /* read it into local memory */
    readEntry( &entry, sizeof( entry ) );

    printf( "--\n%s\n", extendedtableEntryTypes[ entry.type ].name );
    printf( " bus ID: %d", entry.busID );
    printf( " address type: " );
    switch ( entry.addressType ) {
    case 0:
        printf( "I/O address\n" );
        break;
    case 1:
        printf( "memory address\n" );
        break;
    case 2:
        printf( "prefetch address\n" );
        break;
    default:
        printf( "UNKNOWN type\n" );
        break;
    }

    printf( " address base: 0x%llx\n", entry.addressBase );
    printf( " address range: 0x%llx\n", entry.addressLength );
}


static void
bhdEntry( void )
{
    BhdEntry    entry;

    /* read it into local memory */
    readEntry( &entry, sizeof( entry ) );

    printf( "--\n%s\n", extendedtableEntryTypes[ entry.type ].name );
    printf( " bus ID: %d", entry.busID );
    printf( " bus info: 0x%02x", entry.busInfo );
    printf( " parent bus ID: %d", entry.busParent );
}


static void
cbasmEntry( void )
{
    CbasmEntry  entry;

    /* read it into local memory */
    readEntry( &entry, sizeof( entry ) );

    printf( "--\n%s\n", extendedtableEntryTypes[ entry.type ].name );
    printf( " bus ID: %d", entry.busID );
    printf( " address modifier: %s\n", (entry.addressMod & 0x01) ?
                                        "subtract" : "add" );
    printf( " predefined range: 0x%08x", entry.predefinedRange );
}


/*
 * do a dmesg output
 */
static void
doDmesg( void )
{
    puts( SEP_LINE );

    printf( "dmesg output:\n\n" );
    fflush( stdout );
    system( "dmesg" );
}


/*
 *  build "options" entries for the kernel config file
 */
static void
doOptionList( void )
{
    puts( SEP_LINE );

    printf( "# SMP kernel config file options:\n\n" );
    printf( "\n# Required:\n" );
    printf( "options            SMP\t\t\t# Symmetric MultiProcessor Kernel\n" );
    printf( "options            APIC_IO\t\t\t# Symmetric (APIC) I/O\n" );

    printf( "\n# Useful:\n" );
    printf( "#options           SMP_AUTOSTART\t\t# start the additional CPUs during boot\n" );

    printf( "\n# Optional (built-in defaults will work in most cases):\n" );
    printf( "#options           NCPU=%d\t\t\t# number of CPUs\n", ncpu );
    printf( "#options           NBUS=%d\t\t\t# number of busses\n", nbus );
    printf( "#options           NAPIC=%d\t\t\t# number of IO APICs\n", napic );
    printf( "#options           NINTR=%d\t\t# number of INTs\n",
                (nintr < 24) ? 24 : nintr );

    printf( "\n# Rogue hardware:\n" );
    printf( "#\n#  Tyan Tomcat II:\n" );
    printf( "#options           SMP_TIMER_NC\t\t# \n" );
    printf( "#\n#  SuperMicro P6DNE:\n" );
    printf( "#options           SMP_TIMER_NC\t\t# \n" );
}


/*
 * 
 */
static void
pnstr( char* s, int c )
{
    char string[ MAXPNSTR + 1 ];

    if ( c > MAXPNSTR )
        c = MAXPNSTR;
    strncpy( string, s, c );
    string[ c ] = '\0';
    printf( "%s", string );
}
