#
# wmake vhdd makefile
#

PRJNAME = vhdd
MODNAME = $(PRJNAME).dll
CC      = @wcc386
ASM     = @wasm
INC2H   = @inc2h
OUTDIR  = bld
LNK     = $(OUTDIR)\$(PRJNAME).lnk
OUT     = $(OUTDIR)/
CFLAGS  = -4s $(%QS_CFLAGS) $(%QS_WCCINC) -s -mf -oxs -i=$(OUTDIR) -fi=$(%QS_BASE)\ldrapps\runtime\initdef.h
AFLAGS  = -mf -4p -zq -i=%(QS_BASE)\hc

FILE1   = $(OUT)vhdd.obj
FILE2   = $(OUT)rwdisc.obj

BUILDFILES = $(FILE1) $(FILE2)

result : $(OUTDIR) $(MODNAME)

$(OUT)vhdd.obj: vhdd.c rwdisk.h $(OUT)dskinfo.h $(OUT)ioint13.h
      @echo $^&.c
      $(CC) $(CFLAGS) -fo=$@ vhdd.c

$(OUT)rwdisc.obj: rwdisc.c rwdisk.h $(OUT)dskinfo.h $(OUT)ioint13.h
      @echo $^&.c
      $(CC) $(CFLAGS) -fo=$@ rwdisc.c

$(OUT)dskinfo.h : ../../init/inc/dskinfo.inc
      @echo $^&.inc
      $(INC2H) $< $@ /p1

$(OUT)ioint13.h : ../../hc/inc/ioint13.inc
      @echo $^&.inc
      $(INC2H) $< $@ /p1

$(OUTDIR) : .SYMBOLIC
      @if not exist $(OUTDIR) mkdir $(OUTDIR)

$(MODNAME) : $(LNK) $(BUILDFILES)
      @echo Linking...
      @wlink @$(LNK)
      @lxlite $@ >nul
      @md dll
      @copy $@ dll\$@ >nul
      @pkzip /add /max /silent /nozipext /dir /rec /move $(%QS_BIN)\$(%QS_ARCH) dll\*.*

clean  : .SYMBOLIC
      @echo Cleaning ...
      @if exist $(OUTDIR)\*.obj del $(OUTDIR)\*.obj > nul
      @if exist $(OUTDIR)\*.lnk del $(OUTDIR)\*.lnk > nul
      @if exist $(OUTDIR)\*.h del $(OUTDIR)\*.h > nul

$(LNK) : makefile
      @echo sys os2v2_dll        >$@
      @echo name $(PRJNAME)      >>$@
      @echo file $(FILE1)        >>$@
      @echo file $(FILE2)        >>$@
      @echo file $(%QS_BASE)\ldrapps\runtime\bld\Common\initdll.obj>>$@
      @echo lib $(%QS_BASE)\ldrapps\runtime\runtime.lib>>$@
      @echo op map=$(PRJNAME).map>>$@
      @echo sort global          >>$@
      @echo op quiet             >>$@
      @echo op internalrelocs    >>$@
      @echo op togglerelocs      >>$@
      @echo op el                >>$@
      @echo op nod               >>$@
      @echo op osname=$(%QS_NAME)>>$@
