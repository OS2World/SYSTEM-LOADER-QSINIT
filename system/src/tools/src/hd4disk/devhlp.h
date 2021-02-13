//
// QSINIT OS/2 "ramdisk" basedev
// devhelp header for watcom
//

#ifndef QS_DEVHELP_H
#define QS_DEVHELP_H

#include "qstypes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef u32t         LIN;       // 32-Bit Linear Addess
typedef u32t   far *PLIN;       // 16:16 Ptr to a 32-Bit Linear Addess
typedef u8t   far *PBYTE;

typedef struct {
   u32t         PhysAddr;
   u32t             Size;
} PAGELIST, far *PPAGELIST;

extern void (far *DevHelp)(void);

#define DEVHELP_CALL \
   "call    dword ptr [DevHelp]" \
   "jc      error"          \
   "sub     ax,ax"          \
"error:"                    \
   value [ax]

u16t DevHelp_ABIOSCall(u16t Lid, u8t near *ReqBlk, u16t Entry_Type);
#pragma aux DevHelp_ABIOSCall = \
   "mov     dl,36h"         \
   DEVHELP_CALL             \
   parm caller [ax] [si] [dh]  \
   modify exact [ax dl];

u16t DevHelp_AllocateCtxHook(void near *HookHandler, u32t far *HookHandle);
#pragma aux DevHelp_AllocateCtxHook = \
   "push    bx"             \
   "movzx   eax,ax"         \
   "mov     ebx,-1"         \
   "mov     dl,63h"         \
   "call    dword ptr [DevHelp]" \
   "pop     bx"             \
   "jc      error"          \
   "mov     es:[bx],eax"    \
   "sub     ax,ax"          \
"error:"                    \
   value [ax]               \
   parm caller nomemory [ax] [es bx] \
   modify exact [ax bx dl es];

u16t DevHelp_AllocGDTSelector(u16t far *Selectors, u16t Count);
#pragma aux DevHelp_AllocGDTSelector = \
   "mov     dl,2Dh"         \
   DEVHELP_CALL             \
   parm caller [es di] [cx] \
   modify nomemory exact [ax dl];

#define MEMTYPE_ABOVE_1M   0
#define MEMTYPE_BELOW_1M   1

u16t DevHelp_AllocPhys(u32t lSize, u16t MemType, u32t far *PhysAddr);
#pragma aux DevHelp_AllocPhys = \
   "xchg    ax,bx"          \
   "mov     dl,18h"         \
   "call    dword ptr[DevHelp]" \
   "jc      error"          \
   "mov     es:[di],bx"     \
   "mov     es:[di+2],ax"   \
   "sub     ax,ax"          \
"error:"                    \
   value [ax]               \
   parm caller nomemory [ax bx] [dh] [es di] \
   modify exact [ax bx dx];

#define WAIT_NOT_ALLOWED 0
#define WAIT_IS_ALLOWED  1

u16t DevHelp_AllocReqPacket(u16t WaitFlag, pvoid far *ReqPktAddr);
#pragma aux DevHelp_AllocReqPacket = \
   "mov     dl,0Dh",        \
   "call    dword ptr [DevHelp]" \
   "jc      error"          \
   "push    es"             \
   "push    bx"             \
   "mov     bx,sp"          \
   "les     bx,ss:[bx]"     \
   "pop     es:[bx]"        \
   "pop     es:[bx+2]"      \
"error:"                    \
   "mov     ax,0"           \
   "sbb     ax,0"           \
   value [ax]               \
   parm caller [dh] []      \
   modify exact [ax bx dl es];

u16t DevHelp_ArmCtxHook(u32t HookData, u32t HookHandle);
#pragma aux DevHelp_ArmCtxHook = \
   "mov     bx,sp"          \
   "mov     dl,65h",        \
   "mov     eax,ss:[bx]"    \
   "mov     ecx,-1"         \
   "mov     ebx,ss:[bx+4]"  \
   DEVHELP_CALL             \
   parm caller nomemory []  \
   modify nomemory exact [ax bx cx dl];

typedef struct {
  u16t           Reserved[3];
  void (far *ProtIDCEntry)(void);
  u16t         ProtIDC_DS;
} IDCTABLE, __near *NPIDCTABLE;

u16t DevHelp_AttachDD(char near *DDName, u8t near *DDTable);
#pragma aux DevHelp_AttachDD = \
   "mov     dl,2Ah"         \
   "call    dword ptr [DevHelp]" \
   "mov     ax,0"           \
   "sbb     ax,0"           \
   value [ax]               \
   parm caller [bx] [di]    \
   modify exact [ax dl];

u16t DevHelp_Beep(u16t Frequency, u16t DurationMS);
#pragma aux DevHelp_Beep = \
   "mov     dl,52h"         \
   DEVHELP_CALL             \
   parm caller nomemory [bx] [cx] \
   modify nomemory exact [ax dx];

u16t DevHelp_CloseEventSem(u32t hEvent);
#pragma aux DevHelp_CloseEventSem = \
   "movzx   esp,sp"         \
   "mov     dl,68h"         \
   "mov     eax,[esp]"      \
   DEVHELP_CALL             \
   parm caller nomemory []  \
   modify nomemory exact [ax dl];

u16t DevHelp_DeRegister(u16t MonitorPID, u16t MonitorHandle, u16t far *MonitorsLeft);
#pragma aux DevHelp_DeRegister = \
   "mov     dl,21h"         \
   "call    dword ptr [DevHelp]" \
   "jc      error"          \
   "mov     es:[di],ax"     \
   "sub     ax,ax"          \
"error:"                    \
   value [ax]               \
   parm caller nomemory [bx] [ax] [es di] \
   modify exact [ax dl];

u16t DevHelp_DevDone(void far *ReqPktAddr);
#pragma aux DevHelp_DevDone = \
   "mov     dl,1"           \
   DEVHELP_CALL             \
   parm caller [es bx]      \
   modify exact [ax dl];

#define DYNAPI_CALLGATE16       0x0001   // 16:16 CallGate
#define DYNAPI_CALLGATE32       0x0000   //  0:32 CallGate

#define DYNAPI_ROUTINE16        0x0002   // 16:16 Routine Addr
#define DYNAPI_ROUTINE32        0x0000   //  0:32 Routine Addr

u16t DevHelp_DynamicAPI(void far *RoutineAddress, u16t ParmCount, u16t Flags,
                        u16t far *CallGateSel);
#pragma aux DevHelp_DynamicAPI = \
   "mov     dl,6Ch"         \
   "xchg    ax,bx"          \
   "call    dword ptr [DevHelp]" \
   "jc      error"          \
   "mov     es:[si],di"     \
   "sub     ax,ax"          \
"error:"                    \
   value [ax]               \
   parm caller [ax bx] [cx] [dh] [es si] \
   modify exact [ax bx di dl];

u16t DevHelp_EOI(u16t IRQLevel);
#pragma aux DevHelp_EOI = \
   "mov     dl,31h"         \
   "call    dword ptr [DevHelp]" \
   "sub     ax,ax"          \
   value [ax]               \
   parm caller nomemory [al] \
   modify nomemory exact [ax dl];

u16t DevHelp_FreeCtxHook(u32t HookHandle);
#pragma aux DevHelp_FreeCtxHook = \
   "movzx   esp,sp"         \
   "mov     dl,64h",        \
   "mov     eax,[esp]"      \
   DEVHELP_CALL             \
   parm caller nomemory []  \
   modify nomemory exact [ax dl];

u16t DevHelp_FreeGDTSelector(u16t Selector);
#pragma aux DevHelp_FreeGDTSelector = \
   "mov     dl,53h"         \
   DEVHELP_CALL             \
   parm caller nomemory [ax] \
   modify nomemory exact [ax dl];

u16t DevHelp_FreeLIDEntry(u16t LIDNumber);
#pragma aux DevHelp_FreeLIDEntry = \
   "mov     dl,35h"            \
   DEVHELP_CALL                \
   parm caller nomemory [ax]   \
   modify nomemory exact [ax dl];

u16t DevHelp_FreePhys(u32t PhysAddr);
#pragma aux DevHelp_FreePhys = \
   "xchg    ax,bx"          \
   "mov     dl,19h"         \
   "call    dword ptr [DevHelp]" \
   "mov     ax,0"           \
   "sbb     ax,0"           \
   value [ax]               \
   parm caller nomemory [ax bx] \
   modify exact [ax bx dl];

u16t DevHelp_FreeReqPacket(void far *ReqPktAddr);
#pragma aux DevHelp_FreeReqPacket = \
   "mov     dl,0Eh",        \
   "call    dword ptr [DevHelp]" \
   "sub     ax,ax"          \
   value [ax]               \
   parm caller [es bx]      \
   modify exact [ax dl];

typedef struct {
   u8t           Type;
   u8t    Granularity;
   LIN       BaseAddr;
   u32t         Limit;
} SELDESCINFO, __far *PSELDESCINFO;

typedef struct _GATEDESCINFO {
   u8t           Type;
   u8t      ParmCount;
   u16t      Selector;
   u16t    Reserved_1;
   u32t        Offset;
} GATEDESCINFO, __far *PGATEDESCINFO;

u16t DevHelp_GetDescInfo(u16t Selector, void far *SelInfo);
#pragma aux DevHelp_GetDescInfo = \
   "mov     dl,5Dh"         \
   "call    dword ptr [DevHelp]" \
   "jc      error"          \
   "mov     es:[bx],ax"     \
   "mov     es:[bx+2],ecx"  \
   "mov     es:[bx+6],edx"  \
   "sub     ax,ax"          \
"error:"                    \
   value [ax]               \
   parm caller nomemory [ax] [es bx] \
   modify exact [ax cx dx];

u16t DevHelp_GetDeviceBlock(u16t Lid, pvoid far *DeviceBlockPtr);
#pragma aux DevHelp_GetDeviceBlock = \
   "mov     dl,38h"         \
   "call    dword ptr [DevHelp]" \
   "jc      error"          \
   "mov     es:[si],dx"     \
   "mov     es:[si+2],cx"   \
   "sub     ax,ax"          \
"error:"                    \
   value [ax]               \
   parm caller nomemory [ax] [es si] \
   modify exact [ax bx cx dx];

#define DHGETDOSV_SYSINFOSEG            1
#define DHGETDOSV_LOCINFOSEG            2
#define DHGETDOSV_VECTORSDF             4
#define DHGETDOSV_VECTORREBOOT          5
#define DHGETDOSV_YIELDFLAG             7
#define DHGETDOSV_TCYIELDFLAG           8
#define DHGETDOSV_DOSCODEPAGE           11
#define DHGETDOSV_INTERRUPTLEV          13
#define DHGETDOSV_DEVICECLASSTABLE      14
#define DHGETDOSV_DMQSSELECTOR          15
#define DHGETDOSV_APMINFO               16

u16t DevHelp_GetDOSVar(u16t VarNumber, u16t VarMember, pvoid far *KernelVar);
#pragma aux DevHelp_GetDOSVar = \
   "mov     dl,24h"         \
   "call    dword ptr [DevHelp]" \
   "mov     es:[di],bx"     \
   "mov     es:[di+2],ax"   \
   "sub     ax,ax"          \
   value [ax]               \
   parm caller nomemory [al] [cx] [es di] \
   modify exact [ax bx dl];

u16t DevHelp_GetLIDEntry(u16t DeviceType, s16t LIDIndex, u16t Type, u16t far *LID);
#pragma aux DevHelp_GetLIDEntry = \
   "mov     dl,34h"         \
   "call    dword ptr [DevHelp]" \
   "jc      error"          \
   "mov     es:[di],ax"     \
   "sub     ax,ax"          \
"error:"                    \
   value [ax]               \
   parm caller nomemory [al] [bl] [dh] [es di] \
   modify exact [ax dl];

u16t DevHelp_InternalError(char far *MsgText, u16t MsgLength);
#pragma aux DevHelp_InternalError aborts = \
   "push    ds"             \
   "push    es"             \
   "pop     ds"             \
   "pop     es"             \
   "mov     dl,2Bh"         \
   "jmp     dword ptr es:[DevHelp]" \
   parm [es si] [di]        \
   modify nomemory exact [];

u16t DevHelp_LinToGDTSelector(u16t Selector, LIN LinearAddr, u32t Size);
#pragma aux DevHelp_LinToGDTSelector = \
   "mov     bx,sp"          \
   "mov     dl,5Ch"         \
   "mov     ecx,ss:[bx+4]"  \
   "mov     ebx,ss:[bx]"    \
   "call    dword ptr [DevHelp]" \
   "jc      error"          \
   "sub     ax,ax"          \
"error:"                    \
   value [ax]               \
   parm caller nomemory [ax] [] \
   modify nomemory exact [ax bx cx dl];

#define LOCKTYPE_SHORT_ANYMEM    0x00
#define LOCKTYPE_LONG_ANYMEM     0x01
#define LOCKTYPE_LONG_HIGHMEM    0x03
#define LOCKTYPE_SHORT_VERIFY    0x04

u16t DevHelp_Lock(u16t Segment, u16t LockType, u16t WaitFlag, u32t far *LockHandle);
#pragma aux DevHelp_Lock = \
   "mov     dl,13h"         \
   "call    dword ptr [DevHelp]" \
   "jc      error"          \
   "mov     es:[di],bx"     \
   "mov     es:[di+2],ax"   \
   "sub     ax,ax"          \
"error:"                    \
   value [ax]               \
   parm caller [ax] [bh] [bl] [es di] \
   modify exact [ax dl];

u16t DevHelp_MonFlush(u16t MonitorHandle);
#pragma aux DevHelp_MonFlush = \
   "mov     dl,23h"         \
   DEVHELP_CALL             \
   parm caller [ax]         \
   modify exact [ax dl];

u16t DevHelp_MonitorCreate(u16t MonitorHandle, PBYTE FinalBuffer, 
                           void near *NotifyRtn,
                           u16t far *MonitorChainHandle);
#pragma aux DevHelp_MonitorCreate = \
   "mov     dl,1Fh"         \
   "call    dword ptr [DevHelp]" \
   "jc      error"          \
   "movzx   esp,sp"         \
   "mov     si,[esp]"       \
   "mov     es,[esp+2]"     \
   "mov     es:[si],ax"     \
   "sub     ax,ax"          \
"error:"                    \
   value [ax]               \
   parm caller [ax] [es si] [di] [] \
   modify exact [ax dl es si];

/* DevHelp_MonWrite has one change compared to the original specification.
   The DataRecord parameter was originally a far (16:16) pointer.  It has
   been changed to a near pointer because DS must point to the default data
   segment and therefore the DataRecord parameter must be near.  This
   allows the compiler to catch the error. */
u16t DevHelp_MonWrite(u16t MonitorHandle, u8t near *DataRecord, u16t Count,
                      u32t TimeStampMS, u16t WaitFlag);
#pragma aux DevHelp_MonWrite = \
   "mov     dl,22h"         \
   DEVHELP_CALL             \
   parm caller [ax] [si] [cx] [di bx] [dh] \
   modify exact [ax dl];

u16t DevHelp_OpenEventSem(u32t hEvent);
#pragma aux DevHelp_OpenEventSem = \
   "mov     eax,[esp]"      \
   "mov     dl,67h"         \
   DEVHELP_CALL             \
   parm caller nomemory []  \
   modify nomemory exact [ax dl];

#define GDTSEL_R3CODE           0x0000
#define GDTSEL_R3DATA           0x0001
#define GDTSEL_R2CODE           0x0003
#define GDTSEL_R2DATA           0x0004
#define GDTSEL_R0CODE           0x0005
#define GDTSEL_R0DATA           0x0006

// GDTSEL_ADDR32 may be OR'd with above defines
#define GDTSEL_ADDR32           0x0080

u16t DevHelp_PageListToGDTSelector(u16t Selector, u32t Size, u16t Access, LIN pPageList);
#pragma aux DevHelp_PageListToGDTSelector = \
   "movzx   esp,sp"         \
   "mov     ecx,[esp]"      \
   "mov     dh,[esp+4]"     \
   "mov     edi,[esp+6]"    \
   "mov     dl,60h"         \
   "call    dword ptr [DevHelp]" \
   "jc      error"          \
   "sub     ax,ax"          \
"error:"                    \
   value [ax]               \
   parm caller []           \
   modify nomemory exact [ax cx dx di];

u16t DevHelp_PageListToLin(u32t Size, LIN pPageList, PLIN LinearAddr);
#pragma aux DevHelp_PageListToLin = \
   "movzx   esp,sp"         \
   "mov     ecx,[esp]"      \
   "mov     edi,[esp+4]"    \
   "mov     dl,5Fh"         \
   "call    dword ptr [DevHelp]" \
   "jc      error"          \
   "les     di,[esp+8]"     \
   "mov     es:[di],eax"    \
   "sub     ax,ax"          \
"error:"                    \
   value [ax]               \
   parm caller []           \
   modify exact [ax cx dx di];

/* NOTE: After the DevHelp call, SI contains the modified selector. However,
   the C interface has no provisions for returning this value to the caller.
   You can, however, use the _SI() inline function defined in include.h. */
u16t DevHelp_PhysToGDTSel(u32t PhysAddr, u32t Count, u16t Selector, u8t Access);
#pragma aux DevHelp_PhysToGDTSel = \
   "push    bp"             \
   "mov     dl,54h"         \
   "mov     bp,sp"          \
   "mov     eax,[bp+2]"     \
   "mov     ecx,[bp+6]"     \
   "mov     si,[bp+10]"     \
   "mov     dh,[bp+12]"     \
   "call    dword ptr [DevHelp]" \
   "jc      error"          \
   "sub     ax,ax"          \
"error:"                    \
   "pop     bp"             \
   value [ax]               \
   parm caller nomemory []  \
   modify nomemory exact [ax cx dx si];

u16t DevHelp_PhysToGDTSelector(u32t PhysAddr, u16t Count, u16t Selector);
#pragma aux DevHelp_PhysToGDTSelector = \
   "xchg    ax,bx"             \
   "mov     dl,2Eh"            \
   DEVHELP_CALL                \
   parm nomemory [ax bx] [cx] [si] \
   modify nomemory exact [ax bx dl];

#define SELTYPE_R3CODE      0
#define SELTYPE_R3DATA      1
#define SELTYPE_FREE        2
#define SELTYPE_R2CODE      3
#define SELTYPE_R2DATA      4
#define SELTYPE_R3VIDEO     5

u16t DevHelp_PhysToUVirt(u32t PhysAddr, u16t Length, u16t flags, u16t TagType, void far *SelOffset);
#pragma aux DevHelp_PhysToUVirt = \
   "xchg    ax,bx"          \
   "mov     dl,17h"         \
   "call    dword ptr [DevHelp]" \
   "jc      error"          \
   "push    es"             \
   "push    bx"             \
   "mov     bx,sp"          \
   "les     bx,ss:[bx+4]"   \
   "pop     es:[bx]"        \
   "pop     es:[bx+2]"      \
   "sub     ax,ax"          \
"error:"                    \
   value [ax]               \
   parm caller nomemory [bx ax] [cx] [dh] [si] [] \
   modify exact [ax bx dl es];

u16t DevHelp_PhysToVirt(u32t PhysAddr, u16t usLength, void far *SelOffset);
#pragma aux DevHelp_PhysToVirt = \
   "xchg    ax,bx"          \
   "mov     dx,15h"         \
   "push    ds"             \
   "call    dword ptr [DevHelp]" \
   "jc      error"          \
   "sub     ax,ax"          \
   "mov     es:[di],si"     \
   "mov     es:[di+2],ds"   \
"error:"                    \
   "pop  ds"                \
   value [ax]               \
   parm caller nomemory [bx ax] [cx] [es di] \
   modify exact [ax bx dx si];

u16t DevHelp_PostEventSem(u32t hEvent);
#pragma aux DevHelp_PostEventSem = \
   "mov     eax,[esp]"      \
   "mov     dl,69h"         \
   DEVHELP_CALL             \
   parm caller nomemory []  \
   modify nomemory exact [ax dl];

#define WAIT_IS_INTERRUPTABLE      0
#define WAIT_IS_NOT_INTERRUPTABLE  1

#define WAIT_INTERRUPTED           0x8003
#define WAIT_TIMED_OUT             0x8001

u16t DevHelp_ProcBlock(u32t EventId, u32t WaitTime, u16t IntWaitFlag);
#pragma aux DevHelp_ProcBlock = \
   "mov     dl,4"           \
   "xchg    ax,bx"          \
   "xchg    cx,di"          \
   "call    dword ptr [DevHelp]" \
   "jc      error"          \
   "mov     ax,0"           \
"error:"                    \
   value [ax]               \
   parm caller nomemory [ax bx] [di cx] [dh] \
   modify nomemory exact [ax bx cx dl di];

// return awake count!
u16t DevHelp_ProcRun(u32t EventId);
#pragma aux DevHelp_ProcRun = \
   "mov     dl,5"           \
   "xchg    ax,bx"          \
   "call    dword ptr [DevHelp]" \
   value [ax]               \
   parm caller nomemory [ax bx] \
   modify exact [ax bx dl];

u16t DevHelp_PullParticular(u8t near *Queue, PBYTE ReqPktAddr);
#pragma aux DevHelp_PullParticular= \
   "mov  dl,0Bh"            \
   DEVHELP_CALL             \
   parm [si] [es bx]        \
   modify exact [ax dl];

u16t DevHelp_PullRequest(u8t near *Queue, PBYTE far *ReqPktAddr);
#pragma aux DevHelp_PullRequest = \
   "push    es"             \
   "mov     dl,0Ah"         \
   "call    dword ptr [DevHelp]" \
   "jc      error"          \
   "movzx   esp,sp"         \
   "push    es"             \
   "push    bx"             \
   "mov     bx,sp"          \
   "les     bx,[esp]"       \
   "pop     es:[bx]"        \
   "pop     es:[bx+2]"      \
   "sub     ax,ax"          \
"error:"                    \
   value [ax]               \
   parm caller [si] []      \
   modify exact [ax bx dl es];

u16t DevHelp_PushRequest(u8t near *Queue, PBYTE ReqPktddr);
#pragma aux DevHelp_PushRequest = \
   "mov     dl,09h"         \
   "call    dword ptr [DevHelp]" \
   "sub     ax,ax"          \
   value [ax]               \
   parm [si] [es bx]        \
   modify exact [ax dl];

u16t DevHelp_QueueFlush(u8t near *Queue);
#pragma aux DevHelp_QueueFlush = \
   "mov     dl,10h"         \
   "call    dword ptr [DevHelp]" \
   "sub     ax,ax"          \
   value [ax]               \
   parm [bx]                \
   modify exact [ax dl];

typedef struct {
   u16t         QSize;
   u16t       QChrOut;
   u16t        QCount;
   u8t       Queue[1];
} QUEUEHDR, far *PQUEUEHDR;

u16t DevHelp_QueueInit(u8t near *Queue);
#pragma aux DevHelp_QueueInit = \
   "mov     dl,0Fh"         \
   "call    dword ptr [DevHelp]" \
   "sub     ax,ax"          \
   value [ax]               \
   parm [bx]                \
   modify exact [ax dl];

u16t DevHelp_QueueRead(u8t near *Queue, PBYTE Char);
#pragma aux DevHelp_QueueRead = \
   "mov     dl,12h"         \
   "call    dword ptr [DevHelp]" \
   "jc      error"          \
   "mov     es:[di],al"     \
   "sub     ax,ax"          \
"error:"                    \
   value [ax]               \
   parm [bx] [es di]        \
   modify exact [ax dl];

u16t DevHelp_QueueWrite(u8t near *Queue, u8t Char);
#pragma aux DevHelp_QueueWrite = \
   "mov     dl,11h"         \
   DEVHELP_CALL             \
   parm [bx] [al]           \
   modify exact [ax dl];

#define CHAIN_AT_TOP    0
#define CHAIN_AT_BOTTOM 1

u16t DevHelp_Register(u16t MonitorHandle, u16t MonitorPID, PBYTE InputBuffer,
                      u8t near *OutputBuffer, u8t ChainFlag);
#pragma aux DevHelp_Register = \
   "mov     dl,20h"         \
   DEVHELP_CALL             \
   parm caller nomemory [ax] [cx] [es si] [di] [dh] \
   modify nomemory exact [ax dl];

u16t DevHelp_RegisterBeep(void far *BeepHandler);
#pragma aux DevHelp_RegisterBeep = \
   "mov     dl,51h"         \
   "call    dword ptr [DevHelp]" \
   "sub     ax,ax"          \
   value [ax]               \
   parm caller nomemory [cx di] \
   modify nomemory exact [ax dl];

#define DEVICECLASS_ADDDM       1
#define DEVICECLASS_MOUSE       2

u16t DevHelp_RegisterDeviceClass(char near *DeviceString, void far *DriverEP,
                                 u16t DeviceFlags, u16t DeviceClass,
                                 u16t far *DeviceHandle);
#pragma aux DevHelp_RegisterDeviceClass = \
   "mov     dl,43h"         \
   "xchg    ax,bx"          \
   "call    dword ptr [DevHelp]" \
   "jc      error"          \
   "movzx   esp,sp"         \
   "les     bx,[esp]"       \
   "mov     es:[bx],ax"     \
   "sub     ax,ax"          \
"error:"                    \
   value [ax]               \
   parm caller [si] [ax bx] [di] [cx] [] \
   modify exact [ax bx dl es];

u16t DevHelp_RegisterPDD(char near *PhysDevName, void far *HandlerRoutine);
#pragma aux DevHelp_RegisterPDD = \
   "mov     dl,50h"         \
   DEVHELP_CALL             \
   parm caller [si] [es di] \
   modify nomemory exact [ax dl];

typedef struct {
   u16t          Size;
   u16t         Flags;
   u16t      IRQLevel;
   u16t      CLIStack;
   u16t      STIStack;
   u16t      EOIStack;
   u16t  NestingLevel;
} STACKUSAGEDATA;

u16t DevHelp_RegisterStackUsage(STACKUSAGEDATA near *StackUsageData);
#pragma aux DevHelp_RegisterStackUsage = \
   "mov     dl,3Ah"         \
   "call    dword ptr [DevHelp]" \
   "mov     ax,0"           \
   "sbb     ax,0"           \
   value [ax]               \
   parm caller [bx]         \
   modify nomemory exact [ax dl];

u16t DevHelp_RegisterTmrDD(void near *TimerEntry, u32t far *TmrRollover, 
                           u32t far *Tmr);
#pragma aux DevHelp_RegisterTmrDD = \
   "mov     dl,61h"         \
   "call    dword ptr [DevHelp]" \
   "mov     ax,bx"          \
   "movzx   esp,sp"         \
   "les     bx,[esp]"       \
   "mov     es:[bx],ax"     \
   "mov     es:[bx+2],di"   \
   "les     bx,[esp+4]"     \
   "mov     es:[bx],cx"     \
   "mov     es:[bx+2],di"   \
   "sub     ax,ax"          \
   value [ax]               \
   parm caller nomemory [di] [] \
   modify exact [ax bx cx di dl es];

u16t DevHlp_ResetEventSem(u32t hEvent, u32t far *pNumPosts);
#pragma aux DevHelp_ResetEventSem = \
   "movzx   esp,sp"         \
   "mov     eax,[esp]"      \
   "mov     edi,[esp+4]"    \
   "mov     dl,6Ah"         \
   DEVHELP_CALL             \
   parm caller nomemory []  \
   modify exact [ax di dl];

u16t DevHelp_ResetTimer(void near *TimerHandler);
#pragma aux DevHelp_ResetTimer = \
   "mov     dl,1Eh"         \
   DEVHELP_CALL             \
   parm caller nomemory [ax] \
   modify nomemory exact [ax dl];

typedef struct {
   u16t              MsgId;           // Message Id #
   u16t        cMsgStrings;           // # of (%) substitution strings
   char far *MsgStrings[1];           // Substitution string pointers
} MSGTABLE, __near *NPMSGTABLE;

#define MSG_REPLACEMENT_STRING  (1178)

u16t DevHelp_Save_Message(MSGTABLE near *MsgTable);
#pragma aux DevHelp_Save_Message = \
   "xor     bx,bx"          \
   "mov     dl,3Dh"         \
   DEVHELP_CALL             \
   parm [si]                \
   modify nomemory exact [ax bx dx];

u16t DevHelp_SchedClock(void near *SchedRoutineAddr);
#pragma aux DevHelp_SchedClock = \
   "mov     dl,0h"          \
   "call    dword ptr [DevHelp]" \
   "sub     ax,ax"          \
   value [ax]               \
   parm caller [ax]         \
   modify nomemory exact [ax dl];

u16t DevHelp_SemClear(u32t SemHandle);
#pragma aux DevHelp_SemClear = \
   "xchg    ax,bx"          \
   "mov     dl,7h"          \
   DEVHELP_CALL             \
   parm nomemory [ax bx]    \
   modify nomemory exact [ax bx dl];

#define SEMUSEFLAG_IN_USE       0
#define SEMUSEFLAG_NOT_IN_USE   1

u16t DevHelp_SemHandle(u32t SemKey, u16t SemUseFlag, u32t far *SemHandle);
#pragma aux DevHelp_SemHandle = \
   "xchg    ax,bx"          \
   "mov     dl,8h"          \
   "call    dword ptr [DevHelp]" \
   "jc      error"          \
   "mov     es:[si],bx"     \
   "mov     es:[si+2],ax"   \
   "sub     ax,ax"          \
"error:"                    \
   value [ax]               \
   parm nomemory [ax bx] [dh] [es si] \
   modify exact [ax bx dl];

u16t DevHelp_SemRequest(u32t SemHandle, u32t SemTimeout);
#pragma aux DevHelp_SemRequest = \
   "xchg    ax,bx"          \
   "xchg    di,cx"          \
   "mov     dl,06h"         \
   DEVHELP_CALL             \
   parm nomemory [ax bx] [cx di] \
   modify nomemory exact [ax bx cx di dl];

#define EVENT_MOUSEHOTKEY   0
#define EVENT_CTRLBREAK     1
#define EVENT_CTRLC         2
#define EVENT_CTRLNUMLOCK   3
#define EVENT_CTRLPRTSC     4
#define EVENT_SHIFTPRTSC    5
#define EVENT_KBDHOTKEY     6
#define EVENT_KBDREBOOT     7

u16t DevHelp_SendEvent(u16t EventType, u16t Parm);
#pragma aux DevHelp_SendEvent = \
   "mov     dl,25h"         \
   "call    dword ptr [DevHelp]" \
   "mov     ax,0"           \
   "sbb     ax,0"           \
   value [ax]               \
   parm nomemory [ah] [bx]  \
   modify nomemory exact [ax dl];

u16t DevHelp_SetIRQ(void near *IRQHandler, u16t IRQLevel, u16t SharedFlag);
#pragma aux DevHelp_SetIRQ = \
   "mov     dl,1Bh"         \
   DEVHELP_CALL             \
   parm caller nomemory [ax] [bx] [dh] \
   modify nomemory exact [ax dl];

u16t DevHelp_SetTimer(void near *TimerHandler);
#pragma aux DevHelp_SetTimer = \
   "mov     dl,1Dh"         \
   DEVHELP_CALL             \
   parm caller nomemory [ax] \
   modify nomemory exact [ax dl];

u16t DevHelp_SortRequest(u8t near *Queue, PBYTE ReqPktAddr);
#pragma aux DevHelp_SortRequest = \
   "mov     dl,0Ch"         \
   "call    dword ptr [DevHelp]" \
   "sub     ax,ax"          \
   value [ax]               \
   parm [si] [es bx]        \
   modify exact [ax dl];

u16t DevHelp_TCYield(void);
#pragma aux DevHelp_TCYield = \
   "mov     dl,3"           \
   "call    dword ptr [DevHelp]" \
   "sub     ax,ax"          \
   value [ax]               \
   parm caller nomemory []  \
   modify nomemory exact [ax dl];

u16t DevHelp_TickCount(void near *TimerHandler, u16t TickCount);
#pragma aux DevHelp_TickCount = \
   "mov     dl,33h"         \
   DEVHELP_CALL             \
   parm caller nomemory [ax] [bx] \
   modify nomemory exact [ax dl];

u16t DevHelp_UnLock(u32t LockHandle);
#pragma aux DevHelp_UnLock = \
   "xchg    ax,bx"          \
   "mov     dl,14h"         \
   DEVHELP_CALL             \
   parm nomemory [ax bx]    \
   modify nomemory exact [ax bx dl];

u16t DevHelp_UnSetIRQ(u16t IRQLevel);
#pragma aux DevHelp_UnSetIRQ = \
   "mov     dl,1Ch"         \
   DEVHELP_CALL             \
   parm caller nomemory [bx] \
   modify nomemory exact [ax dl];

#define VERIFY_READONLY    0
#define VERIFY_READWRITE   1

u16t DevHelp_VerifyAccess(u16t MemSelector, u16t Length, u16t MemOffset, u8t AccessFlag);
#pragma aux DevHelp_VerifyAccess = \
   "mov     dl,27h"         \
   DEVHELP_CALL             \
   parm caller nomemory [ax] [cx] [di] [dh] \
   modify nomemory exact [ax dl];

#define VIDEO_PAUSE_OFF            0
#define VIDEO_PAUSE_ON             1

u16t DevHelp_VideoPause(u16t OnOff);
#pragma aux DevHelp_VideoPause = \
   "mov     dl,3Ch"         \
   "call    dword ptr [DevHelp]" \
   "mov     ax,0"           \
   "sbb     ax,0"           \
   value [ax]               \
   parm nomemory [al]       \
   modify nomemory exact [ax dl];

u16t DevHelp_VirtToLin(u16t Selector, u32t Offset, PLIN LinearAddr);
#pragma aux DevHelp_VirtToLin = \
   "movzx   esp,sp"         \
   "mov     esi,[esp]"      \
   "mov     dl,5bh"         \
   "call    dword ptr [DevHelp]" \
   "jc      error"          \
   "les     bx,[esp+4]"     \
   "mov     es:[bx],eax"    \
   "sub     ax,ax"          \
"error:"                    \
   value [ax]               \
   parm caller nomemory [ax] [] \
   modify exact [ax si es bx dl];

u16t DevHelp_VirtToPhys(void far *SelOffset, u32t far *PhysAddr);
#pragma aux DevHelp_VirtToPhys = \
   "push    ds"             \
   "mov     dl,16h"         \
   "push    es"             \
   "mov     si,ds"          \
   "mov     es,si"          \
   "mov     ds,bx"          \
   "mov     si,ax"          \
   "call    dword ptr es:[DevHelp]" \
   "pop     es"             \
   "mov     es:[di],bx"     \
   "mov     es:[di+2],ax"   \
   "pop     ds"             \
   "sub     ax,ax"          \
   value [ax]               \
   parm caller nomemory [ax bx] [es di] \
   modify exact [ax bx dl es si];

#define VMDHA_16M               0x0001
#define VMDHA_FIXED             0x0002
#define VMDHA_SWAP              0x0004
#define VMDHA_CONTIG            0x0008
#define VMDHA_PHYS              0x0010
#define VMDHA_PROCESS           0x0020
#define VMDHA_SGSCONT           0x0040
#define VMDHA_RESERVE           0x0100
#define VMDHA_USEHIGHMEM        0x0800

u16t DevHelp_VMAlloc(u32t Flags, u32t Size, u32t PhysAddr, PLIN LinearAddr, 
                     u32t far *SelOffset);
#pragma aux DevHelp_VMAlloc = \
   "movzx   esp,sp"         \
   "mov     eax,[esp]"      \
   "mov     ecx,[esp+4]"    \
   "mov     edi,[esp+8]"    \
   "mov     dl,57h"         \
   "call    dword ptr [DevHelp]" \
   "jc      error"          \
   "les     di,[esp+12]"    \
   "mov     es:[di],eax"    \
   "les     di,[esp+16]"    \
   "mov     es:[di],ecx"    \
   "sub     ax,ax"          \
"error:"                    \
   value [ax]               \
   parm caller nomemory []  \
   modify exact [ax cx di dl es];

u16t DevHelp_VMFree(LIN LinearAddr);
#pragma aux DevHelp_VMFree = \
   "movzx   esp,sp"         \
   "mov     dl,58h"         \
   "mov     eax,[esp]"      \
   DEVHELP_CALL             \
   parm caller nomemory []  \
   modify nomemory exact [ax dl];

#define VMDHGP_WRITE            0x0001
#define VMDHGP_SELMAP           0x0002
#define VMDHGP_SGSCONTROL       0x0004
#define VMDHGP_4MEG             0x0008

u16t DevHelp_VMGlobalToProcess(u32t Flags, LIN LinearAddr, u32t Length,
                               PLIN ProcessLinearAddr);
#pragma aux DevHelp_VMGlobalToProcess = \
   "mov     dl,5Ah"         \
   "movzx   esp,sp"         \
   "mov     eax,[esp]"      \
   "mov     ebx,[esp+4]"    \
   "mov     ecx,[esp+8]"    \
   "call    dword ptr [DevHelp]" \
   "jc      error"          \
   "les     bx,[esp+12]"    \
   "mov     es:[bx],eax"    \
   "sub     ax,ax"          \
"error:"                    \
   value [ax]               \
   parm caller nomemory []  \
   modify exact [ax bx cx dl es];

#define VMDHL_NOBLOCK           0x0001
#define VMDHL_CONTIGUOUS        0x0002
#define VMDHL_16M               0x0004
#define VMDHL_WRITE             0x0008
#define VMDHL_LONG              0x0010
#define VMDHL_VERIFY            0x0020

u16t DevHelp_VMLock(u32t Flags, LIN LinearAddr, u32t Length, LIN pPagelist,
                    LIN pLockHandle, u32t far *PageListCount);
#pragma aux DevHelp_VMLock = \
   "mov     dl,55h"         \
   "movzx   esp,sp"         \
   "mov     eax,[esp]"      \
   "mov     ebx,[esp+4]"    \
   "mov     ecx,[esp+8]"    \
   "mov     edi,[esp+12]"   \
   "mov     esi,[esp+16]"   \
   "call    dword ptr [DevHelp]" \
   "jc      error"          \
   "les     bx,[esp+20]"    \
   "mov     es:[bx],eax"    \
   "sub     ax,ax"          \
"error:"                    \
   value [ax]               \
   parm caller nomemory []  \
   modify exact [ax bx cx dl si di es];

#define VMDHPG_READONLY         0x0000
#define VMDHPG_WRITE            0x0001

u16t DevHelp_VMProcessToGlobal(u32t Flags, LIN LinearAddr, u32t Length,
                               PLIN GlobalLinearAddr);
#pragma aux DevHelp_ProcessToGlobal = \
   "mov     dl,59h"         \
   "movzx   esp,sp"         \
   "mov     eax,[esp]"      \
   "mov     ebx,[esp+4]"    \
   "mov     ecx,[esp+8]"    \
   "call    dword ptr [DevHelp]" \
   "jc      error"          \
   "les     bx,[esp+12]"    \
   "mov     es:[bx],eax"    \
   "sub     ax,ax"          \
"error:"                    \
   value [ax]               \
   parm caller nomemory []  \
   modify exact [ax bx cx dl es];

#define VMDHS_DECOMMIT          0x0001
#define VMDHS_RESIDENT          0x0002
#define VMDHS_SWAP              0x0004

u16t DevHelp_VMSetMem(LIN LinearAddr, u32t Size, u32t Flags);
#pragma aux DevHelp_VMSetMem = \
   "mov     dl,66h"         \
   "movzx   esp,sp"         \
   "mov     ebx,[esp]"      \
   "mov     ecx,[esp+4]"    \
   "mov     eax,[esp+8]"    \
   DEVHELP_CALL             \
   parm caller nomemory []  \
   modify nomemory exact [ax bx cx dl];

u16t DevHelp_VMUnLock(LIN pLockHandle);
#pragma aux DevHelp_VMUnLock = \
   "mov     dl,56h"         \
   "movzx   esp,sp"         \
   "mov     esi,[esp]"      \
   DEVHELP_CALL             \
   parm caller []           \
   modify exact [ax si dl];

u16t DevHelp_Yield(void);
#pragma aux DevHelp_Yield = \
   "mov     dl,2"           \
   "call    dword ptr [DevHelp]" \
   "sub     ax,ax"          \
   value [ax]               \
   parm caller nomemory []  \
   modify nomemory exact [ax dl];

// DevHlp Error Codes
#define MSG_MEMORY_ALLOCATION_FAILED    0x00
#define ERROR_LID_ALREADY_OWNED         0x01
#define ERROR_LID_DOES_NOT_EXIST        0x02
#define ERROR_ABIOS_NOT_PRESENT         0x03
#define ERROR_NOT_YOUR_LID              0x04
#define ERROR_INVALID_ENTRY_POINT       0x05

u32t DevHelp_GetDSLin(void);
#pragma aux DevHelp_GetDSLin = \
   "mov     ax,ds"          \
   "xor     esi,esi"        \
   "mov     dl,5Bh"         \
   "call    dword ptr [DevHelp]" \
   "jnc     noerror"        \
   "xor     eax,eax"        \
"noerror:"                  \
   "shld    edx,eax,16"     \
   value [dx ax]            \
   modify exact [ax si dx];

u16t DevHelp_OpenFile(char far *FileName, u32t far *FileSize);
#pragma aux DevHelp_OpenFile = \
   "mov     dl,7Fh"         \
   "sub     sp,10"          \
   "movzx   esp,sp"         \
   "mov     word ptr [esp],8" \
   "mov     eax,[esp+10]"   \
   "mov     [esp+2],eax"    \
   "mov     ax,ss"          \
   "mov     es,ax"          \
   "mov     di,sp"          \
   "call    dword ptr [DevHelp]" \
   "jc      error"          \
   "mov     edx,[esp+6]"    \
   "les     di,[esp+14]"    \
   "mov     es:[di],edx"    \
   "xor     ax,ax"          \
"error:"                    \
   "add     sp,10"          \
   value [ax]               \
   parm caller []           \
   modify exact [ax dx di es];

u16t DevHelp_ReadFile(void far *Buffer, u32t Length);
#pragma aux DevHelp_ReadFile = \
   "mov     dl,81h"         \
   "push    8"              \
   "mov     ax,ss"          \
   "mov     es,ax"          \
   "mov     di,sp"          \
   "call    dword ptr [DevHelp]" \
   "jc      error"          \
   "xor     ax,ax"          \
"error:"                    \
   "add     sp,2"           \
   value [ax]               \
   parm caller []           \
   modify exact [ax dx di es];

u16t DevHelp_ReadFileAt(void far *Buffer, u32t Length, u32t Position);
#pragma aux DevHelp_ReadFileAt = \
   "mov     dl,82h"         \
   "push    12"             \
   "mov     ax,ss"          \
   "mov     es,ax"          \
   "mov     di,sp"          \
   "call    dword ptr [DevHelp]" \
   "jc      error"          \
   "xor     ax,ax"          \
"error:"                    \
   "add     sp,2"           \
   value [ax]               \
   parm caller []           \
   modify exact [ax dx di es];

void DevHelp_CloseFile(void);
#pragma aux DevHelp_CloseFile = \
   "mov     dl,80h"         \
   "push    0"              \
   "push    2"              \
   "mov     ax,ss"          \
   "mov     es,ax"          \
   "mov     di,sp"          \
   "call    dword ptr [DevHelp]" \
   "add     sp,4"           \
   parm caller nomemory []  \
   modify exact [ax dx di es];

#ifdef __cplusplus
}
#endif

#endif // QS_DEVHELP_H
