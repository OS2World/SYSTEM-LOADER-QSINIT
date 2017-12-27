//
// QSINIT API
// base header
//
#ifndef QS_BASE_H
#define QS_BASE_H

#include "qstypes.h"
#include "qserr.h"

#ifndef QSBASE_NOIO
#include "../ldrapps/hc/qsio.h"
#endif
#ifndef QSBASE_NOLOG
#include "../ldrapps/hc/qslog.h"
#endif
#ifndef QSBASE_NOSYS
#include "../ldrapps/hc/qssys.h"
#endif
#ifndef QSBASE_NOMT
#include "../ldrapps/hc/qstask.h"
#endif
#ifndef QSBASE_NOTM
#include "../ldrapps/hc/qstime.h"
#endif
#ifndef QSBASE_NOEXI
#include "../ldrapps/hc/qsclass.h"
#endif
#ifndef QSBASE_NOVIO
#include "../ldrapps/hc/vio.h"
#endif
#ifndef QSBASE_NOEXCPT
#include "../ldrapps/hc/qsxcpt.h"
#endif
#ifndef QSBASE_NOREG
#include "../ldrapps/hc/qsstor.h"
#endif
#ifndef QSBASE_NOUTIL
#include "../ldrapps/hc/qsutil.h"
#endif
#ifndef QSBASE_NOSHELL
#include "../ldrapps/hc/qsshell.h"
#endif
#ifndef QSBASE_NOHDM
#include "../ldrapps/hc/qshm.h"
#endif
#ifndef QSBASE_NOPAGEM
#include "../ldrapps/hc/qspage.h"
#endif
#ifndef QSBASE_NOMOD
#include "../ldrapps/hc/qsmod.h"
#endif
#ifndef QSBASE_NOSESMGR
#include "../ldrapps/hc/qscon.h"
#endif

#endif // QS_BASE_H
