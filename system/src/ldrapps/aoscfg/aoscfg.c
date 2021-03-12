//
// QSINIT "os2cfg" executable
// ArcaLoader os2ldr.cfg parameters partial apply
//
#include "qsbase.h"
#include "stdlib.h"
#include "qsint.h"
#include "qcl/qsvdmem.h"

#define RUN_ONCE_KEY    "AOSCFG_DONE"

/* use common shell processing for all commands - this prevent static linking
   of EXTCMD or VDISK modules for shl_ramdisk/shl_mem functions. */
static void shell_cmd(const char *cmd, const char *args) {
   char   *cstr = strcat_dyn(sprintf_dyn(cmd), args);
   cmd_state cs = cmd_init(cstr, 0);
   cmd_run(cs, CMDR_ECHOOFF);
   cmd_close(cs);
   free(cstr);
}

static u32t get_flag_value(str_list *cfg, const char *key, u32t def) {
   char *value = str_findkey(cfg, key, 0), *uv, *uvp;
   u32t    res;
   if (!value) return def;
   uvp = strdup(value);
   uv  = trimleft(uvp, " \t");
   uv  = trimright(uv, " \t");
   if (*uv==0) res = def; else {
      strupr(uv);
      if (strcmp(uv,"TRUE")==0 || strcmp(uv,"ON")==0 || strcmp(uv,"YES")==0)
         res = 1; else
      if (strcmp(uv,"FALSE")==0 || strcmp(uv,"OFF")==0 || strcmp(uv,"NO")==0)
         res = 0; else
      res = str2ulong(uv);
   }
   free(uvp);
   return res;
}

int main(int argc, char *argv[]) {
   u32t       flen;
   void     *fdata;
   str_list   *cfg;
   char     *value;
   // just exit ;)
   if (hlp_hosttype()==QSHT_EFI) return 3;
   // check "we are called" flag
   if (sto_dword(RUN_ONCE_KEY)) {
      printf("Second configuration check call is not required.\n");
      return 2;
   }
   // read cfg file, be quiet if failed
   fdata = hlp_freadfull("OS2LDR.CFG", &flen, 0);
   if (!fdata) return 1;

   cfg   = str_settext((char*)fdata, flen, 0);
   hlp_memfree(fdata);
   // save non-zero value to flag "we are called at least once"
   sto_savedword(RUN_ONCE_KEY, 1);
   // process config keys
   value = str_findkey(cfg, "HIDEMEM", 0);
   if (value) shell_cmd("mem hide ", value);
   // limit can be already set by VMTRR or someone else, check this too
   value = str_findkey(cfg, "MEMLIMIT", 0);
   if (value) {
      u32t limit = str2ulong(value);
      if (limit) {
         u32t oldv = sto_dword(STOKEY_MEMLIM);
         if (!oldv || limit<oldv) sto_savedword(STOKEY_MEMLIM, limit);
      }
   }
   // bootos2.exe will use this key when found no LOGSIZE parameter
   value = str_findkey(cfg, "LOGSIZE", 0);
   if (value) {
      u32t logsize = str2ulong(value);
      if (logsize>=64) sto_savedword(STOKEY_LOGSIZE, logsize); else
         log_printf("aoscfg: logsize too small (%u)!\n", logsize);
   }
   // apply DBPORT & DBCARD only if NO port selected
   if (hlp_seroutinfo(0)==0) {
      value = str_findkey(cfg, "DBCARD", 0);
      if (value) shell_cmd("dbcard ", value); else {
         value = str_findkey(cfg, "DBPORT", 0);
         if (!value) value = str_findkey(cfg, "DBGPORT", 0);
         if (value) {
            u32t port = str2ulong(value);
            if (port) hlp_seroutset(port, 0);
         }
      }
   }
   // ACPI reset can be used
   if (get_flag_value(cfg, "ACPIRESET", 0)) sto_savedword(STOKEY_ACPIRST, 1);
   // save dump flags for BOOTOS2
   value = str_findkey(cfg, "DUMP_ORDER", 0);
   if (value) sto_save(STOKEY_DUMPSEQ, value, strlen(value)+1, 1);
   if (get_flag_value(cfg, "DUMP_SEARCH", 0)) sto_savedword(STOKEY_DUMPSRCH, 1);

   // ramdisk creation (only if not exist one)
   value = str_findkey(cfg, "RAMDISKNAME", 0);
   // this setenv affects only RAMDISK shell command below
   if (value) setenv("VDISKNAME", value, 1);

   value = str_findkey(cfg, "RAMDISK", 0);
   if (value) {
      qs_vdisk  dsk = NEW(qs_vdisk);
      if (dsk) {
         u32t sectors;
         qserr    res = dsk->query(0, 0, &sectors, 0, 0);
         DELETE(dsk);
         if (!res) {
            log_printf("aoscfg: ramdisk already exists (%uMb)!\n", sectors>>11);
         } else {
            shell_cmd("ramdisk ", value);
         }
      } else
         log_printf("aoscfg: ramdisk service is missing\n");
   }
   free(cfg);

   return 0;
}
