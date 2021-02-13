#include "qsbase.h"
#include "stdlib.h"
#include "direct.h"
#include "errno.h"
#include "qcl/qsinif.h"

u32t _std shl_setini(const char *cmd, str_list *args) {
   int  rc=-1, pos=0, bootio=0, quiet=0, elvl = 0;
   // help overrides any other keys
   if (str_findkey(args, "/?", 0)) {
      cmd_shellhelp(cmd,CLR_HELP);
      return 0;
   }
   while (pos<args->count)
      if (stricmp(args->item[pos],"/boot")==0) { bootio=1; pos++; } else
         if (stricmp(args->item[pos],"/q")==0) { quiet=1; pos++; }
            else break;
   // at least 3 args still should be line
   if (args->count-pos>=3) {
      static const
      char  *cmdlist[] = { "APPEND", "DEL", "ERASE", "GET", "INSERT",
                           "KEY", "SEC", "SET", 0 };
      char    *ininame = args->item[pos++],
                  *cmd = args->item[pos++];
      int          idx = 0, ro_mode;

      while (1)
         if (cmdlist[idx]==0) { idx=-1; break; } else
            if (stricmp(cmd,cmdlist[idx])==0) break; else idx++;

      ro_mode = idx==3 || idx==5 || idx==6;

      if (idx<0) rc = EINVAL; else
      // boot i/o has no write access
      if (bootio && !ro_mode) rc = EACCES; else {
         int plainmode = 0,
                remarg = args->count-pos;
         /* we should have here at least one arg, so check and detect for
            "too many args" and "plain" (sectionless) file mode */
         switch (idx) {
            case 0:  // APPEND [section] string
            case 1:  // DEL    [section] key
            case 4:  // INSERT [section] string
            case 5:  // KEY    [section] key
               if (remarg>2) rc = EINVAL; else
                  if (remarg==1) plainmode = 1;
               break;
            case 2:  // ERASE  section
            case 6:  // SEC    section
               if (remarg!=1) rc = EINVAL;
               break;
            case 3:  // GET    [section] key varname
            case 7:  // SET    [section] key value
               if (remarg<2 || remarg>3) rc = EINVAL; else
                  if (remarg==2) plainmode = 1;
               break;
         }

         if (rc<=0) {
            str_list *text = 0;
            qs_inifile ifl = plainmode ? 0 : NEW(qs_inifile);
            char  *secname;
            
            if (bootio) {
               u32t fsz;
               void *fd = hlp_freadfull(ininame, &fsz, 0);
               if (!fd) rc = ENOENT; else {
                  text = str_settext((char*)fd, fsz);
                  hlp_memfree(fd);

                  if (ifl) {
                     ifl->create(text);
                     free(text);
                     text = 0;
                  }
               }
            } else {
               int newfile = !idx || idx==4 || idx==7;
               if (plainmode) {
                  u32t fsz;
                  void *fd = freadfull(ininame, &fsz);
                  if (fd) {
                     text = str_settext((char*)fd, fsz);
                     hlp_memfree(fd);
                  } else
                  if (newfile) {
                     text = (str_list*)malloc_th(sizeof(str_list));
                     text->count = 0;
                  } else
                     rc = ENOENT;
               } else {
                  qserr res = ifl->open(ininame, (ro_mode?QSINI_READONLY:0) |
                                        (newfile?0:QSINI_EXISTING));
                  rc = qserr2errno(res);
               }
            }

            secname = plainmode ? 0 : args->item[pos++];
            // pos points to 1st arg after "section"
            
            switch (idx) {
               case 0:  // APPEND
               case 4:  // INSERT
                  if (rc<=0) {
                     str_list *st = plainmode ? text: ifl->getsec(secname,0);
                     char   *wstr = strdup(args->item[pos]);

                     // str_newentry() wants \0\0 at the end of string
                     wstr = strcat_dyn(wstr, "\1");
                     wstr[strlen(wstr)-1] = 0;

                     st = str_newentry (st, idx?0:st->count, wstr, 0, 1);
                     free(wstr);

                     if (!plainmode) {
                        ifl->secadd(secname, st);
                        free(st);
                     } else
                        text = st;
                  }
                  break;
               case 1:  // DEL
                  if (rc<=0)
                     if (plainmode) {
                        u32t keypos = 0;
                        if (str_findkey(text, args->item[pos], &keypos))
                           str_delentry(text, keypos);
                     } else
                        ifl->erase(secname, args->item[pos]);
                  break;
               case 2:  // ERASE
                  if (rc<=0 && ifl) ifl->secerase(secname);
                  break;
               case 3: { // GET
                  char *value = 0;
                  if (rc<=0)
                     if (plainmode)
                        value = str_findkey(text, args->item[pos], 0);
                     else
                        value = ifl->getstr(secname, args->item[pos], 0);

                  setenv(args->item[pos+1], value,1);

                  if (!plainmode && value) free(value);
                  break;
               }
               case 5: { // KEY
                  elvl = 1;
                  if (rc<=0)
                     if (plainmode)
                        elvl = str_findkey(text, args->item[pos], 0)?0:1;
                     else
                        elvl = ifl->exist(secname, args->item[pos])?0:1;
                  break;
               }
               case 6: { // SEC
                  elvl = 1;
                  if (rc<=0 && ifl) elvl = ifl->secexist(secname,0)?0:1;
                  break;
               }
               case 7:  // SET
                  if (rc<=0)
                     if (plainmode) {
                        // this code is terribly slow
                        u32t keypos = 0;
                        char  *line = strdup(args->item[pos]);

                        line = strcat_dyn(line," = ");
                        line = strcat_dyn(line,args->item[pos+1]);
                        line = strcat_dyn(line,"0");
                        line[strlen(line)-1] = 0;  // \0\0

                        if (str_findkey(text, args->item[pos], &keypos))
                           str_delentry(text, keypos);
                        else
                           keypos = FFFF;

                        text = str_newentry(text, keypos, line, 0, 1);
                        free(line);
                     } else
                        ifl->setstr(secname, args->item[pos], args->item[pos+1]);
                  break;
            }

            if (rc<=0 && !ro_mode)
               if (plainmode) {
                  if (text->count==0) rc = fwritefull(ininame, 0, 0); else {
                     char *fd = str_gettostr(text, "\r\n");
                     rc = fwritefull(ininame, fd, strlen(fd));
                     free(fd);
                  }
               } else {
                  // check write result
                  qserr res = ifl->close();
                  rc = qserr2errno(res);
               }
            if (text) free(text);
            if (ifl) DELETE(ifl);
         }
      }
   } else
      rc = EINVAL;
   if (rc>0 && !quiet) cmd_shellerr(EMSG_CLIB,rc,0);
   // if called by pointer - it just will never be updated
   env_setvar("ERRORLEVEL", elvl);
   return elvl;
}

static void nokey_err(const char *name) {
   printf("Key \"%s\" not found\n", name);
}

u32t _std shl_setkey(const char *cmd, str_list *args) {
   int  rc=-1, nopause=0, verbose=0, quiet=0;
   static char *argstr   = "v|np|q";
   static short argval[] = {1, 1,1};
   // help overrides any other keys
   if (str_findkey(args, "/?", 0)) {
      cmd_shellhelp(cmd,CLR_HELP);
      return 0;
   }
   args = str_parseargs(args, 0, SPA_RETLIST|SPA_NOSLASH, argstr, argval,
                        &verbose, &nopause, &quiet);
   if (args->count>0) {
      static const char *ckey[] = { "SETA", "SETB", "SETF", "GETA", "GETB",
                                    "GETF", "DEL" , "LIST", "VIEW", "SETP", 0 };
      u32t ii, cnum;
      for (cnum=0; ckey[cnum]; cnum++)
         if (stricmp(ckey[cnum], args->item[0])==0) break;

      if (ckey[cnum]) {
         switch (cnum) {
            case 0: // SETKEY SETA key value
               if (args->count==3) {
                  sto_save(args->item[1], args->item[2], strlen(args->item[2]), 1);
                  rc = 0;
               }
               break;
            case 1: // SETKEY SETB key value
               if (args->count==3) {
                  char *eptr;
                  u32t   val = args->item[2][0]=='-' ?
                               strtol (args->item[2], &eptr, 0):
                               strtoul(args->item[2], &eptr, 0);
                  // verify conversion
                  if (*eptr) {
                     if (!quiet++)
                        printf("Invalid integer value\n");
                  } else {
                     sto_savedword(args->item[1], val);
                     rc = 0;
                  }
               }
               break;
            case 2: // SETKEY SETF key file
               if (args->count==3) {
                  dir_t  info;

                  if (_dos_stat(args->item[2], &info)) rc = ENOENT; else
                     if (info.d_attr&_A_SUBDIR) rc = EISDIR; else
                        if (info.d_size>_64MB) rc = EFBIG;
                  if (rc<0) {
                     u32t  flen;
                     void   *ff = freadfull(args->item[2], &flen);

                     if (!ff) {
                        if (!quiet++)
                           printf("Unable to read \"%s\"\n", args->item[2]);
                        rc = EIO;
                     } else {
                        sto_save(args->item[1], ff, flen, 1);
                        hlp_memfree(ff);
                        rc = 0;
                     }
                  }
               }
               break;
            case 3: // SETKEY GETA key varname
               if (args->count==3) {
                  u32t  vlen;
                  char *copy = (char*)sto_datacopy(args->item[1], &vlen);

                  if (copy) {
                     // need to add zero?
                     if (copy[vlen-1]) {
                        copy = (char*)realloc(copy, vlen+1);
                        copy[vlen] = 0;
                     }
                     setenv(args->item[2], copy, 1);
                     free(copy);
                     rc = 0;
                  } else {
                     if (!quiet++) nokey_err(args->item[1]);
                     rc = ENOENT;
                  }
               }
               break;
            case 4: // SETKEY GETB key varname
               if (args->count==3)
                  if (sto_size(args->item[1])) {
                     env_setvar(args->item[2], sto_dword(args->item[1]));
                     rc = 0;
                  } else {
                     if (!quiet++) nokey_err(args->item[1]);
                     rc = ENOENT;
                  }
               break;
            case 5: // SETKEY GETF key file
               if (args->count==3) {
                  void *ptr = sto_data(args->item[1]);
                  u32t size = sto_size(args->item[1]);

                  if (ptr && size)
                     rc = fwritefull(args->item[2], ptr, size);
                  else {
                     if (!quiet++) nokey_err(args->item[1]);
                     rc = ENOENT;
                  }
               }
               break;
            case 6: // SETKEY DEL mask
               if (args->count==2) {
                  if (strchr(args->item[1],'*') || strchr(args->item[1],'?')) {
                     str_list *lst = sto_list(args->item[1],0);
                     if (lst) {
                        for (ii=0; ii<lst->count; ii++) sto_del(lst->item[ii]);
                        free(lst);
                        rc = 0;
                     } else {
                        if (!quiet++)
                           printf("There are no keys found with mask \"%s\"\n",
                              args->item[1]);
                        rc = ENOENT;
                     }
                  } else
                  if (sto_data(args->item[1])) {
                     sto_del(args->item[1]);
                     rc = 0;
                  } else {
                     if (!quiet++) nokey_err(args->item[1]);
                     rc = ENOENT;
                  }
               }
               break;
            case 7: // SETKEY LIST [mask] [/np]
            case 8: // SETKEY VIEW [mask] [/np]
               if (args->count==1 || args->count==2) {
                  str_list *lst = sto_list(args->count==2 ? args->item[1] : 0, 1);

                  rc = 0;

                  if (!lst) {
                     printf(args->count==1 ? "Storage is empty\n" :
                                             "There is no such entries\n");
                  } else {
                     cmd_printseq(0, nopause?PRNSEQ_INITNOPAUSE:PRNSEQ_INIT, 0);
                     if (cnum==7)
                        cmd_printseq(" Variable Name                            Length    Type",
                                     0, VIO_COLOR_LWHITE);
                     for (ii=0; ii<lst->count; ii++) {
                        u32t size = sto_size(lst->item[ii]+1);
                        if (size) {
                           char *ln = 0;
                           int  brk = 0;
                                
                           if (cnum==7) {
                              char *nm = sprintf_dyn("\"%s\"", lst->item[ii]+1);
                              ln = sprintf_dyn("%-40s%9u   %s", nm, size,
                                 lst->item[ii][0]=='@'?" ptr ":"value");
                              free(nm);
                           } else {
                              char typech = lst->item[ii][0]=='@'?'p':'v';

                              if (size<=4) {
                                 u32t val = sto_dword(lst->item[ii]+1);
                                 // print signed variant if high bit is set
                                 ln = sprintf_dyn(val & 0x80000000 ?
                                    "%c  %s = %i (0x%X/%u)" : "%c  %s = %u (0x%X)",
                                       typech, lst->item[ii]+1, val, val, val);
                              } else {
                                 char line[128];
                                 u8t  *ptr = (u8t*)sto_data(lst->item[ii]+1);
                                 u32t  ofs;

                                 strcpy(line, "   ");
                                 cmd_printf("%c  %s (%u bytes):\n", typech,
                                    lst->item[ii]+1, size);

                                 if (ptr)
                                    for (ofs=0; ofs<size; ofs+=16) {
                                       _bindump(line+3, ptr+ofs, ofs, 8, 0,
                                                size-ofs<16 ? size-ofs : 16, 16, 1);
                                       brk = cmd_printseq(line, 0, 0);
                                       if (brk) break;
                                    }
   
                              }
                           }
                           if (ln) {
                              brk = cmd_printseq(ln, 0, 0);
                              free(ln);
                           }
                           if (brk) { rc = EINTR; break; }
                        }
                     }
                     free(lst);
                  }
               }
               break;
            case 9: // SETKEY SETP [/q] key address length
               if (args->count==4) {
                  char *eptr;
                  u32t  addr = strtoul(args->item[2], &eptr, 0);
                  // check address/length conversion & page attrs
                  if (*eptr) {
                     if (!quiet++)
                        printf("Invalid address value\n");
                  } else {
                     u32t len = strtoul(args->item[3], &eptr, 0);
                     if (*eptr || !len || addr+len<addr) {
                        if (!quiet++)
                           printf("Invalid length value\n");
                     } else
                     if (pag_queryrange((void*)addr,len) &
                                        (PGEA_UNKNOWN|PGEA_NOTPRESENT))
                     {
                        rc = EFAULT;
                        if (!quiet++)
                           printf("Address range has invalid pages!\n");
                     } else {
                        sto_save(args->item[1], (void*)addr, len, 0);
                        rc = 0;
                     }
                  }

               }
               break;
         }
      } else
      if (!quiet++) printf("Invalid subcommand (%s)\n", args->item[0]);
   }
   // free args copy, returned by str_parseargs() above
   free(args);

   if (rc<0) rc = EINVAL;
   if (rc && rc!=EINTR && !quiet) cmd_shellerr(EMSG_CLIB,rc,0);
   return rc;
}
