//
// QSINIT API
// ini files support class
//
#ifndef QSINIT_INIFILE_SC
#define QSINIT_INIFILE_SC

#include "qstypes.h"
#include "qsclass.h"
#include "qsshell.h"
#include "qsio.h"

#ifdef __cplusplus
extern "C" {
#endif

/// @name ini open mode
//@{
#define QSINI_READONLY     0x0001        ///< do not flush changes back to the file
#define QSINI_EXCLUSIVE    0x0002        ///< lock write access to file on disk
#define QSINI_EXISTING     0x0004        ///< existing file only
//@}

/// @name get section text flags
//@{
#define GETSEC_NOEMPTY        1  ///< do not return empty and commented lines (";" at 1st pos)
#define GETSEC_NOEKEY         2  ///< do not return lines with empty key
#define GETSEC_NOEVALUE       4  ///< do not return lines with empty value
//@}

/** ini file access shared class.
    */
typedef struct qs_inifile_s {
   /** return complete section of ini file.
       @param   fname     File name
       @param   mode      Open mode (QSINI_*). Note, that single QSINI_READONLY
                          file will return no error if file is not exist.
       @return 0 on success or error value */
   qserr      _exicc (*open)     (const char *fname, u32t mode);
   void       _exicc (*create)   (str_list *text);

   qserr      _exicc (*close)    (void);

   qserr      _exicc (*fstat)    (io_direntry_info *rc);
   /// save file copy
   qserr      _exicc (*savecopy) (const char *fname);
   /** flush current file state to disk file.
       @param   force     Force write if file was changed by external
                          program (ignored now) */
   qserr      _exicc (*flush)    (int force);

   void       _exicc (*setstr)   (const char *sec, const char *key, const char *value);
   void       _exicc (*setint)   (const char *sec, const char *key, s32t value);
   void       _exicc (*setuint)  (const char *sec, const char *key, u32t value);
   void       _exicc (*seti64)   (const char *sec, const char *key, s64t value);
   void       _exicc (*setui64)  (const char *sec, const char *key, u64t value);
   /** read string value.
       Note, what function returns heap block in any case, i.e. "def" value
       is duplicated too.
       Function can return 0 if def is 0 and no key.
       @return string or default value string in process heap block (must be free()-d) */
   char*      _exicc (*getstr)   (const char *sec, const char *key, const char *def);
   s32t       _exicc (*getint)   (const char *sec, const char *key, s32t def);
   u32t       _exicc (*getuint)  (const char *sec, const char *key, u32t def);
   s64t       _exicc (*geti64)   (const char *sec, const char *key, s64t def);
   u64t       _exicc (*getui64)  (const char *sec, const char *key, u64t def);
   /** return uppercased or original section names list.
       @param   org       Return original names (1) or uppercased (0).
       @return string list, must be free(d). */
   str_list*  _exicc (*sections) (int org);

   /** query section presence in ini file.
       @param   sec       Section name
       @param   flags     Ignore sections with empty values (GETSEC_* flags)
       @return presence flag (1/0) */
   int        _exicc (*secexist) (const char *sec, u32t flags);
   /// query key presence
   int        _exicc (*exist)    (const char *sec, const char *key);

   /** return complete section of ini file.
       @param   sec       Section name
       @param   flags     Options to ignore empty values (GETSEC_*)
       @return string list */
   str_list*  _exicc (*getsec)   (const char *sec, u32t flags);
   /** return list of keys in specified section.
       @param       sec     Section name
       @param [out] values  List of values for returning keys (can be 0)
       @return list or 0 */
   str_list*  _exicc (*keylist)  (const char *sec, str_list**values);
   /** return entire file text.
       @return string list, must be free(d). */
   str_list*  _exicc (*text)     (void);

   /** write complete section text.
       @param   sec       Section name
       @param   text      Section text (without header) */
   void       _exicc (*secadd)   (const char *sec, str_list *text);

   void       _exicc (*secerase) (const char *sec);
   void       _exicc (*erase)    (const char *sec, const char *key);
} _qs_inifile, *qs_inifile;

#ifdef __cplusplus
}
#endif

#endif // QSINIT_INIFILE_SC
