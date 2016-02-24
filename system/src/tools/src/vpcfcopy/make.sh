#!/bin/sh

. ../../../setup.sh

wcl386 -zq -I../../../init/fat vpcfcopy.c

if (($? == 0))
then
  strip vpcfcopy
  mv -f vpcfcopy ../../t_unx
fi

rm -f *.o
