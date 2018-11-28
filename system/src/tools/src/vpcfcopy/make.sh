#!/bin/sh
# **************************************************
# * make [1]        - release make                 *
# * make 0          - debug make                   *
# * make [x] clean  - clean                        *
# * make [x] nolink - make without linking         *
# **************************************************

prjname=vpcfcopy

# setup variables
. ../../../setup.sh

bldtype=$1
bldarg=$2

if [ "$bldtype" != "0" ]
then
  if [ "$bldtype" != "1" ]
  then
    bldarg=$1
  fi
  bldtype=1
fi
if test -z $bldarg
then
  bldarg=\*
fi

# creating dirs (else spprj will fail to write misc files before build)
spprj -b -w -nb $toolprjkey $prjname.prj $bldtype makedirs
# makefile for reference
spprj -nb $prjname.prj $bldtype bld/$prjname.mak
# and build it
spprj -b -nq -bl -w -es $toolprjkey $prjname.prj $bldtype "$bldarg"
