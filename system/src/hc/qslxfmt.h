//
// QSINIT
// LX/LE format definitions
// warning! this file is not equal to exe386.h!
//
#ifndef QS_LXLE_FMT
#define QS_LXLE_FMT

#include "qstypes.h"

#define EMAGIC           0x5A4D
#define E32MAGIC         0x584C
#define LXMAGIC          E32MAGIC
#define LEMAGIC          0x454C
#define E32RESBYTES1     0
#define E32RESBYTES2     0
#define E32RESBYTES3     20
#define E32CPU286        0x001
#define E32CPU386        0x002
#define E32CPU486        0x003
#define E32MODNAME       0x7F

#define E32_MAGIC(x)        (x).e32_magic
#define E32_BORDER(x)       (x).e32_border
#define E32_WORDER(x)       (x).e32_worder
#define E32_LEVEL(x)        (x).e32_level
#define E32_CPU(x)          (x).e32_cpu
#define E32_OS(x)           (x).e32_os
#define E32_VER(x)          (x).e32_ver
#define E32_MFLAGS(x)       (x).e32_mflags
#define E32_MPAGES(x)       (x).e32_mpages
#define E32_STARTOBJ(x)     (x).e32_startobj
#define E32_EIP(x)          (x).e32_eip
#define E32_STACKOBJ(x)     (x).e32_stackobj
#define E32_ESP(x)          (x).e32_esp
#define E32_PAGESIZE(x)     (x).e32_pagesize
#define E32_PAGESHIFT(x)    (x).e32_pageshift
#define E32_FIXUPSIZE(x)    (x).e32_fixupsize
#define E32_FIXUPSUM(x)     (x).e32_fixupsum
#define E32_LDRSIZE(x)      (x).e32_ldrsize
#define E32_LDRSUM(x)       (x).e32_ldrsum
#define E32_OBJTAB(x)       (x).e32_objtab
#define E32_OBJCNT(x)       (x).e32_objcnt
#define E32_OBJMAP(x)       (x).e32_objmap
#define E32_ITERMAP(x)      (x).e32_itermap
#define E32_RSRCTAB(x)      (x).e32_rsrctab
#define E32_RSRCCNT(x)      (x).e32_rsrccnt
#define E32_RESTAB(x)       (x).e32_restab
#define E32_ENTTAB(x)       (x).e32_enttab
#define E32_DIRTAB(x)       (x).e32_dirtab
#define E32_DIRCNT(x)       (x).e32_dircnt
#define E32_FPAGETAB(x)     (x).e32_fpagetab
#define E32_FRECTAB(x)      (x).e32_frectab
#define E32_IMPMOD(x)       (x).e32_impmod
#define E32_IMPMODCNT(x)    (x).e32_impmodcnt
#define E32_IMPPROC(x)      (x).e32_impproc
#define E32_PAGESUM(x)      (x).e32_pagesum
#define E32_DATAPAGE(x)     (x).e32_datapage
#define E32_PRELOAD(x)      (x).e32_preload
#define E32_NRESTAB(x)      (x).e32_nrestab
#define E32_CBNRESTAB(x)    (x).e32_cbnrestab
#define E32_NRESSUM(x)      (x).e32_nressum
#define E32_AUTODATA(x)     (x).e32_autodata
#define E32_DEBUGINFO(x)    (x).e32_debuginfo
#define E32_DEBUGLEN(x)     (x).e32_debuglen
#define E32_INSTPRELOAD(x)  (x).e32_instpreload
#define E32_INSTDEMAND(x)   (x).e32_instdemand
#define E32_HEAPSIZE(x)     (x).e32_heapsize
#define E32_STACKSIZE(x)    (x).e32_stacksize

#define E32NOTP         0x00008000
#define E32NOLOAD       0x00002000
#define E32PMAPI        0x00000300
#define E32PMW          0x00000200
#define E32NOPMW        0x00000100
#define E32NOEXTFIX     0x00000020
#define E32NOINTFIX     0x00000010
#define E32SYSDLL       0x00000008
#define E32LIBINIT      0x00000004
#define E32LIBTERM      0x40000000
#define E32APPMASK      0x00000300

#define E32PROTDLL      0x00010000
#define E32DEVICE       0x00020000
#define E32MODEXE       0x00000000
#define E32MODDLL       0x00008000
#define E32MODPROTDLL   0x00018000
#define E32MODPDEV      0x00020000
#define E32MODVDEV      0x00028000
#define E32MODMASK      0x00038000
#define E32NOTMPSAFE    0x00080000

#define RINTSIZE16      8
#define RINTSIZE32      10
#define RORDSIZE        8
#define RNAMSIZE16      8
#define RNAMSIZE32      10
#define RADDSIZE16      10
#define RADDSIZE32      12

#define R32_SOFF(x)      (x).r32_soff
#define R32_OBJNO(x)     (x).r32_objmod
#define R32_MODORD(x)    (x).r32_objmod
#define R32_OFFSET16(x)  (x).r32_target.intref.offset16
#define R32_OFFSET32(x)  (x).r32_target.intref.offset32
#define R32_PROCOFF16(x) (x).r32_target.extref.proc.offset16
#define R32_PROCOFF32(x) (x).r32_target.extref.proc.offset32
#define R32_PROCORD(x)   (x).r32_target.extref.ord
#define R32_ENTRY(x)     (x).r32_target.addfix.entry
#define R32_ADDVAL16(x)  (x).r32_target.addfix.addval.offset16
#define R32_ADDVAL32(x)  (x).r32_target.addfix.addval.offset32
#define R32_SRCCNT(x)    (x).r32_srccount
#define R32_CHAIN(x)     (x).r32_chain

#define NRSTYP          0x0F            // Source type mask
#define NRSBYT          0x00            // lo byte (8-bits)
#define NRSSEG          0x02            // 16-bit segment (16-bits)
#define NRSPTR          0x03            // 16:16 pointer (32-bits)
#define NRSOFF          0x05            // 16-bit offset (16-bits)
#define NRPTR48         0x06            // 16:32 pointer (48-bits)
#define NROFF32         0x07            // 32-bit offset (32-bits)
#define NRSOFF32        0x08            // 32-bit self-relative offset (32-bits)
#define NRALIAS         0x10
#define NRCHAIN         0x20

#define NRRTYP          0x03
#define NRRINT          0x00
#define NRRORD          0x01
#define NRRNAM          0x02
#define NRRENT          0x03
#define NRADD           0x04
#define NRICHAIN        0x08

#define NR32BITOFF      0x10
#define NR32BITADD      0x20
#define NR16OBJMOD      0x40
#define NR8BITORD       0x80

#define PAGEPERDIR      62
#define LG2DIR          7

#define O32_SIZE(x)     (x).o32_size
#define O32_BASE(x)     (x).o32_base
#define O32_FLAGS(x)    (x).o32_flags
#define O32_PAGEMAP(x)  (x).o32_pagemap
#define O32_MAPSIZE(x)  (x).o32_mapsize
#define O32_RESERVED(x) (x).o32_reserved

#define OBJREAD         0x00000001
#define OBJWRITE        0x00000002
#define OBJEXEC         0x00000004
#define OBJRSRC         0x00000008
#define OBJDISCARD      0x00000010
#define OBJSHARED       0x00000020
#define OBJPRELOAD      0x00000040
#define OBJINVALID      0x00000080
#define OBJNONPERM      0x00000000
#define OBJPERM         0x00000100
#define OBJRESIDENT     0x00000200
#define OBJCONTIG       0x00000300
#define OBJDYNAMIC      0x00000400
#define LNKNONPERM      0x00000600
#define OBJTYPEMASK     0x00000700
#define OBJALIAS16      0x00001000
#define OBJBIGDEF       0x00002000
#define OBJCONFORM      0x00004000
#define OBJIOPL         0x00008000

#define LXPAGEIDX(x)    ((x).o32_pagedataoffset)
#define LXPAGESIZE(x)   ((x).o32_pagesize)
#define LXPAGEFLAGS(x)  ((x).o32_pageflags)

#define LEPAGEIDX(x)    ((u32t)(x).pagehigh2<<16|(u32t)(x).pagehigh1<<8|(x).pagelow)
#define LEPAGESIZE(x)   (PAGESIZE)
#define LEPAGEFLAGS(x)  ((x).pageflags)

// LX page table flags
#define LX_VALID        0x00000000
#define LX_ITERDATA     0x00000001
#define LX_INVALID      0x00000002
#define LX_ZEROED       0x00000003
#define LX_RANGE        0x00000004
#define LX_ITERDATA2    0x00000005
#define LX_ITERDATA3    0x00000008

// LE page table flags
#define LE_VALID        0x00000000
#define LE_LASTINFILE   0x00000003
#define LE_ITERDATA     0x00000040
#define LE_INVALID      0x00000080
#define LE_ZEROED       0x000000C0

#define B32_CNT(x)      (x).b32_cnt
#define B32_TYPE(x)     (x).b32_type
#define B32_OBJ(x)      (x).b32_obj

#define E32_EFLAGS(x)   (x).e32_flags
#define E32_OFFSET16(x) (x).e32_variant.e32_offset.offset16
#define E32_OFFSET32(x) (x).e32_variant.e32_offset.offset32
#define E32_GATEOFF(x)  (x).e32_variant.e32_callgate.offset
#define E32_GATE(x)     (x).e32_variant.e32_callgate.callgate
#define E32_MODORD(x)   (x).e32_variant.e32_fwd.modord
#define E32_VALUE(x)    (x).e32_variant.e32_fwd.value

#define FIXENT16        3
#define FIXENT32        5
#define GATEENT16       5
#define FWDENT          7
                        
#define EMPTY           0x00
#define ENTRY16         0x01
#define GATE16          0x02
#define ENTRY32         0x03
#define ENTRYFWD        0x04
#define TYPEINFO        0x80
                        
#define E32EXPORT       0x01
#define E32SHARED       0x02
#define E32PARAMS       0xF8

#define FWD_ORDINAL     0x01

#define ERES1WDS    0x0004
#define ERES2WDS    0x000A

#pragma pack(1)

struct exe_hdr {
    u16t   e_magic;
    u16t   e_cblp;
    u16t   e_cp;
    u16t   e_crlc;
    u16t   e_cparhdr;
    u16t   e_minalloc;
    u16t   e_maxalloc;
    u16t   e_ss;
    u16t   e_sp;
    u16t   e_csum;
    u16t   e_ip;
    u16t   e_cs;
    u16t   e_lfarlc;
    u16t   e_ovno;
    u16t   e_res[ERES1WDS];
    u16t   e_oemid;
    u16t   e_oeminfo;
    u16t   e_res2[ERES2WDS];
    u32t   e_lfanew;
};

typedef struct exe_hdr mz_exe_t;

struct e32_exe {
    u16t   e32_magic;
    u8t    e32_border;
    u8t    e32_worder;
    u32t   e32_level;
    u16t   e32_cpu;
    u16t   e32_os;
    u32t   e32_ver;
    u32t   e32_mflags;
    u32t   e32_mpages;
    u32t   e32_startobj;
    u32t   e32_eip;
    u32t   e32_stackobj;
    u32t   e32_esp;
    u32t   e32_pagesize;
    u32t   e32_pageshift;
    u32t   e32_fixupsize;
    u32t   e32_fixupsum;
    u32t   e32_ldrsize;
    u32t   e32_ldrsum;
    u32t   e32_objtab;
    u32t   e32_objcnt;
    u32t   e32_objmap;
    u32t   e32_itermap;
    u32t   e32_rsrctab;
    u32t   e32_rsrccnt;
    u32t   e32_restab;
    u32t   e32_enttab;
    u32t   e32_dirtab;
    u32t   e32_dircnt;
    u32t   e32_fpagetab;
    u32t   e32_frectab;
    u32t   e32_impmod;
    u32t   e32_impmodcnt;
    u32t   e32_impproc;
    u32t   e32_pagesum;
    u32t   e32_datapage;
    u32t   e32_preload;
    u32t   e32_nrestab;
    u32t   e32_cbnrestab;
    u32t   e32_nressum;
    u32t   e32_autodata;
    u32t   e32_debuginfo;
    u32t   e32_debuglen;
    u32t   e32_instpreload;
    u32t   e32_instdemand;
    u32t   e32_heapsize;
    u32t   e32_stacksize;
    u8t    e32_res3[E32RESBYTES3];
};

typedef struct e32_exe lx_exe_t;

typedef union _offset {
    u16t   offset16;
    u32t   offset32;
} offset;


struct r32_rlc {
    u8t   nr_stype;
    u8t   nr_flags;
    s16t  r32_soff;
    u16t  r32_objmod;
    union targetid {
        offset   intref;
        union extfixup {
            offset  proc;
            u32t    ord;
        } extref;
        struct addfixup {
            u16t    entry;
            offset  addval;
        } addfix;
    } r32_target;
    u16t  r32_srccount;
    u16t  r32_chain;
};

typedef struct r32_rlc lx_rlc_t;

typedef struct _OBJPAGEDIR {
    u32t   next;
    u16t   ht[PAGEPERDIR];
} OBJPAGEDIR;

struct o32_obj {
    u32t   o32_size;
    u32t   o32_base;
    u32t   o32_flags;
    u32t   o32_pagemap;
    u32t   o32_mapsize;
    u32t   o32_reserved;
};

typedef struct o32_obj lx_obj_t;

struct o32_map {
    u32t   o32_pagedataoffset;
    u16t   o32_pagesize;
    u16t   o32_pageflags;
};

typedef struct o32_map lx_map_t;

struct o32_lemap {
    u8t    pagehigh1;
    u8t    pagehigh2;
    u8t    pagelow;
    u8t    pageflags;
};

typedef struct o32_lemap le_map_t;

struct rsrc32 {
    u16t   type;
    u16t   name;
    u32t   cb;
    u16t   obj;
    u32t   offset;
};

typedef struct rsrc32 lx_rc32_t;

struct LX_Iter {
    u16t   LX_nIter;
    u16t   LX_nBytes;
    u8t    LX_Iterdata;
};

struct b32_bundle {
    u8t    b32_cnt;
    u8t    b32_type;
    u16t   b32_obj;
};

typedef struct b32_bundle lx_exp_b;

struct e32_entry {
    u8t    e32_flags;
    union entrykind {
        offset   e32_offset;
        struct callgate {
            u16t   offset;
            u16t   callgate;
        } e32_callgate;
        struct fwd {
            u16t   modord;
            u32t   value;
        } e32_fwd;
    } e32_variant;
};

typedef struct e32_entry lx_entry_t;

#pragma pack()

#endif // QS_LXLE_FMT
