//
// QSINIT API
// exception handling
//
#ifndef QSINIT_XCPT
#define QSINIT_XCPT

/* _try_ {
      //...
      _ret_in_try_(1);
   }
   _catch_(xcpt_divide0) {
      //...
   }
   _alsocatch_(xcpt_align) {
      //...
   }
   _endcatch_ 
   

   Exceptions 8, 12, 13 will hang Virtual PC - because of task gate use.
   Both VBox and hardware PC works fine.
*/

#ifdef __cplusplus
extern "C" {
#endif
#include "qstypes.h"
#include "setjmp.h"

typedef enum {
           xcpt_divide0  = 0,               ///< divide-by-zero exception
           xcpt_step     = 1,               ///< single-step interrupt
           xcpt_nmi      = 2,               ///< NMI interrupt
           xcpt_bp       = 3,               ///< breakpoint interrupt
           xcpt_overflow = 4,               ///< overflow exception
           xcpt_bound    = 5,               ///< bound exception
           xcpt_opcode   = 6,               ///< invalid opcode
           xcpt_nofpu    = 7,               ///< FP unit not present
           xcpt_double   = 8,               ///< double exception
           xcpt_tss     = 10,               ///< invalid TSS
           xcpt_segment = 11,               ///< segment not present
           xcpt_stack   = 12,               ///< stack fault
           xcpt_gpfault = 13,               ///< general protection fault
           xcpt_pgfault = 14,               ///< page fault
           xcpt_fpuerr  = 16,               ///< floating-point error
           xcpt_align   = 17,               ///< alignment exception
           xcpt_machine = 18,               ///< machine check exception
           xcpt_simd    = 19,               ///< SSEx exception
           xcpt_interr  = 0xFFFC,           ///< exit called in start module (int.error)
           xcpt_hookerr = 0xFFFD,           ///< ebp changed in chained call
           xcpt_invalid = 0xFFFE,           ///< invalid call to _currentexception_
           xcpt_all     = 0xFFFF
        } sys_xcptype;

typedef struct { u32t reserved[20]; } sys_xcpt;

#define _try_                                              \
   {                                                       \
      volatile static sys_xcpt exc;                        \
      sys_exfunc1(&exc, __FILE__, __LINE__);               \
      if (_setjmp(sys_exfunc6(&exc))==0)                   \
      {

#define _catch_(typ)                                       \
         sys_exfunc3(&exc);                                \
      } else {                                             \
         sys_exfunc2(&exc);                                \
         if ((typ)==xcpt_all || sys_exfunc7(&exc)==(typ))  \
         {

#define _alsocatch_(typ)                                   \
         } else                                            \
         if ((typ)==xcpt_all || sys_exfunc7(&exc)==(typ))  \
         {

#define _endcatch_                                         \
         } else                                            \
            sys_exfunc5(&exc, __FILE__,__LINE__);          \
      }                                                    \
   }

#define _ret_in_try_(ret)    { sys_exfunc3(&exc); return(ret); }
#define _retvoid_in_try_()   { sys_exfunc3(&exc); return; }

/** Throw raises an exception by passing control and the exception type
    to the exception handler. Control does not return to the location where
    the error occurred.
    If an exception is not processed by the try block in which it was raised */
#define _throw_(type)          sys_exfunc4(type, __FILE__, __LINE__)

/** Rethrow throws the exception from inside the catch block to the
    outer try block for continued processing. If it is not processed by the
    outer try block, it is thrown to the next outer try block, and so on until
    it is caught by a catch block or the system intervenes */
#define _rethrow_()            sys_exfunc5(&exc, __FILE__, __LINE__)

#define _currentexception_()   sys_exfunc7(&exc)

void     _std sys_exfunc1(volatile sys_xcpt* except, const char* file, u32t line);
void     _std sys_exfunc2(volatile sys_xcpt* except);
void     _std sys_exfunc3(volatile sys_xcpt* except);
void     _std sys_exfunc4(sys_xcptype type, const char* file, u32t line);
void     _std sys_exfunc5(volatile sys_xcpt* except, const char* file, u32t line);
jmp_buf* _std sys_exfunc6(volatile sys_xcpt* except);
u32t     _std sys_exfunc7(volatile sys_xcpt* except);

/** set DR0..DR3 access breakpoint.
    It will trap to default trap screen, but can show eip, where memory was broken.
    @param   access  Address to catch
    @param   index   Register index (0..3)
    @param   size    Size (1,2,4). Use 1 for exec.
    @param   type    Access type (0:exec, 1:write, 3:read) */
void     _std sys_errbrk(void *access, u8t index, u8t size, u8t type);

#ifdef __cplusplus
}
#endif
#endif // QSINIT_XCPT
