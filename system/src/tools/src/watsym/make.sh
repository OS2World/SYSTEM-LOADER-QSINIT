#!/bin/sh

. ../../../setup.sh

keys="-zq -DSP_LINUX -DSRT -DNO_HUGELIST -DNO_RANDOM -DNO_SP_ASSERTS"

wcl386 $keys -I../../../ldrapps/hc/rt watsym.cpp ../../../ldrapps/runtime/rt/sp_defs.cpp ../../../ldrapps/runtime/rt/sp_str.cpp

if (($? == 0))
then
  strip watsym
  mv -f watsym ../../t_unx
fi

rm -f *.o
