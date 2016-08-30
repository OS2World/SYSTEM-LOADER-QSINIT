//
// QSINIT "start" module
// some internal functions (not exported)
//
#ifndef QSINIT_MEMINT
#define QSINIT_MEMINT

#ifdef __cplusplus
extern "C" {
#endif

#define PHMAP_FORCE  0x0001    ///< pag_physmap() unoff: force mapping of low address

#define MEM_DIRECT   0x0002    ///< memory block is used by non-paged QSINIT
#define MEM_PHYSEND  0x0004    ///< last block of pc memory
#define MEM_MAPPED   0x0008    ///< mapped physical memory entry
#define MEM_READONLY 0x0010    ///< read-only page

typedef struct {
   u32t      start;    ///< linear start
   u32t        len;    ///< length
   u32t      flags;    ///< MEM_* flags
   u32t      usage;    ///< usage counter
   u64t      paddr;    ///< start of mapped physical memory
   u32t     res[2];
} vmem_entry;

/** internal page map call
    @param  virt    page aligned address
    @param  len     page aligned length
    @param  phys    page aligned physical address
    @param  flags   flags (MEM_DIRECT & MEM_READONLY only)
    @return success flag (1/0) */
int  pag_mappages(u32t virt, u32t len, u64t phys, u32t flags);
int  pag_unmappages(u32t virt, u32t len);

void pag_inittables(void);

/// lock meminit/pager mutex
void pag_lock(void);
/// lock meminit/unpager mutex
void pag_unlock(void);

#ifdef __cplusplus
}

class PAGEMUX_THIS_FUNC {
public:
   PAGEMUX_THIS_FUNC() { pag_lock(); }
   ~PAGEMUX_THIS_FUNC() { pag_unlock(); }
};
#endif
#endif // QSINIT_MEMINT
