#!/bin/sh
# **************************************************
# * make [0]        - debug make                   *
# * make  1         - release make                 *
# * make  2         - init debug make              *
# * make [x] clean  - clean                        *
# * make [x] nolink - make without linking         *
# **************************************************

prjname=qsefi

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

# makefile for reference
spprj -nb $prjname.prj $bldtype bld/$prjname.mak
# and build it
spprj -b -w $prjname.prj $bldtype "$bldarg"
