#
# wmake example makefile ( LE app build example )
#

PRJNAME = example
CC = @wcc386
CFLAGS = -4s $(%QS_CFLAGS) $(%QS_WCCINC) -s -mf -oxs -fi=$(%QS_BASE)\ldrapps\runtime\initdef.h

all : $(PRJNAME).exe

BUILDFILES = example.obj

.c.obj : .AUTODEPEND
      @echo $*.c
      $(CC) $(CFLAGS) -fo=$@ $<

$(PRJNAME).exe : $(PRJNAME).lnk $(BUILDFILES)
      @echo Linking...
      @wlink @$(PRJNAME).lnk 
      @pkzip /add /max /silent /nozipext /path=none $(%QS_BIN)\$(%QS_ARCH) $@

clean: .SYMBOLIC
      @echo Cleaning ...
      @if exist *.obj @del *.obj >nul
      @if exist *.lnk @del *.lnk >nul
      @if exist *.map @del *.map >nul

$(PRJNAME).lnk : makefile
      @echo sys pmodew            >$@
      @echo name $(PRJNAME)      >>$@
      @echo file $(BUILDFILES)   >>$@
      @echo file $(%QS_BASE)\ldrapps\runtime\bld\Common\init.obj>>$@
      @echo lib $(%QS_BASE)\ldrapps\runtime\runtime.lib>>$@
      @echo op map=$(PRJNAME).map>>$@
      @echo op quiet             >>$@
      @echo op el                >>$@
      @echo op nod               >>$@
      @echo op st=32768          >>$@
      @echo op stub=$(%QS_BASE)\ldrapps\runtime\NODOS.BIN>>$@
