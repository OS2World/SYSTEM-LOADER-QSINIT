//
// QSINIT "vmtrr" module
// mtrr optimization code (header)
//
#ifndef QSINIT_MTRR_AUTOOPT
#define QSINIT_MTRR_AUTOOPT

#include "qstypes.h"
#include "qshm.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OPTERR_VIDMEM3GB    0x00001  ///< video memory is not in 3..4Gb location
#define OPTERR_UNKCT        0x00002  ///< one of WT/WP/WC cache types present
#define OPTERR_INTERSECT    0x00003  ///< requested block intersects with others
#define OPTERR_SPLIT4GB     0x00004  ///< error on splitting block on 4Gb border
#define OPTERR_BELOWUC      0x00005  ///< address is below selected UC border
#define OPTERR_LOWUC        0x00006  ///< too low large UC block
#define OPTERR_NOREG        0x00007  ///< no free reg
#define OPTERR_OPTERR       0x00008  ///< optimization error

int mtrropt(u64t wc_addr, u64t wc_len, u32t *memlimit, int defWB);

typedef struct {
   u64t    start;
   u64t      len;
   int     cache;
   int        on;
} mtrrentry;

extern mtrrentry *mtrr;
extern int        regs;

#ifdef __cplusplus
}
#endif
#endif // QSINIT_MTRR_AUTOOPT
