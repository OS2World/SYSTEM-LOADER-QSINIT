;cc #ifndef QS_BASE_AHCI_INC
;cc #define QS_BASE_AHCI_INC
;
; ATA/AHCI defines

;
; ATA register defines
ATA_DATA                     equ            0    ;/<  (RW) data

ATA_FEATURE                  equ            1    ;/<  (W) feature
    ATA_F_DMA                equ         001h    ;/<  enable DMA
    ATA_F_OVL                equ         002h    ;/<  enable overlap

ATA_COUNT                    equ            2    ;/<  (W) sector count

ATA_SECTOR                   equ            3    ;/<  (RW) sector #
ATA_CYL_LSB                  equ            4    ;/<  (RW) cylinder# LSB
ATA_CYL_MSB                  equ            5    ;/<  (RW) cylinder# MSB
ATA_DRIVE                    equ            6    ;/<  (W) Sector/Drive/Head
    ATA_D_LBA                equ         040h    ;/<  use LBA addressing
    ATA_D_IBM                equ         0A0h    ;/<  512 byte sectors, ECC

ATA_COMMAND                  equ            7    ;/<  (W) command

ATA_ERROR                    equ            8    ;/<  (R) error
    ATA_E_ILI                equ         001h    ;/<  illegal length
    ATA_E_NM                 equ         002h    ;/<  no media
    ATA_E_ABORT              equ         004h    ;/<  command aborted
    ATA_E_MCR                equ         008h    ;/<  media change request
    ATA_E_IDNF               equ         010h    ;/<  ID not found
    ATA_E_MC                 equ         020h    ;/<  media changed
    ATA_E_UNC                equ         040h    ;/<  uncorrectable data
    ATA_E_ICRC               equ         080h    ;/<  UDMA crc error
    ATA_E_ATAPI_SENSE_MASK   equ         0F0h    ;/<  ATAPI sense key mask

ATA_IREASON                  equ            9    ;/<  (R) interrupt reason
    ATA_I_CMD                equ         001h    ;/<  cmd (1) | data (0)
    ATA_I_IN                 equ         002h    ;/<  read (1) | write (0)
    ATA_I_RELEASE            equ         004h    ;/<  released bus (1)
    ATA_I_TAGMASK            equ         0F8h    ;/<  tag mask

ATA_STATUS                   equ           10    ;/<  (R) status
ATA_ALTSTAT                  equ           11    ;/<  (R) alternate status
    ATA_S_ERROR              equ          01h    ;/<  error
    ATA_S_INDEX              equ          02h    ;/<  index
    ATA_S_CORR               equ          04h    ;/<  data corrected
    ATA_S_DRQ                equ          08h    ;/<  data request
    ATA_S_DSC                equ          10h    ;/<  drive seek completed
    ATA_S_SERVICE            equ          10h    ;/<  drive needs service
    ATA_S_DWF                equ          20h    ;/<  drive write fault
    ATA_S_DMA                equ          20h    ;/<  DMA ready
    ATA_S_READY              equ          40h    ;/<  drive ready
    ATA_S_BUSY               equ          80h    ;/<  busy

ATA_CONTROL                  equ           12    ;/<  (W) control
    ATA_A_IDS                equ          02h    ;/<  disable interrupts
    ATA_A_RESET              equ          04h    ;/<  RESET controller
    ATA_A_4BIT               equ          08h    ;/<  4 head bits
    ATA_A_HOB                equ          80h    ;/<  High Order Byte enable

;/<  SATA register defines
ATA_SSTATUS                  equ           13
    ATA_SS_DET_MASK          equ    0000000Fh
    ATA_SS_DET_NO_DEVICE     equ    00000000h
    ATA_SS_DET_DEV_PRESENT   equ    00000001h
    ATA_SS_DET_PHY_ONLINE    equ    00000003h
    ATA_SS_DET_PHY_OFFLINE   equ    00000004h

    ATA_SS_SPD_MASK          equ    000000F0h
    ATA_SS_SPD_NO_SPEED      equ    00000000h
    ATA_SS_SPD_GEN1          equ    00000010h
    ATA_SS_SPD_GEN2          equ    00000020h
    ATA_SS_SPD_GEN3          equ    00000030h

    ATA_SS_IPM_MASK          equ    00000F00h
    ATA_SS_IPM_NO_DEVICE     equ    00000000h
    ATA_SS_IPM_ACTIVE        equ    00000100h
    ATA_SS_IPM_PARTIAL       equ    00000200h
    ATA_SS_IPM_SLUMBER       equ    00000600h
    ATA_SS_IPM_DEVSLEEP      equ    00000800h

ATA_SERROR                   equ           14
    ATA_SE_DATA_CORRECTED    equ    00000001h
    ATA_SE_COMM_CORRECTED    equ    00000002h
    ATA_SE_DATA_ERR          equ    00000100h
    ATA_SE_COMM_ERR          equ    00000200h
    ATA_SE_PROT_ERR          equ    00000400h
    ATA_SE_HOST_ERR          equ    00000800h
    ATA_SE_PHY_CHANGED       equ    00010000h
    ATA_SE_PHY_IERROR        equ    00020000h
    ATA_SE_COMM_WAKE         equ    00040000h
    ATA_SE_DECODE_ERR        equ    00080000h
    ATA_SE_PARITY_ERR        equ    00100000h
    ATA_SE_CRC_ERR           equ    00200000h
    ATA_SE_HANDSHAKE_ERR     equ    00400000h
    ATA_SE_LINKSEQ_ERR       equ    00800000h
    ATA_SE_TRANSPORT_ERR     equ    01000000h
    ATA_SE_UNKNOWN_FIS       equ    02000000h
    ATA_SE_EXCHANGED         equ    04000000h

ATA_SCONTROL                 equ           15
    ATA_SC_DET_MASK          equ    0000000Fh
    ATA_SC_DET_IDLE          equ    00000000h
    ATA_SC_DET_RESET         equ    00000001h
    ATA_SC_DET_DISABLE       equ    00000004h

    ATA_SC_SPD_MASK          equ    000000F0h
    ATA_SC_SPD_NO_SPEED      equ    00000000h
    ATA_SC_SPD_SPEED_GEN1    equ    00000010h
    ATA_SC_SPD_SPEED_GEN2    equ    00000020h
    ATA_SC_SPD_SPEED_GEN3    equ    00000030h

    ATA_SC_IPM_MASK          equ    00000F00h
    ATA_SC_IPM_NONE          equ    00000000h
    ATA_SC_IPM_DIS_PARTIAL   equ    00000100h
    ATA_SC_IPM_DIS_SLUMBER   equ    00000200h
    ATA_SC_IPM_DIS_DEVSLEEP  equ    00000400h

ATA_SACTIVE                  equ           16

AHCI_MAX_PORTS               equ           32
AHCI_MAX_SLOTS               equ           32
AHCI_MAX_IRQS                equ           16

;/<  SATA AHCI v1.0 register defines
AHCI_CAP                     equ            0
    AHCI_CAP_NPMASK          equ    0000001Fh
    AHCI_CAP_SXS             equ    00000020h
    AHCI_CAP_EMS             equ    00000040h
    AHCI_CAP_CCCS            equ    00000080h
    AHCI_CAP_NCS             equ    00001F00h
    AHCI_CAP_NCS_SHIFT       equ            8
    AHCI_CAP_PSC             equ    00002000h
    AHCI_CAP_SSC             equ    00004000h
    AHCI_CAP_PMD             equ    00008000h
    AHCI_CAP_FBSS            equ    00010000h
    AHCI_CAP_SPM             equ    00020000h
    AHCI_CAP_SAM             equ    00080000h
    AHCI_CAP_ISS             equ    00F00000h
    AHCI_CAP_ISS_SHIFT       equ           20
    AHCI_CAP_SCLO            equ    01000000h
    AHCI_CAP_SAL             equ    02000000h
    AHCI_CAP_SALP            equ    04000000h
    AHCI_CAP_SSS             equ    08000000h
    AHCI_CAP_SMPS            equ    10000000h
    AHCI_CAP_SSNTF           equ    20000000h
    AHCI_CAP_SNCQ            equ    40000000h
    AHCI_CAP_64BIT           equ    80000000h

AHCI_GHC                     equ            1
    AHCI_GHC_AE              equ    80000000h
    AHCI_GHC_MRSM            equ    00000004h
    AHCI_GHC_IE              equ    00000002h
    AHCI_GHC_HR              equ    00000001h

AHCI_IS                      equ            2
AHCI_PI                      equ            3
AHCI_VS                      equ            4

AHCI_CCCC                    equ            5
    AHCI_CCCC_TV_MASK        equ   0FFFF0000h
    AHCI_CCCC_TV_SHIFT       equ           16
    AHCI_CCCC_CC_MASK        equ    0000FF00h
    AHCI_CCCC_CC_SHIFT       equ            8
    AHCI_CCCC_INT_MASK       equ    000000F8h
    AHCI_CCCC_INT_SHIFT      equ            3
    AHCI_CCCC_EN             equ    00000001h

AHCI_CCCP                    equ            6
AHCI_EM_LOC                  equ            7

AHCI_EM_CTL                  equ            8
    AHCI_EM_MR               equ    00000001h
    AHCI_EM_TM               equ    00000100h
    AHCI_EM_RST              equ    00000200h
    AHCI_EM_LED              equ    00010000h
    AHCI_EM_SAFTE            equ    00020000h
    AHCI_EM_SES2             equ    00040000h
    AHCI_EM_SGPIO            equ    00080000h
    AHCI_EM_SMB              equ    01000000h
    AHCI_EM_XMT              equ    02000000h
    AHCI_EM_ALHD             equ    04000000h
    AHCI_EM_PM               equ    08000000h

AHCI_CAP2                    equ            9
    AHCI_CAP2_BOH            equ    00000001h
    AHCI_CAP2_NVMP           equ    00000002h
    AHCI_CAP2_APST           equ    00000004h
    AHCI_CAP2_SDS            equ    00000008h
    AHCI_CAP2_SADM           equ    00000010h
    AHCI_CAP2_DESO           equ    00000020h

AHCI_VSCAP                   equ         0A4h
AHCI_OFFSET                  equ          40h    ;/<  in dwords!
AHCI_STEP                    equ          20h    ;/<  in dwords!

AHCI_P_CLB                   equ            0
AHCI_P_CLBU                  equ            1
AHCI_P_FB                    equ            2
AHCI_P_FBU                   equ            3
AHCI_P_IS                    equ            4

AHCI_P_IE                    equ            5
    AHCI_P_IX_DHR            equ    00000001h
    AHCI_P_IX_PS             equ    00000002h
    AHCI_P_IX_DS             equ    00000004h
    AHCI_P_IX_SDB            equ    00000008h
    AHCI_P_IX_UF             equ    00000010h
    AHCI_P_IX_DP             equ    00000020h
    AHCI_P_IX_PC             equ    00000040h
    AHCI_P_IX_MP             equ    00000080h

    AHCI_P_IX_PRC            equ    00400000h
    AHCI_P_IX_IPM            equ    00800000h
    AHCI_P_IX_OF             equ    01000000h
    AHCI_P_IX_INF            equ    04000000h
    AHCI_P_IX_IF             equ    08000000h
    AHCI_P_IX_HBD            equ    10000000h
    AHCI_P_IX_HBF            equ    20000000h
    AHCI_P_IX_TFE            equ    40000000h
    AHCI_P_IX_CPD            equ    80000000h

AHCI_P_CMD                   equ            6
    AHCI_P_CMD_ST            equ    00000001h
    AHCI_P_CMD_SUD           equ    00000002h
    AHCI_P_CMD_POD           equ    00000004h
    AHCI_P_CMD_CLO           equ    00000008h
    AHCI_P_CMD_FRE           equ    00000010h
    AHCI_P_CMD_CCS_MASK      equ    00001F00h
    AHCI_P_CMD_CCS_SHIFT     equ            8
    AHCI_P_CMD_ISS           equ    00002000h
    AHCI_P_CMD_FR            equ    00004000h
    AHCI_P_CMD_CR            equ    00008000h
    AHCI_P_CMD_CPS           equ    00010000h
    AHCI_P_CMD_PMA           equ    00020000h
    AHCI_P_CMD_HPCP          equ    00040000h
    AHCI_P_CMD_MPSP          equ    00080000h
    AHCI_P_CMD_CPD           equ    00100000h
    AHCI_P_CMD_ESP           equ    00200000h
    AHCI_P_CMD_FBSCP         equ    00400000h
    AHCI_P_CMD_APSTE         equ    00800000h
    AHCI_P_CMD_ATAPI         equ    01000000h
    AHCI_P_CMD_DLAE          equ    02000000h
    AHCI_P_CMD_ALPE          equ    04000000h
    AHCI_P_CMD_ASP           equ    08000000h
    AHCI_P_CMD_ICC_MASK      equ   0F0000000h
    AHCI_P_CMD_NOOP          equ    00000000h
    AHCI_P_CMD_ACTIVE        equ    10000000h
    AHCI_P_CMD_PARTIAL       equ    20000000h
    AHCI_P_CMD_SLUMBER       equ    60000000h
    AHCI_P_CMD_DEVSLEEP      equ    80000000h

AHCI_P_TFD                   equ            8
AHCI_P_SIG                   equ            9
AHCI_P_SSTS                  equ           10
AHCI_P_SCTL                  equ           11
AHCI_P_SERR                  equ           12
AHCI_P_SACT                  equ           13
AHCI_P_CI                    equ           14
AHCI_P_SNTF                  equ           15

AHCI_P_FBS                   equ           16
    AHCI_P_FBS_EN            equ    00000001h
    AHCI_P_FBS_DEC           equ    00000002h
    AHCI_P_FBS_SDE           equ    00000004h
    AHCI_P_FBS_DEV           equ    00000F00h
    AHCI_P_FBS_DEV_SHIFT     equ            8
    AHCI_P_FBS_ADO           equ    0000F000h
    AHCI_P_FBS_ADO_SHIFT     equ           12
    AHCI_P_FBS_DWE           equ    000F0000h
    AHCI_P_FBS_DWE_SHIFT     equ           16

AHCI_P_DEVSLP                equ           17
    AHCI_P_DEVSLP_ADSE       equ    00000001h
    AHCI_P_DEVSLP_DSP        equ    00000002h
    AHCI_P_DEVSLP_DETO       equ    000003FCh
    AHCI_P_DEVSLP_DETO_SHIFT equ            2
    AHCI_P_DEVSLP_MDAT       equ    00007C00h
    AHCI_P_DEVSLP_MDAT_SHIFT equ           10
    AHCI_P_DEVSLP_DITO       equ    01FF8000h
    AHCI_P_DEVSLP_DITO_SHIFT equ           15
    AHCI_P_DEVSLP_DM         equ    0E000000h
    AHCI_P_DEVSLP_DM_SHIFT   equ           25

;/<  Command list. 32 commands. First, 1Kbyte aligned.
AHCI_CL_OFFSET               equ            0
AHCI_CL_SIZE                 equ           32
;/<  Command tables. Up to 32 commands, Each, 128byte aligned.
AHCI_CT_OFFSET               equ (AHCI_CL_OFFSET + AHCI_CL_SIZE * AHCI_MAX_SLOTS)

ahci_dma_prd    struc
dba             dq      ?
reserved        dd      ?
dbc             dd      ?                        ;/<  0 based
ahci_dma_prd    ends

AHCI_PRD_MASK                equ      3FFFFFh    ;/<  max 4Mb
AHCI_PRD_MAX                 equ (AHCI_PRD_MASK + 1)
AHCI_PRD_IPC                 equ    80000000h

ahci_cmd_tab    struc
cfis            db      64 dup(?)
acmd            db      32 dup(?)
res_cmd         db      32 dup(?)
;cr struct ahci_dma_prd         prd_tab[1];
prd_tab         db      16 dup(?)
ahci_cmd_tab    ends

ahci_cmd_list   struc
cmd_flags       dw      ?
prd_length      dw      ?                        ;/<  PRD entries
bytecount       dd      ?
cmd_table_phys  dq      ?                        ;/<  128byte aligned
ahci_cmd_list   ends

AHCI_CMD_ATAPI               equ         020h
AHCI_CMD_WRITE               equ         040h
AHCI_CMD_PREFETCH            equ         080h
AHCI_CMD_RESET               equ         100h
AHCI_CMD_BIST                equ         200h
AHCI_CMD_CLR_BUSY            equ         400h

;cc #ifndef __cplusplus
;cc typedef struct ahci_cmd_list ahci_cmd_list;
;cc typedef struct ahci_cmd_tab   ahci_cmd_tab;
;cc typedef struct ahci_dma_prd   ahci_dma_prd;
;cc #endif

; ATA device commands
ATA_CMD_DEV_RESET            equ         008h
ATA_CMD_CHK_POWER            equ         0E5h
ATA_CMD_STANDBY              equ         0E2h
ATA_CMD_IDLE                 equ         0E3h
ATA_CMD_EDD                  equ         090h
ATA_CMD_DOWNLOAD_MICRO       equ         092h
ATA_CMD_NOP                  equ         000h
ATA_CMD_FLUSH                equ         0E7h
ATA_CMD_FLUSH_EXT            equ         0EAh
ATA_CMD_ID_ATA               equ         0ECh
ATA_CMD_ID_ATAPI             equ         0A1h
ATA_CMD_SERVICE              equ         0A2h
ATA_CMD_READ                 equ         0C8h
ATA_CMD_READ_EXT             equ         025h
ATA_CMD_READ_QUEUED          equ         026h
ATA_CMD_READ_STREAM_EXT      equ         02Bh
ATA_CMD_READ_STREAM_DMA_EXT  equ         02Ah
ATA_CMD_WRITE                equ         0CAh
ATA_CMD_WRITE_EXT            equ         035h
ATA_CMD_WRITE_QUEUED         equ         036h
ATA_CMD_WRITE_STREAM_EXT     equ         03Bh
ATA_CMD_WRITE_STREAM_DMA_EXT equ         03Ah
ATA_CMD_WRITE_FUA_EXT        equ         03Dh
ATA_CMD_WRITE_QUEUED_FUA_EXT equ         03Eh
ATA_CMD_FPDMA_READ           equ         060h
ATA_CMD_FPDMA_WRITE          equ         061h
ATA_CMD_PIO_READ             equ         020h
ATA_CMD_PIO_READ_EXT         equ         024h
ATA_CMD_PIO_WRITE            equ         030h
ATA_CMD_PIO_WRITE_EXT        equ         034h
ATA_CMD_READ_MULTI           equ         0C4h
ATA_CMD_READ_MULTI_EXT       equ         029h
ATA_CMD_WRITE_MULTI          equ         0C5h
ATA_CMD_WRITE_MULTI_EXT      equ         039h
ATA_CMD_WRITE_MULTI_FUA_EXT  equ         0CEh
ATA_CMD_SET_FEATURES         equ         0EFh
ATA_CMD_SET_MULTI            equ         0C6h
ATA_CMD_PACKET               equ         0A0h
ATA_CMD_VERIFY               equ         040h
ATA_CMD_VERIFY_EXT           equ         042h
ATA_CMD_WRITE_UNCORR_EXT     equ         045h
ATA_CMD_STANDBYNOW1          equ         0E0h
ATA_CMD_IDLEIMMEDIATE        equ         0E1h
ATA_CMD_SLEEP                equ         0E6h
ATA_CMD_INIT_DEV_PARAMS      equ         091h
ATA_CMD_READ_NATIVE_MAX      equ         0F8h
ATA_CMD_READ_NATIVE_MAX_EXT  equ         027h
ATA_CMD_SET_MAX              equ         0F9h
ATA_CMD_SET_MAX_EXT          equ         037h
ATA_CMD_READ_LOG_EXT         equ         02Fh
ATA_CMD_WRITE_LOG_EXT        equ         03Fh
ATA_CMD_READ_LOG_DMA_EXT     equ         047h
ATA_CMD_WRITE_LOG_DMA_EXT    equ         057h
ATA_CMD_TRUSTED_RCV          equ         05Ch
ATA_CMD_TRUSTED_RCV_DMA      equ         05Dh
ATA_CMD_TRUSTED_SND          equ         05Eh
ATA_CMD_TRUSTED_SND_DMA      equ         05Fh
ATA_CMD_PMP_READ             equ         0E4h
ATA_CMD_PMP_WRITE            equ         0E8h
ATA_CMD_CONF_OVERLAY         equ         0B1h
ATA_CMD_SEC_SET_PASS         equ         0F1h
ATA_CMD_SEC_UNLOCK           equ         0F2h
ATA_CMD_SEC_ERASE_PREP       equ         0F3h
ATA_CMD_SEC_ERASE_UNIT       equ         0F4h
ATA_CMD_SEC_FREEZE_LOCK      equ         0F5h
ATA_CMD_SEC_DISABLE_PASS     equ         0F6h
ATA_CMD_CONFIG_STREAM        equ         051h
ATA_CMD_SMART                equ         0B0h
ATA_CMD_MEDIA_LOCK           equ         0DEh
ATA_CMD_MEDIA_UNLOCK         equ         0DFh
ATA_CMD_DSM                  equ         006h
ATA_CMD_CHK_MED_CRD_TYP      equ         0D1h
ATA_CMD_CFA_REQ_EXT_ERR      equ         003h
ATA_CMD_CFA_WRITE_NE         equ         038h
ATA_CMD_CFA_TRANS_SECT       equ         087h
ATA_CMD_CFA_ERASE            equ         0C0h
ATA_CMD_CFA_WRITE_MULT_NE    equ         0CDh


;cc #endif // QS_BASE_AHCI_INC
