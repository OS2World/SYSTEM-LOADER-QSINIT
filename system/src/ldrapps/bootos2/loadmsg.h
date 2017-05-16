//
// QSINIT "bootos2" module
// os2ldr.msg handling
//
#ifndef OS2MSG_REF
#define OS2MSG_REF

#include "qstypes.h"
#pragma pack(1)

#define MSG_POOL_LIMIT  4096      ///< limit every type of messages to 4k
#define MSGF_SIZE       0x40      ///< # of messages, should be in OS2LDR.MSG

typedef struct MsgEntry {
    u16t              MsgNo;      // Message number
    char              *pMsg;      // pointer to message
} msg_entry;

// message file header
typedef struct file_head {
    char            sign[8];      // signature mark
    char         comp_id[3];      // component id
    u16t           msgcount;      // number of messages
    u16t           base_mid;      // base message number
    char        offset_type;      // double(0) or single word (1)
    u16t            version;
    u16t          headerlen;
    char             cptype;      // SBCS code page (0) or DBCS (1)
    u16t              cpnum;      // code page number
    char        reserved[8];
} msg_header;

/** form message data for DOSHLP.
    @param   rpool    preallocated 4k buffer for resident messages
    @param   rsize    size of resident messages pool
    @param   dpool    preallocated 4k buffer for discardable messages
    @param   dsize    size of discardable messages pool 
    @return success flag (1/0) */
int create_msg_pools(char *rpool, int *rsize, char *dpool, int *dsize);

#pragma pack()
#endif // OS2MSG_REF
