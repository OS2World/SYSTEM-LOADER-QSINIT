#define Uses_TScrollBar
#define Uses_TListBox
#define Uses_TDialog
#define Uses_TRect
#define Uses_TView
#define Uses_MsgBox
#define Uses_TProgram
#define Uses_TButton
#define Uses_TCollection
#define Uses_TColoredText
#define Uses_TStaticText

#include <tv.h>
#include "tcolortx.h"
#include "diskedit.h"
#include "direct.h"
#include <stdlib.h>

#ifndef __QSINIT__
void TSysApp::LoadFromZIPRevArchive(TInputLine *ln) {
   messageBox("\x03""Not implemented in pc build!",mfError+mfOKButton);
}
#else
// temporary
extern "C" {
#include "../../../init/zip/unzip.h"
}
#include "qsutil.h"

static const char *kdisk1_name = "1:/OS2KRNL!";
#define KERNELSIZE_CHECK (_1MB+_512KB)

static TListBox *k_ziplist;

TDialog* makeKernZipDialog(void) {
   TView *control;
   
   TDialog* dlg = new TDialog(TRect(26, 1, 54, 22), "Select kernel");
   if (!dlg) return 0;
   
   dlg->options |= ofCenterX | ofCenterY;
   
   control = new TScrollBar(TRect(26, 1, 27, 17));
   dlg->insert(control);
   
   k_ziplist = new TListBox(TRect(1, 1, 26, 17), 1, (TScrollBar*)control);
   dlg->insert(k_ziplist);
   
   control = new TButton(TRect(9, 18, 19, 20), "O~K~", cmOK, bfDefault);
   dlg->insert(control);
   
   dlg->selectNext(False);
   return dlg;
}

void TSysApp::LoadFromZIPRevArchive(TInputLine *ln) {
   TCollection *zl = new TCollection(0,10);
   u32t    zipsize = 0;
   void       *zip = 0;
   ZIP          zz;
   char  *filename;
   u32t   filesize;
   diskfree_t dspc;
   // remove previously unpacked kernel
   unlink(kdisk1_name);
   // get disk free space
   if (_dos_getdiskfree(kdisk1_name[0]-'0'+1,&dspc)) {
      messageBox("\x03""Internal error...",mfError+mfOKButton);
      return;
   }
   filesize = dspc.avail_clusters*dspc.sectors_per_cluster*(dspc.bytes_per_sector>>9);
   if (filesize<KERNELSIZE_CHECK>>9) {
      if (messageBox("\x03""There is no space on virtual disk to unpack kernel. Stop?",
         mfWarning+mfYesButton+mfNoButton)==cmYes) return;
   }

   char rva_p[NAME_MAX+1], *ep;
   ep = getenv("REV_ARCH_PATH");
   if (ep) strncpy(rva_p,ep,NAME_MAX); else strcpy(rva_p,"REV_ARCH.ZIP");
   rva_p[NAME_MAX] = 0;

   if (rva_p[1]!=':') {
      PercentDlgOn("Loading zip file");
      zip=hlp_freadfull(rva_p,&zipsize,PercentDlgCallback);
      // close "loading" dialog
      PercentDlgOff(ln->owner);
   }
   // was not readed?
   if (!zip) zip=freadfull(rva_p,&zipsize);

   do {
      if (!zip) {
         char emsg[NAME_MAX+1+128];
         sprintf(emsg, "\x03""Unable to read ZIP file \"%s\"!", rva_p);
         messageBox(emsg,mfError+mfOKButton);
         break;
      }
      if (!zip_open(&zz, zip, zipsize)) {
         messageBox("\x03""This is not a zip archive!",mfError+mfOKButton);
         break;
      }
  
      while (zip_nextfile(&zz,&filename,&filesize)) { 
         char *str = new char[strlen(filename)+1];
         strcpy(str,filename);
         zl->insert(str);
      }
      // re-open zip
      zip_close(&zz);

      TDialog *ci = makeKernZipDialog();
      k_ziplist->newList(zl);
      int res=execView(ci)==cmOK;
      if (res) {
         char *nm = (char*)zl->at(k_ziplist->focused);

         zip_open(&zz, zip, zipsize);
         while (zip_nextfile(&zz,&filename,&filesize)) { 
            if (strcmp(nm,filename)==0) {
               FILE   *ff;
               void *mem = zip_readfile(&zz);
         
               if (!mem) {
                  messageBox("\x03""Zip unpack error!",mfError+mfOKButton);
                  break;
               }
               ff = fopen(kdisk1_name, "wb");
               if (ff) {
                  int err = fwrite(mem,1,filesize,ff)!=filesize;
                  fclose(ff);
                  if (err) ff=0;
               }
               if (mem) {
                  memset(mem,0,filesize);
                  hlp_memfree(mem);
               }
               if (!ff)  {
                  messageBox("\x03""There is no space for kernel on virtual "
                     "disk! Increase DISKSIZE parameter in OS2LDR.INI.",
                     mfError+mfOKButton);
                  break;
               }
               // done
               setstr(ln,kdisk1_name);
               break;
            }
         }
         zip_close(&zz);

      }
      destroy(ci);

   } while (false);

   destroy(zl);

   if (zip) {
      memset(zip,0,zipsize);
      hlp_memfree(zip);
   }
}
#endif
