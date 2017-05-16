#include "loadmsg.h"
#include "stdlib.h"
#include "qsutil.h"
#include "qslog.h"
#include "kload.h"

#define DISCARDABLE_MSG      0
#define RESIDENT_MSG         1
#define SKIP_MSG             2  // just skip this message

typedef struct {
   u16t            msg_no;      // Message number
   u8t             target;      // resident?
} msgf_entry;

// message file contents
static msgf_entry mfc[MSGF_SIZE] = {
   { 169,1 }, // MSG_SWAPIN_ATTEMPT_FAILED
   { 171,1 }, // MSG_INT_TOO_LONG
   { 732,0 }, // MSG_BOOT_DRIVE_NOT_ACCESSIBLE
   {1004,0 }, // MSG_STAND_BY
   {1120,2 }, // MSG_NEXT_DISKETTE
   {1178,0 }, // MSG_REPLACEMENT_STRING
   {1188,0 }, // MSG_SYSINIT_PROCS_INITED
   {1190,0 }, // MSG_SYSINIT_PSD_NOT_FND
   {1191,0 }, // MSG_SYSINIT_NO_IFS_STMT
   {1196,0 }, // MSG_SYSINIT_INVAL_PARM
   {1199,1 }, // MSG_SYSINIT_BOOT_FAILURE
   {1201,0 }, // MSG_SYSINIT_UDRVR_FAIL
   {1202,0 }, // MSG_SYSINIT_SDRVR_FAIL
   {1205,0 }, // MSG_SYSINIT_DOS_MODIFIED
   {1206,0 }, // MSG_SYSINIT_UFILE_NO_MEM
   {1503,1 }, // MSG_SWAP_FILE_FULL
   {1518,1 }, // MSG_SYSINIT_SFILE_NOT_FND
   {1519,0 }, // MSG_SYSINIT_SFILE_NO_MEM
   {1709,0 }, // MSG_SYSINIT_START_NO_LOAD
   {1710,0 }, // MSG_SYSINIT_START_NO_INIT
   {1718,0 }, // MSG_SYSINIT_UFILE_NOT_FND
   {1719,0 }, // MSG_SYSINIT_UDRVR_INVAL
   {1720,0 }, // MSG_SYSINIT_FSD_NOT_VAL
   {1721,0 }, // MSG_SYSINIT_BANNER
   {1722,0 }, // MSG_SYSINIT_CANT_LOAD_MOD
   {1723,0 }, // MSG_SYSINIT_EPT_MISSING
   {1724,0 }, // MSG_SYSINIT_CANT_OPEN_CON
   {1725,0 }, // MSG_SYSINIT_WRONG_HANDLE
   {1726,1 }, // MSG_SYSINIT_PRESS_ENTER
   {1733,0 }, // MSG_SYSINIT_SCFILE_INVAL
   {1758,0 }, // MSG_SYSINIT_AUTO_ALTF1
   {1810,1 }, // MSG_CMD_DIV_0
   {1915,1 }, // MSG_INTERNAL_ERROR
   {1919,0 }, // MSG_SYSINIT_UEXEC_FAIL
   {1923,1 }, // MSG_PAGE_FAULT
   {1924,1 }, // MSG_INTERRUPTS_DISABLED
   {1925,0 }, // MSG_SYSINIT_SEXEC_FAIL
   {1926,1 }, // MSG_GEN_PROT_FAULT
   {1928,1 }, // MSG_NOMEM_FOR_RELOAD
   {1944,1 }, // MSG_NMI
   {1945,1 }, // MSG_NMI_EXC1 
   {1946,1 }, // MSG_NM12_EXC2
   {1947,1 }, // MSG_NMI2_EXC3
   {1948,1 }, // MSG_NMI2_EXC4
   {2025,0 }, // MSG_SYSINIT_BOOT_ERROR
   {2027,0 }, // MSG_SYSINIT_INSER_DK
   {2028,0 }, // MSG_SYSINIT_DOS_NOT_FD
   {2029,0 }, // MSG_SYSINIT_DOS_NOT_VAL
   {2030,1 }, // MSG_SYSINIT_MORE_MEM
   {2065,1 }, // MSG_SYSINIT_SYS_STOPPED
   {2067,0 }, // MSG_SYSINIT_SDRVR_INVAL
   {2070,1 }, // MSG_DEMAND_LOAD_FAILED
   {3140,1 }, // MSG_EISA_NMI_SOFTWARE
   {3141,1 }, // MSG_EISA_NMI_BUS
   {3142,1 }, // MSG_EISA_NMI_FAILSAFE
   {3143,1 }, // MSG_EISA_NMI_IOCHK
   {3144,1 }, // MSG_EISA_NMI_PARITY
   {3145,1 }, // MSG_BAD_COMMAND_COM
   {3161,2 }, // MSG_NOT_80386_CPU
   {3162,2 }, // MSG_80_STEPPING
   {3163,2 }, // MSG_32BIT_MULTIPLY
   {3175,1 }, // MSG_XCPT_ACCESS_VIOLATION
   {3357,0 }, // MSG_SYSINIT_CMOS_ERR
   {3358,1 }  // MSG_SYSINIT_BAD_TRAPDUMP_DRV
};

int create_msg_pools(char *rpool, int *rsize, char *dpool, int *dsize) {
   u32t    mf_size, rc = 0;
   u8t    *mf_data = (u8t*)read_file("OS2LDR.MSG", &mf_size, 0);
   msg_header *hdr = (msg_header*)mf_data;

   if (!mf_data) return 0;
   do {
      u16t *mpos = (u16t*)(hdr+1);
      u32t    ii;
      int   rrem = MSG_POOL_LIMIT,
            drem = MSG_POOL_LIMIT;
      if (mf_size<sizeof(msg_header)) break;
      if (hdr->msgcount!=MSGF_SIZE) {
         log_printf("msg: # mismatch (%u)\n", hdr->msgcount);
         break;
      }
      if (hdr->offset_type!=1) {
         log_printf("msg: offset type (%u)\n", (u32t)hdr->offset_type);
         break;
      }
      if (mf_size<sizeof(msg_header)+2*hdr->msgcount) break;

      for (ii=0; ii<hdr->msgcount; ii++) {
         if (mf_size<=mpos[ii]) break;

         if (mfc[ii].target!=SKIP_MSG) {
            int     len = (ii==hdr->msgcount-1?mf_size:mpos[ii+1]) - mpos[ii] - 1,
               storelen = len+4+1;
            char    *ps;
            if (mfc[ii].target==RESIDENT_MSG) {
               if (rrem-storelen<0) {
                  log_printf("res msg %u is lost\n", mfc[ii].msg_no);
                  continue;
               }
               ps    = rpool;
               rpool+= storelen;
               rrem -= storelen;
            } else {
               if (drem-storelen<0) {
                  log_printf("dis msg %u is lost\n", mfc[ii].msg_no);
                  continue;
               }
               ps    = dpool;
               dpool+= storelen;
               drem -= storelen;
            }
            *(u16t*)ps = mfc[ii].msg_no; ps+=2;
            *(u16t*)ps = len; ps+=2;
            memcpy(ps, mf_data+mpos[ii]+1, len);
         }
         // we`re reach this point finally, all is ok
         rc = ii==hdr->msgcount-1;
      }
      if (rc) {
         *rsize = MSG_POOL_LIMIT - rrem;
         *dsize = MSG_POOL_LIMIT - drem;
      }
   } while (0);

   hlp_memfree(mf_data);
   if (!rc) log_printf("broken OS2LDR.MSG!\n");
   return rc;
}
