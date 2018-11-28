//
// QSINIT API
// signal header
//
#ifndef QSINIT_SIGNAL
#define QSINIT_SIGNAL

#ifdef __cplusplus
extern "C" {
#endif

#include "stdlib.h"

typedef int sig_atomic_t;
typedef void (_std *__sig_function)(int);

#define SIG_DFL      ((__sig_function) 0)        /* default signal action */
#define SIG_IGN      ((__sig_function) 1)        /* ignore signal */
#define SIG_GET      ((__sig_function) 2)        /* return current value */
#define SIG_ERR      ((__sig_function)-1)

#define SIGINT        2          // Ctrl-C on keyboard
#define SIGABRT       6          // abnormal termination (abort() call)
#define SIGKILL       9          // process kill
#define SIGALRM      14          // alarm() signal
#define SIGUSR1      16
#define SIGUSR2      17
#define SIGUSR3      18

/** signal function.
    Some notes about signal processing in QSINIT:

    * signal() function works in non-MT mode, but signals itself - does not!

    * signal() settings are unique for every thread.

    * any call to kill(), tkill() and alarm() will turn MT mode ON

    * kill() will send signal to the main thread of specified process, tkill()
      to a thread in current process.

    * signal handler called in the *separate fiber*. Signal delivery will be
      delayed if thread is in the waiting state or current fiber is not 0
      (main thread stream).

    * siglongjmp() MUST be used instead of longjmp() in the signal handler.

    * default processing reacts on SIGINT, SIGKILL and SIGABRT only. It
      terminates process in the main thread and thread in secondary. */
__sig_function _std signal(int sig, __sig_function);

/** send signal to a process or process tree
    Function turns on MT mode in any case (even with sig==0).

    Sig==0 cannot check pid - because error will be returned too if main thread
    of target is suspended or waits for something. mod_checkpid() should be used
    to validate pid.

    @param  pid      process id, pid>0 - specified one, 0 - current and all of
                     its children, pid==-1 - any process except pid 1,
                     pid<-1 - -pid and all of its children.
    @param  sig      signal number, sig==0 can be used to validate ability
                     to send a signal.
    @return 0 on success and -1 on error (with errno set) */
int            _std kill  (int pid, int sig);

/** thread signal.
    @param  tid      thread ID
    @param  sig      signal
    @return 0 on success. On error, -1 is returned, and errno is set appropriately */
int            _std tkill (u32t tid, int sig);

#define raise(sig) kill(getpid(),(sig))

/** posix alarm function.
    Note, that this function hiddenly turns on MT mode, because signal
    processing requires it mandatory.

    Alarm timer is unique for every thread.

    @param  seconds  interval before SIGALRM, use 0 to remove installed one.
    @return # of seconds until the previous request would have generated
            a SIGALRM signal, 0 if no one before */
unsigned int   _std alarm (unsigned int seconds);

#ifdef __cplusplus
}
#endif
#endif // QSINIT_SIGNAL
