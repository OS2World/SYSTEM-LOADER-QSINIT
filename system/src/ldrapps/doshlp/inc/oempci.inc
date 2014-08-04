;
; QSINIT
; doshlp code - oemhlp pci functions
;

OEM_PCI_BIOS_INFO       equ     0                               ;
PciBiosInfo_P           struc                                   ;
pbi_bPCIFunc            db      ?                               ;
PciBiosInfo_P           ends                                    ;

PCI_PBI_PARM_SIZE       equ     SIZE PciBiosInfo_P              ;

PciBiosInfo_D           struc                                   ;
pbi_bReturn             db      ?                               ;
pbi_bHWMech             db      ?                               ;
pbi_bMajorRev           db      ?                               ;
pbi_bMinorRev           db      ?                               ;
pbi_bLastBus            db      ?                               ;
PciBiosInfo_D           ends                                    ;

PCI_PBI_DATA_SIZE       equ     SIZE PciBiosInfo_D              ;

OEM_PCI_FIND_DEVICE     equ     1                               ;
PciFindDevice_P         struc                                   ;
pfd_bPCIFunc            db      ?                               ;
pfd_usDeviceID          dw      ?                               ;
pfd_usVendorID          dw      ?                               ;
pfd_bIndex              db      ?                               ;
PciFindDevice_P         ends                                    ;

PCI_PFD_PARM_SIZE       equ     SIZE PciFindDevice_P            ;

PciFindDevice_D         struc                                   ;
pfd_bReturn             db      ?                               ;
pfd_bBusNum             db      ?                               ;
pfd_bDevFunc            db      ?                               ;
PciFindDevice_D         ends                                    ;

PCI_PFD_DATA_SIZE       equ     SIZE PciFindDevice_D            ;

OEM_PCI_FIND_CLASS_CODE equ     2                               ;
PciFindClassCode_P      struc                                   ;
pfcc_bPCIFunc           db      ?                               ;
pfcc_ulClassCode        dd      ?                               ;
pfcc_bIndex             db      ?                               ;
PciFindClassCode_P      ends                                    ;

PCI_PFCC_PARM_SIZE      equ     SIZE PciFindClassCode_P         ;

PciFindClassCode_D      struc                                   ;
pfcc_bReturn            db      ?                               ;
pfcc_bBusNum            db      ?                               ;
pfcc_bDevFunc           db      ?                               ;
PciFindClassCode_D      ends                                    ;

PCI_PFCC_DATA_SIZE      equ     SIZE PciFindClassCode_D         ;

OEM_PCI_READ_CONFIG     equ     3                               ;
PciReadConfig_P         struc                                   ;
prc_bPCIFunc            db      ?                               ;
prc_bBusNum             db      ?                               ;
prc_bDevFunc            db      ?                               ;
prc_bConfigReg          db      ?                               ;
prc_bSize               db      ?                               ;
PciReadConfig_P         ends                                    ;

PCI_PRC_PARM_SIZE       equ     SIZE PciReadConfig_P            ;

PciReadConfig_D         struc                                   ;
prc_bReturn             db      ?                               ;
prc_ulData              dd      ?                               ;
PciReadConfig_D         ends                                    ;

PCI_PRC_DATA_SIZE       equ     SIZE PciReadConfig_D            ;

OEM_PCI_WRITE_CONFIG    equ     4                               ;
PciWriteConfig_P        struc                                   ;
pwc_bPCIFunc            db      ?                               ;
pwc_bBusNum             db      ?                               ;
pwc_bDevFunc            db      ?                               ;
pwc_bConfigReg          db      ?                               ;
pwc_bSize               db      ?                               ;
pwc_ulData              dd      ?                               ;
PciWriteConfig_P        ends                                    ;

PCI_PWC_PARM_SIZE       equ     SIZE PciWriteConfig_P           ;

PciWriteConfig_D        struc                                   ;
pwc_bReturn             db      ?                               ;
PciWriteConfig_D        ends                                    ;

PCI_PWC_DATA_SIZE       equ     SIZE PciWriteConfig_D           ;
