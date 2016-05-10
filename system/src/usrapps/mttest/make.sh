#!/bin/sh
# **************************************************
# * make            - make                         *
# * make clean      - clean                        *
# * make nolink     - make without linking         *
# **************************************************

prjname=mttest

# setup variables
. ../../setup.sh

export appinit_lib=$QS_BASE/ldrapps/runtime

bldtype=0
bldarg=$1

if test -z $bldarg
then
  bldarg=\*
fi

if test -f $prjname.prj
then
  # creating dirs (else spprj will fail to write misc files before build)
  spprj -b -w -nb $toolprjkey $prjname.prj $bldtype makedirs
  # makefile for reference
  spprj -nb $prjname.prj $bldtype bld/$prjname.mak
  # and build it
  spprj -b -nq -bl -w -es $toolprjkey $prjname.prj $bldtype "$bldarg"
else
    echo Project \"$prjname\" is not exist!
fi
