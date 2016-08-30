#!/bin/sh

export QS_BASE=$QS_ROOT/system/src
export QS_BIN=$QS_ROOT/tbin
export QS_APPINC=$QS_BASE/ldrapps/hc
export QS_ARCH=QSINIT.LDI
export QS_NAME=QSINIT
export BUILD_HERE=unx

export PATH=$QS_BASE/tools/t_$BUILD_HERE:$QS_BASE/tools:$PATH:$WATCOM/binl

export INCLUDE=$WATCOM/h:$QS_BASE/hc

# no watcom libs, structs packed to 1 byte,
# inline FP code, unmangled stdcall (runtime functions), favor size
export QS_CFLAGS="-zz -zl -zls -zq -zp1 -fp5 -fpi87 -zri -os -bt=$QS_NAME"

# special header for main function
export QS_MHDR="$QS_BASE/ldrapps/runtime/initdef.h"

# /dev/null path
export DEVNUL="/dev/null"

# common include key for wcc
export QS_WCCINC="-i$QS_BASE/hc -i$QS_APPINC -i$QS_BASE/init/hc"
