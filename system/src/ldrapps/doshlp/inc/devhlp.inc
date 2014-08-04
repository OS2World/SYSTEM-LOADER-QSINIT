;
; QSINIT
; doshlp code - devhlp functions
;

;---------------------------------------------------------------
; 16-Bit DevHelps
;---------------------------------------------------------------
DevHlp_SchedClock       equ     0       ;  0  Called each timer tick
DevHlp_DevDone          equ     1       ;  1  Device I/O complete
DevHlp_Yield            equ     2       ;  2  yield CPU if resched set
DevHlp_TCYield          equ     3       ;  3  yield to time critical task
                                        
DevHlp_ProcBlock        equ     4       ;  4  Block on event
DevHlp_ProcRun          equ     5       ;  5  Unblock process
                                        
DevHlp_SemRequest       equ     6       ;  6  claim a semaphore
DevHlp_SemClear         equ     7       ;  7  release a semaphore
DevHlp_SemHandle        equ     8       ;  8  obtain a semaphore handle
                                        
DevHlp_PushRequest      equ     9       ;  9  Push the request
DevHlp_PullRequest      equ    10       ;  A  Pull next request from Q
DevHlp_PullParticular   equ    11       ;  B  Pull a specific request
DevHlp_SortRequest      equ    12       ;  C  Push request in sorted order
                                        
DevHlp_AllocReqPacket   equ    13       ;  D  allocate request packet
DevHlp_FreeReqPacket    equ    14       ;  E  free request packet
                                        
DevHlp_QueueInit        equ    15       ;  F  Init/Clear char queue
DevHlp_QueueFlush       equ    16       ; 10  flush queue
DevHlp_QueueWrite       equ    17       ; 11  Put a char in the queue
DevHlp_QueueRead        equ    18       ; 12  Get a char from the queue
                                        
DevHlp_Lock             equ    19       ; 13  Lock segment
DevHlp_Unlock           equ    20       ; 14  Unlock segment
                                        
DevHlp_PhysToVirt       equ    21       ; 15  convert physical address to virtual
DevHlp_VirtToPhys       equ    22       ; 16  convert virtual address to physical
DevHlp_PhysToUVirt      equ    23       ; 17  convert physical to LDT
                                        
DevHlp_AllocPhys        equ    24       ; 18  allocate physical memory
DevHlp_FreePhys         equ    25       ; 19  free physical memory
                                        
DevHlp_SetROMVector     equ    26       ; 1A  set a ROM service routine vector
DevHlp_SetIRQ           equ    27       ; 1B  set an IRQ interrupt
DevHlp_UnSetIRQ         equ    28       ; 1C  unset an IRQ interrupt
                                        
DevHlp_SetTimer         equ    29       ; 1D  set timer request handler
DevHlp_ResetTimer       equ    30       ; 1E  unset timer request handler
                                        
DevHlp_MonitorCreate    equ    31       ; 1F  create a monitor
DevHlp_Register         equ    32       ; 20  install a monitor
DevHlp_DeRegister       equ    33       ; 21  remove a monitor
DevHlp_MonWrite         equ    34       ; 22  pass data records to monitor
DevHlp_MonFlush         equ    35       ; 23  remove all data from stream
                                        
DevHlp_GetDOSVar        equ    36       ; 24  Return pointer to DOS variable

; Dos variable indexes used in DevHlp_GetDOSVar
DHGETDOSV_SYSINFOSEG    equ    1
DHGETDOSV_LOCINFOSEG    equ    2
DHGETDOSV_VECTORSDF     equ    4
DHGETDOSV_VECTORREBOOT  equ    5
DHGETDOSV_VECTORMSATS   equ    6
DHGETDOSV_INTERRUPTLEV  equ    13       ; Interrupt level
DHGETDOSV_DEVICECLASSTABLE equ 14       ; Device Class Table
DHGETDOSV_DMQSSEL       equ    15       ; DMQS selector

DevHlp_SendEvent        equ    37       ; 25  an event occurred
DevHlp_ROMCritSection   equ    38       ; 26  ROM Critical Section
DevHlp_VerifyAccess     equ    39       ; 27  Verify access to memory
DevHlp_RAS              equ    40       ; 28  Put info in RAS trace buffer
                                    
DevHlp_ABIOSGetParms    equ    41       ; 29  Get ABIOS Calling Parms
DevHlp_AttachDD         equ    42       ; 2A  Attach to a device driver
DevHlp_InternalError    equ    43       ; 2B  Signal an internal error
DevHlp_ModifyPriority   equ    44       ; 2C  Undocumented (used by PM)
DevHlp_AllocGDTSelector equ    45       ; 2D  Allocate GDT Selectors
DevHlp_PhysToGDTSelector equ   46       ; 2E  Convert phys addr to GDT sel
DevHlp_RealToProt       equ    47       ; 2F  Change from real to protected mode
DevHlp_ProtToReal       equ    48       ; 30  Change from protected to real mode
                                    
DevHlp_EOI              equ    49       ; 31  Send EOI to PIC
DevHlp_UnPhysToVirt     equ    50       ; 32  mark completion of PhysToVirt
DevHlp_TickCount        equ    51       ; 33  modify timer
                                    
DevHlp_GetLIDEntry      equ    52       ; 34  Obtain Logical ID
DevHlp_FreeLIDEntry     equ    53       ; 35  Release Logical ID
DevHlp_ABIOSCall        equ    54       ; 36  Call ABIOS
DevHlp_ABIOSCommonEntry equ    55       ; 37  Invoke Common Entry Point
DevHlp_GetDeviceBlock   equ    56       ; 38  Get ABIOS Device Block
                                        ; 39  Reserved for Profiling Kernel
DevHlp_RegisterStackUsage equ  58       ; 3A  Register for stack usage
DevHlp_LogEntry         equ    59       ; 3B  Place data in log buffer
                                    
DevHlp_VideoPause       equ    60       ; 3C  Video pause on/off
                                    
DevHlp_Save_Message     equ    61       ; 3D  Save msg in SysInit Message Table
                                    
DevHlp_SegRealloc       equ    62       ; 3E  Realloc DD protect mode segment
DevHlp_PutWaitingQueue  equ    63       ; 3F  Put I/O request on waiting queue
DevHlp_GetWaitingQueue  equ    64       ; 40  Get I/O request from waiting queue
DevHlp_RegisterDeviceClass equ     67   ; 43  Register DC entry point

;---------------------------------------------------------------
; 32-Bit DevHelps
;---------------------------------------------------------------
DevHlp_RegisterPDD      equ     80      ; 50  Register PDD entry point with
                                        ;     VDM manager for later PDD-VDD
                                        ;     communication
DevHlp_RegisterBeep     equ     81      ; 51  register PTD beep service
                                        ;     entry point with kernel
DevHlp_Beep             equ     82      ; 52  preempt beep service via PTD
DevHlp_FreeGDTSelector  equ     83      ; 53  Free allocated GDT selector
DevHlp_PhysToGDTSel     equ     84      ; 54  Convert Phys Addr to GDT sel
                                        ;     with given access
DevHlp_VMLock           equ     85      ; 55  Lock linear address range
DevHlp_VMUnlock         equ     86      ; 56  Unlock address range
DevHlp_VMAlloc          equ     87      ; 56  Allocate memory
DevHlp_VMFree           equ     88      ; 58  Free memory or mapping
DevHlp_VMProcessToGlobal equ    89      ; 59  Create global mapping to process
                                        ;     memory

DevHlp_VMGlobalToProcess equ    90      ; 5A  Create process mapping to global
                                        ;     memory

DevHlp_VirtToLin        equ     91      ; 5B  Convert virtual address to linear
DevHlp_LinToGDTSelector equ     92      ; 5C  Convert linear address to virtual
DevHlp_GetDescInfo      equ     93      ; 5D  Return descriptor information
DevHlp_LinToPageList    equ     94      ; 5E  build pagelist array from lin addr
DevHlp_PageListToLin    equ     95      ; 5F  map page list array to lin addr
DevHlp_PageListToGDTSelector equ    96  ; 60  map page list array to GDT sel.
DevHlp_RegisterTmrDD    equ     97      ; 61  register TMR Device Driver.
DevHlp_RegisterPerfCtrs equ     98      ; 62  register device driver perf. ctrs (PVW).

DevHlp_AllocateCtxHook  equ     99      ; 63  allocate a context hook
DevHlp_FreeCtxHook      equ     100     ; 64  free a context hook
DevHlp_ArmCtxHook       equ     101     ; 65  arm a context hook

DevHlp_VMSetMem         equ     102     ; 66  commit/decommit memory

DevHlp_OpenEventSem     equ     103     ; 67  open an event semaphore
DevHlp_CloseEventSem    equ     104     ; 68  close an event semaphore
DevHlp_PostEventSem     equ     105     ; 69  post an event semaphore
DevHlp_ResetEventSem    equ     106     ; 6A  reset an event semaphore
DevHlp_RegisterFreq     equ     107     ; 6B  register PTD freq service
                                        ;     entry point with kernel
DevHlp_DynamicAPI       equ     108     ; 6C  add a dynamic API
;
; Request bit definitions used in DevHlp_DynamicAPI
DHDYNAM_16B_CALLGATE    equ     1       ; 1 = 16 bit callgate, 0 = 32 bit
DHDYNAM_16B_APIRTN      equ     2       ; 1 = 16 bit API rtn,  0 = 32 rtn

DevHlp_ProcRun2         equ     109     ; 6D  unblock process via procrun2
DevHlp_CreateInt13VDM   equ     110     ; 6E  create Int13 VDM
DevHlp_RegisterKrnlExit equ     111     ; 6F  used to capture Kernel Exits
DevHlp_PMPostEventSem   equ     112     ; 70  PM Post Event Semaphore
DevHlp_KillProc         equ     125     ; 7D  kill Proc
DevHlp_QSysState        equ     126     ; 7E  query System State

DevHlp_OpenFile         equ     127     ; 7F  ring-0 File system Write
DevHlp_CloseFile        equ     128     ; 80  ring-0 File system Seek
DevHlp_ReadFile         equ     129     ; 81  ring-0 File system Read
DevHlp_ReadFileAt       equ     130     ; 82  file system Read at (seek)

VMDHA_16M               equ   0001h     ; DevHlp_VMAlloc flags
VMDHA_FIXED             equ   0002h     ;
VMDHA_SWAP              equ   0004h     ;
VMDHA_CONTIG            equ   0008h     ;
VMDHA_PHYS              equ   0010h     ;
VMDHA_PROCESS           equ   0020h     ;
VMDHA_SGSCONT           equ   0040h     ;
VMDHA_RESERVE           equ   0100h     ;
VMDHA_USEHIGHMEM        equ   0800h     ;
