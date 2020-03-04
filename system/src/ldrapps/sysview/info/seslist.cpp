#include "diskedit.h"
#include "seslist.h"
#ifdef __QSINIT__
#include "qstask.h"
#include "qcl/qsmt.h"

extern qs_mtlib mtl;
#endif

#define cmProcRefresh  1000
#define cmProcKill     1001
#define cmProcKillTree 1002


void TSesListDialog::FreeSessionData() {
#ifdef __QSINIT__
   if (sl) { free(sl); sl = 0; }
   secnt = 0;
#endif
}

void TSesListDialog::ReadSessionData() {
#ifdef __QSINIT__
   FreeSessionData();

   TCollection *sc = new TCollection(0,10);
   sl = se_enum(0);

   if (sl) {
      u32t ii;
      for (secnt=0; sl[secnt]; secnt++) ;

      TRect nr(lbSesList->getBounds());
      int wdt = nr.b.x-nr.a.x;

      for (ii=0; ii<secnt; ii++) {
         char    *str = new char[wdt+1], buf[16],
              *devstr = 0;
         se_stats *se = sl[ii];
         u32t   amask = 0, di;
         // active on devices
         for (di=0; di<se->handlers; di++) amask|=1<<se->mhi[di].dev_id;

         for (di=0; di<32; di++)
            if (se->devmask & 1<<di) {
               snprintf(buf, 16, "%s%u,", amask & 1<<di?"*":"", di);
               devstr = strcat_dyn(devstr, buf);
            }
         if (devstr) trimright(devstr, ","); else devstr = strdup("");

         snprintf(str, wdt+1, "%4u  %-30s %3ux%u [%s]",
            se->sess_no, se->title?se->title:"-", se->mx, se->my, devstr);
         free(devstr);

         sc->insert(str);
      }
   }
   lbSesList->newList(sc);
#endif
}

TSesListDialog::TSesListDialog() : TDialog(TRect(5, 5, 75, 18), "Session list"),
   TWindowInit(TSesListDialog::initFrame)
{
   TView *control;
   options |= ofCenterX | ofCenterY;
   helpCtx = hcSesList;

   control = new TStaticText(TRect(2, 1, 57, 2), " No    Title                 "
                             "          Mode [Device]");
   insert(control);

   control = new TScrollBar(TRect(57, 2, 58, 12));
   insert(control);
   lbSesList = new TListBox(TRect(2, 2, 57, 12), 1, (TScrollBar*)control);
   insert(lbSesList);

   control = new TButton(TRect(59, 2, 69, 4), "~S~elect", cmOK, bfDefault);
   control->helpCtx = hcSesListSwitch;
   insert(control);
/*
   control = new TButton(TRect(59, 4, 69, 6), "~C~lose", cmCancel, bfNormal);
   insert(control); */

   selectNext(False);
#ifdef __QSINIT__
   sl = 0;
#endif
   ReadSessionData();
}

TSesListDialog::~TSesListDialog() {
   FreeSessionData();
}

void TSesListDialog::handleEvent( TEvent& event) {
   TDialog::handleEvent(event);
}

Boolean TSesListDialog::valid(ushort command) {
   Boolean rslt = TDialog::valid(command);
   if (rslt && (command == cmOK))
      if (lbSesList->focused>=0) {
#ifdef __QSINIT__
         qserr err = se_switch(sl[lbSesList->focused]->sess_no, FFFF);
         if (err) {
            SysApp.PrintPTErr(err);
            ReadSessionData();
            rslt = False;
         }
#endif
      }
   return rslt;
}

void TSysApp::SessionListDlg() {
   TSesListDialog *dlg = new TSesListDialog();
   execView(dlg);
   delete dlg;
}

void CheckMTMode(void) {
#ifdef __QSINIT__
   if (!mt_active()) {
      //if (askDlg(MSGA_TURNMTMODE)) {
      if (1) {
         if (!mtl) mtl = NEW(qs_mtlib);
         if (mtl) {
            qserr err = mtl->initialize();
            if (err==E_SYS_DONE) err = 0;

            if (err && !mt_active()) SysApp.PrintPTErr(err);
         }
      }
   } else
   if (!mtl) mtl = NEW(qs_mtlib);
#endif
}

void TSysApp::SessionNew() {
#ifdef __QSINIT__
   CheckMTMode();
   if (mt_active()) {
      qserr  err = 0;
      char *cmdp = env_getvar("COMSPEC", 0);
      u32t   cmd = mod_searchload(cmdp?cmdp:"cmd.exe", 0, &err);
      if (cmdp) free(cmdp);
      if (cmd) err = mtl->execse(cmd,0,0,0,0,0,0);
      if (err) PrintPTErr(err);
   }
#endif
}

TProcessTree::TProcessTree(const TRect& bounds,TScrollBar *aVScrollBar, TScrollBar *aHScrollBar) :
   TListViewer(bounds,1,aHScrollBar,aVScrollBar)
{
#ifdef __QSINIT__
   pos2pi = 0;  nproc = 0;  pi = 0;  str = 0;
#endif
   ReadProcList();
}

TProcessTree::~TProcessTree() {
   FreeProcList();
}

void TProcessTree::focusItem(int item) {
   TListViewer::focusItem(item);
   ((TProcInfoDialog*)owner)->UpdateInfo();
}

void TProcessTree::FreeProcList() {
#ifdef __QSINIT__
   if (pi) { free(pi); pi = 0; }
   if (pos2pi) { delete pos2pi; pos2pi = 0; }
   if (str) { str->freeitems(0,FFFF); DELETE(str); str = 0; }
   nproc = 0;
#endif
}

#ifdef __QSINIT__
process_information *TProcessTree::selInfo() {
   if (!nproc || !pi || !pos2pi) return 0;
   return focused>=0 && focused<nproc ? pos2pi[focused] : 0;
}

static u32t tree_walk(process_information *pd, process_information **pos2pi,
                      ptr_list str, u32t pos, u32t level)
{
   char *line = 0;
   for (u32t ll=0; ll<level; ll++) {
      line = strcat_dyn(line, "\xFA");
   }
   if (level) strcat_dyn(line, " ");
   line = strcat_dyn(line, pd->mi?pd->mi->name:"");
   str->add(line);
   pos2pi[pos++] = pd;

   for (u32t ii=0; ii<pd->ncld; ii++)
      pos = tree_walk(pd->cll[ii], pos2pi, str, pos, level+1);
   return pos;
}
#endif // __QSINIT__

void TProcessTree::ReadProcList() {
   FreeProcList();
#ifdef __QSINIT__
   nproc   = mod_processenum(&pi);
   pos2pi  = new process_information* [nproc];
   str     = NEW(ptr_list);
   // pid 1 in 1st pos assumed here
   tree_walk(pi, pos2pi, str, 0, 0);
   setRange(nproc);
#endif // __QSINIT__
}

void TProcessTree::UpdateList() {
#ifdef __QSINIT__
   process_information *pi = selInfo();
   u32t                pid = pi->pid, ii;

   ReadProcList();
   for (ii=0; ii<nproc ; ii++)
      if (pos2pi[ii]->pid==pid) { focusItem(ii); drawView(); return; }
#endif // __QSINIT__
   focusItem(0);
   drawView();
}

void TProcessTree::getText(char *text, int item, int maxChars) {
#ifdef __QSINIT__
   if (str && str->max()>=item) {
      strncpy(text, (char*)str->value(item), maxChars);
      text[maxChars] = 0;
   }
#else
   text[0] = 0;
#endif
}

TProcInfoDialog::TProcInfoDialog() : TDialog(TRect(1, 1, 79, 21),
   "Process information"), TWindowInit(TProcInfoDialog::initFrame)
{
   TView *control;
   int      diffx = 0, diffy = 0;
   options  |= ofCenterX | ofCenterY;
   helpCtx   = hcProcInfo;
   in_update = 0;
   selpid    = 0;
   // expand dialog size in case of larger screen mode
   if (TScreen::screenHeight>25 || TScreen::screenWidth>80) {
      diffy = TScreen::screenHeight - 25,
      diffx = TScreen::screenWidth - 80;
      TRect nr(getExtent());
      if (diffy>48) diffy = 48;
      if (diffx>32) diffx = 32;
      nr.b.x += diffx>>=1;
      nr.b.y += diffy>>=1;
      nr.move(TScreen::screenWidth-nr.b.x>>1, TScreen::screenHeight-nr.b.y>>1);
      locate(nr);
   }

   control = new TScrollBar(TRect(24+diffx, 1, 25+diffx, 19+diffy));
   insert(control);

   procList = new TProcessTree(TRect(2, 1, 24+diffx, 19+diffy), (TScrollBar*)control, 0);
   insert(procList);

   control = new TScrollBar(TRect(75+diffx, 1, 76+diffx, 16+diffy));
   insert(control);

   procInfo = new TListBox(TRect(26+diffx, 1, 75+diffx, 16+diffy), 1, (TScrollBar*)control);
   insert(procInfo);

   control = new TButton(TRect(28+diffx, 17+diffy, 39+diffx, 19+diffy),
                         "~R~efresh", cmProcRefresh, bfNormal);
   control->helpCtx = hcProcInfoSwitch;
   insert(control);

   control = new TButton(TRect(40+diffx, 17+diffy, 50+diffx, 19+diffy),
                         "~K~ill", cmProcKill, bfNormal);
   control->helpCtx = hcProcInfoKill;
   insert(control);

   control = new TButton(TRect(51+diffx, 17+diffy, 64+diffx, 19+diffy),
                         "Kill ~t~ree", cmProcKillTree, bfNormal);
   control->helpCtx = hcProcInfoKillTree;
   insert(control);

   control = new TButton(TRect(65+diffx, 17+diffy, 74+diffx, 19+diffy),
                         "~C~lose", cmCancel, bfDefault);
   insert(control);

   selectNext(False);
   UpdateInfo();
}

void TProcInfoDialog::UpdateInfo() {
#ifdef __QSINIT__
   process_information *pi = procList->selInfo();
   if (pi && (pi->pid!=selpid || in_update)) {
      TCollection *dl = new TCollection(0,10);
      char *pid = new char[64];
      snprintf(pid, 64, pi->parent?"PID %u, Parent PID %u":"PID %u", pi->pid,
         pi->parent?pi->parent->pid:0);
      dl->insert(pid);
      if (pi->mi)
         if (pi->mi->mod_path) {
            u32t blen = 48 + strlen(pi->mi->mod_path), ii, code = 0, data = 0;
            char  *mp = new char[blen];
            snprintf(mp, blen, "Module: %s", pi->mi->mod_path);
            dl->insert(mp);
            // enum objects
            for (ii=0; ii<pi->mi->objects; ii++) {
               u32t osz, ofl;
               if (!mod_objectinfo(pi->mi->handle, ii, 0, &osz, &ofl, 0))
                  if (ofl&4) code+=osz; else // OBJEXEC
                     if ((ofl&0x88)==0) data+=osz; // all except OBJRSRC & OBJINVALID
            }
            if (code || data) {
               mp = new char[64];
               snprintf(mp, 64, data?"  %uk code, %uk data and stack":"  %uk code",
                  code>>10, data>>10);
               dl->insert(mp);
            }
         }
      char *ee = new char[2];
      ee[0] = 0;
      dl->insert(ee);
      ee = new char[48];
      snprintf(ee, 48, "Thread list (%u):", pi->threads);
      dl->insert(ee);
      u32t ii, thmem = 0;
      for (ii=0; ii<pi->threads; ii++) {
         static const char *ths[4] = { "RUNNING", "SUSPEND", "GONE", "WAITING" };
         thread_information *ti = pi->ti+ii;
         mem_statdata      mi_t;
         char            thtime[64];
         // thread owned memory
         mi_t.ph  = pi->pid;
         mi_t.tid = ti->tid;
         mem_statinfo(QSMEMINFO_THREAD, &mi_t);
         thmem += mi_t.mem;
         
         if (ti->time==0) strcpy(thtime,"no time data"); else
            if (ti->time<10) strcpy(thtime,"<1 ms"); else {
               u64t ms = ti->time/10;
               if (ms<100000) sprintf(thtime, "%Lu ms", ms); else
                  if ((ms/=1000)<10000) sprintf(thtime, "%Lu sec", ms); else
                     if ((ms/=60)<10000) sprintf(thtime, "%Lu min", ms); else
                        if ((ms/=60)<10000) sprintf(thtime, ">%Lu hr", ms); else {
                           ms/=24*365;  // :)
                           sprintf(thtime, ">%Lu yr", ms);
                        }
            }
      
         ee = new char[64];
         snprintf(ee, 64, "%3u %7s %7uk - %s", ti->tid, ti->state<4?ths[ti->state]:"???",
            mi_t.mem>>10, thtime);
         dl->insert(ee);
      }
      ee = new char[2];
      ee[0] = 0;
      dl->insert(ee);

      mem_statdata mi_p, mi_m;
      u32t             sysmem = 0;
      mi_p.ph = pi->pid;
      mem_statinfo(QSMEMINFO_PROCESS, &mi_p);
      mi_p.mem += hlp_memused(pi->pid,0,0);

      if (pi->pid==1) {
         sysmem += hlp_memused(0,0,0);
         mem_statinfo(QSMEMINFO_GLOBAL, &mi_m);
         sysmem += mi_m.mem;
      }
      if (pi->mi) {
         mi_p.ph = pi->mi->handle;
         mem_statinfo(QSMEMINFO_MODULE, &mi_m);
      } else { 
         mi_m.mem=0; mi_m.nblk=0;
      }
      u32t total = thmem + mi_p.mem + mi_m.mem + sysmem;
      ee = new char[48];
      snprintf(ee, 48, "Memory (total) : %7uk / %u", total>>10, total);
      dl->insert(ee);  
      if (sysmem) {
         ee = new char[48];
         snprintf(ee, 48, "- system global: %7uk / %u", sysmem>>10, sysmem);
         dl->insert(ee);  
      }
      ee = new char[48];
      snprintf(ee, 48, "- process owned: %7uk / %u", mi_p.mem>>10, mi_p.mem);
      dl->insert(ee);  
      ee = new char[48];
      snprintf(ee, 48, "- module owned : %7uk / %u", mi_m.mem>>10, mi_m.mem);
      dl->insert(ee);  
      ee = new char[48];
      snprintf(ee, 48, "- thread owned : %7uk / %u", thmem>>10, thmem);
      dl->insert(ee);  

      selpid = pi->pid;
      procInfo->newList(dl);
   }
#endif
}

void TProcInfoDialog::handleEvent( TEvent& event) {
   TDialog::handleEvent(event);
   switch (event.what) {
      case evCommand:
         switch (event.message.command) {
            case cmProcKill    :
            case cmProcKillTree:
#ifdef __QSINIT__
               CheckMTMode();
               if (!mt_active()) break; else {
                  process_information *pi = procList->selInfo();
                  if (pi) {
                     // dynamic call to prevent static linking of MTLIB
                     qserr err = mtl->stop(pi->pid, event.message.command==
                                           cmProcKill?0:1, EXIT_FAILURE);
                     if (err) SysApp.PrintPTErr(err);
                  }
               }
#endif
               // no break here
            case cmProcRefresh :
               in_update = 1;
               procList->UpdateList();
               in_update = 0;
               break;
         }
         break;
      default:
         break;
   }
}

void TSysApp::ProcInfoDlg() {
   TProcInfoDialog *dlg = new TProcInfoDialog();
   execView(dlg);
   delete dlg;
}
