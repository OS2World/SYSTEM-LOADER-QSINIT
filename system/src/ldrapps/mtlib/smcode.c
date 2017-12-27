//
// QSINIT
// session specific code
// ------------------------------------------------------------
//
#include "mtlib.h"

// warning! called from interrupt!
u32t key_available(u32t sno) {
   se_sysdata *sd = se_dataptr(sno);
   return sd ? sd->vs_keybuf[sd->vs_keyrp] : 0;
}

qserr _std mod_execse(u32t mod, const char *env, const char *params,
   u32t flags, u32t vdev, mt_pid *ppid, const char *title)
{
   module *mi = (module*)mod;
   qserr  res = 0;
   if (!mt_on) {
      res = mt_initialize();
      if (res) return res;
   }
   if (!mod || (flags&~(QEXS_DETACH|QEXS_WAITFOR|QEXS_BOOTENV|QEXS_BACKGROUND))
      || (flags&QEXS_DETACH) && vdev) return E_SYS_INVPARM;
   // check device bits
   if (vdev && (vdev&se_devicemask())!=vdev) return E_CON_NODEVICE;

   mt_swlock();
   if (mi->sign!=MOD_SIGN) res = E_SYS_INVOBJECT; else
   if (mi->flags&MOD_EXECPROC) res = E_MOD_EXECINPROC; else
   if (mi->flags&MOD_LIBRARY) res = E_MOD_LIBEXEC; else {
      int         eres;
      char    *envdata = 0;
      se_start_data ld;
      // here we safe again
      mt_swunlock();
      // create default environment
      if (!env) envdata = env_copy(flags&QEXS_BOOTENV?pq_qsinit:mod_context(), 0);
      // and launch it
      ld.sign  = SEDAT_SIGN;
      ld.flags = flags;
      ld.pid   = 0;
      ld.vdev  = vdev;
      ld.title = 0;
      if (title) {
         ld.title = strdup(title);
         // string will be released by mt_launch()
         mem_modblock(ld.title);
      }
      eres     = mod_exec(mod, env?env:envdata, params, &ld);
      res      = eres<0?E_SYS_SOFTFAULT:0;
      if (!res && ppid) *ppid = ld.pid;
      if (envdata) free(envdata);
   }
   return res;
}
