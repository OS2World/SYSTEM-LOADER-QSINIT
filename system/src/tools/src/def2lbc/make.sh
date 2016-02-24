#!/bin/sh

. ../../../setup.sh

keys="-zq -DSP_LINUX -DSRT -DNO_HUGELIST -DNO_RANDOM -DSORT_BY_OBJECTS_ON -DNO_SP_ASSERTS"

wcl386 $keys -I../../../ldrapps/hc/rt ref2lbc.cpp ../../../ldrapps/runtime/rt/sp_defs.cpp ../../../ldrapps/runtime/rt/sp_str.cpp

if (($? == 0))
then
  strip ref2lbc
  mv -f ref2lbc ../../t_unx
fi

rm -f *.o
