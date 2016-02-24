#!/bin/sh
# **************************************************
# * make [0]        - debug make                   *
# * make  1         - release make                 *
# * make  2         - init debug make              *
# * make [x] clean  - clean                        *
# * make [x] nolink - make without linking         *
# **************************************************

prjname=qsinit

# setup variables
. ../setup.sh

bldtype=$1
bldarg=$2

if [ "$bldtype" != "1" ] && [ "$bldtype" != "2" ]
then
  if [ "$bldtype" != "0" ]
  then
    bldarg=$1
  fi
  bldtype=0
fi
if test -z $bldarg
then
  bldarg=\*
fi

# creating dirs (else spprj will fail to write misc files before build)
spprj -b -w -nb $prjname.prj $bldtype makedirs
# makefile for reference
spprj -nb $prjname.prj $bldtype bld/$prjname.mak
# and build it
spprj -b -w $prjname.prj $bldtype "$bldarg"

if test -f OS2LDR
then
  cp -f OS2LDR $QS_BIN
fi
