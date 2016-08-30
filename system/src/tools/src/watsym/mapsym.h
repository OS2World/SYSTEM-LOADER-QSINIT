#ifndef MS_SYMFILE
#define MS_SYMFILE

#include "sp_defs.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SYM_PTR_TO_OFS( a )     ( a * 16 )

#define SYM_VERSION_MAJOR       5
#define SYM_VERSION_MINOR       1

#pragma pack(1)

typedef struct {
    u16            next_ptr;           // pointer to next header
    u8             abs_type;           // type of abs symbols (16/32bit)
    u8                  pad;           // pad byte
    u16           entry_seg;           // segment of entry point
    u16       abs_sym_count;           // number of abs symbols
    u16         abs_tab_ofs;           // offset of abs sym table from mapdef
    u16            num_segs;           // number of segments in map
    u16             seg_ptr;           // pointer to first segment in chain
    u8          max_sym_len;           // maximum length of symbol name
    u8             name_len;           // length of map name
    char            name[1];           // map name
} sym_header;

#define SYM_HEADER_FIXSIZE      offsetof(sym_header, name_len)

typedef struct {
    u16               zero;            // next map, must be 0
    u8           minor_ver;            // minor version number
    u8           major_ver;            // major version number
} sym_endmap;

typedef struct {
    u16           next_ptr;            // next segdef, 0 if last; may be circular
    u16           num_syms;            // number of symbols in segment
    u16        sym_tab_ofs;            // offset of symbol table from segdef
    u16          load_addr;            // segment load address
    u16             phys_0;            // physical address 0
    u16             phys_1;            // physical address 1
    u16             phys_2;            // physical address 2
    u8            sym_type;            // type of symbols (16/32bit)
    u8                 pad;            // pad byte
    u16         linnum_ptr;            // pointer to line numbers
    u8           is_loaded;            // segment loaded flag
    u8           curr_inst;            // current instance
    u8            name_len;            // length of symbol name
    char           name[1];            // segment name
} sym_segdef;

#define SYM_SEGDEF_FIXSIZE      offsetof(sym_segdef, name_len)

/* NB: 32-bit segments do not imply 32-bit symbol records; MAPSYM will emit
   16-bit symbols as long as all offsets in a segment fit within 16 bits. */

typedef struct {
    u16             offset;            // offset of symbol within segment
    u8            name_len;            // length of symbol name
    char           name[1];            // symbol name
} sym_symdef;

#define SYM_SYMDEF_FIXSIZE      offsetof(sym_symdef, name_len)

typedef struct {
    u32             offset;            // offset of symbol within segment
    u8            name_len;            // length of symbol name
    char           name[1];            // symbol name
} sym_symdef_32;

#define SYM_SYMDEF_32_FIXSIZE   offsetof(sym_symdef_32, name_len)

typedef struct {
    u16           next_ptr;            // next linedef, 0 if last
    u16            seg_ptr;            // pointer to corresponding segdef
    u16          lines_ofs;            // offset of line table from linedef
    u16               file;            // external file handle
    u16          lines_num;            // number of linerecs
    u8            name_len;            // symbol name length
    char           name[1];            // symbol name
} sym_linedef;

#define SYM_LINEDEF_FIXSIZE     offsetof(sym_linedef, name_len)

typedef struct {
    u16        code_offset;            // offset within segment
    u32        line_offset;            // offset within file; may be -1
} sym_linerec;

enum {
    SYM_FLAG_32BIT = 1,                 // symbols are 32-bit
    SYM_FLAG_ALPHA = 2                  // alphasorted symbol table included
};

#pragma pack()
#ifdef __cplusplus
}
#endif
#endif // MS_SYMFILE
