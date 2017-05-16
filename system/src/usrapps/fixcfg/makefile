#
# wmake C++ example
#

PRJNAME = fixcfg
CC = @wcc386
CPP = @wpp386
CFLAGS = -5s $(%QS_CFLAGS) $(%QS_WCCINC) -s -mf -oxs -fi=$(%QS_MHDR)
CPPFLAGS = $(CFLAGS) -zv -i$(%QS_APPINC)/rt

all : $(PRJNAME).exe

FILE1 = fixcfg.obj
FILE2 = process.obj

BUILDFILES = $(FILE1) $(FILE2)

.c.obj : .AUTODEPEND
      @echo $*.c
      $(CC) $(CFLAGS) -fo=$@ $<

.cpp.obj : .AUTODEPEND
      @echo $*.cpp
      $(CPP) $(CPPFLAGS) -fo=$@ $<

$(PRJNAME).exe : $(PRJNAME).lnk $(BUILDFILES)
      @echo Linking...
      @wlink @$(PRJNAME).lnk 
      @zip -q -9 -X -j $(%QS_BIN)/$(%QS_ARCH) $@

clean: .SYMBOLIC
      @echo Cleaning ...
      @if exist *.obj @del *.obj >nul
      @if exist *.lnk @del *.lnk >nul
      @if exist *.map @del *.map >nul

#
# note, that cpplib.lib must be ABOVE plib3s.lib to prevent default
# new/delete linkage
#
$(PRJNAME).lnk : makefile
      @echo sys os2v2            >$@
      @echo name $(PRJNAME)      >>$@
      @echo file $(FILE1)        >>$@
      @echo file $(FILE2)        >>$@
      @echo file $(%QS_BASE)/ldrapps/runtime/bld/Common/init.obj>>$@
      @echo lib $(%QS_BASE)/ldrapps/runtime/runtime.lib>>$@
      @echo lib $(%QS_BASE)/ldrapps/runtime/cpplib.lib>>$@
      @echo lib $(%WATCOM)/lib386/plib3s.lib>>$@
      @echo op quiet             >>$@
      @echo op el                >>$@
      @echo op internalrelocs    >>$@
      @echo op togglerelocs      >>$@
      @echo op osname=QSINIT     >>$@
      @echo op nod               >>$@
      @echo op st=16384          >>$@
