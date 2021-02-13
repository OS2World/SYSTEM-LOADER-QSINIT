;
; DosTable and DosTable2 (DevHelp_GetDOSVar(9))
;
;cc #ifndef QS_DOSHLP_DOSTAB
;cc #define QS_DOSHLP_DOSTAB

;cc #pragma pack(1)

DosTableRec        struc                                        ;
dtab_size          db      ?                                    ; size in dwords
dtab_HardError     dd      ?                                    ;
dtab_UCase         dd      ?                                    ;
dtab_res1          dd      3 dup (?)                            ;
dtab_MemMapAlias   dd      ?                                    ;
dtab_MemUnmapAlias dd      ?                                    ;
dtab_GoProtAll     dd      ?                                    ;
dtab_GoRealAll     dd      ?                                    ;
dtab_dhRedirDev    dd      ?                                    ;
dtab_res2          dd      2 dup (?)                            ;
dtab_SFFromSFN     dd      ?                                    ;
dtab_SegGetInfo    dd      ?                                    ;
dtab_AsgCheckDrive dd      ?                                    ;
dtab_GDTRedir      dd      ?                                    ;
dtab_RedirCallOut  dd      ?                                    ;
DosTableRec        ends                                         ;

DosTable2Rec       struc                                        ;
dtab2_size         db      ?                                    ; size in dwords
dtab2_ErrMap24     dd      ?                                    ;
dtab2_ErrMap24End  dd      ?                                    ;
dtab2_ErrTable24   dd      ?                                    ;
dtab2_CDSAddr      dd      ?                                    ;
dtab2_GDTRes       dd      ?                                    ;
dtab2_IntLevel     dd      ?                                    ;
dtab2_InDos        dd      ?                                    ;
dtab2_RMStackSeg   dd      ?                                    ;
dtab2_Seg3xBoxPTDA dd      ?                                    ;
dtab2_FlatCS       dd      ?                                    ;
dtab2_FlatDS       dd      ?                                    ;
dtab2_TKSSBase     dd      ?                                    ;
dtab2_IntSwStack   dd      ?                                    ;
dtab2_PrivateStack dd      ?                                    ;
dtab2_PDiskTable   dd      ?                                    ;
dtab2_EMHandler    dd      ?                                    ;
dtab2_ReserveVM    dd      ?                                    ;
dtab2_PgpPageDir   dd      ?                                    ;
DosTable2Rec       ends                                         ;

PhysDiskTableHdr   struc
pdth_tbllen        dw      ?                                    ; length
pdth_reserved      db      ?                                    ;
pdth_totaldisks    db      ?                                    ; # of partitionable disks
PhysDiskTableHdr   ends

; Physical Disk Entry Structure
PhysDiskEntry      struc
pdte_ddheader      dd      ?                                    ; pointer to DD header
pdte_reserved      db      ?                                    ;
pdte_reldisknum    db      ?                                    ; relative physical disk # for DD
pdte_devpid        dw      ?                                    ;
pdte_devflags      dw      ?                                    ;
pdte_logunitbmp    dd      ?                                    ; logical units bitmap
PhysDiskEntry      ends

;cc #pragma pack()

;cc #endif // QS_DOSHLP_DOSTAB
