#!/bin/sh
# ***************************************************
# * make            - build all projects            *
# * make clean      - clean all projects            *
# * make prj        - build single project          *
# * make prj xx     - build single project`s target *
# ***************************************************

# setup variables
. ../setup.sh

export appinit_lib=$QS_BASE/ldrapps/runtime

build_all=1

# echo "'$1'"

if [ "$1" != "clean" ]
then
  if test -n "$1"
  then
    build_all=0
  fi
fi

if [ "$build_all" = "1" ]
then
  bldparm=$1
  # ------- add project here -------
  bash -c "./make.sh runtime  $bldparm"
  bash -c "./make.sh tv       $bldparm"
  bash -c "./make.sh anacnda2 $bldparm"
  bash -c "./make.sh start    $bldparm"
  bash -c "./make.sh tetris   $bldparm"
  bash -c "./make.sh doshlp   $bldparm"
  bash -c "./make.sh bootos2  $bldparm"
  bash -c "./make.sh console  $bldparm"
  bash -c "./make.sh bootmenu $bldparm"
  bash -c "./make.sh sysview  $bldparm"
  bash -c "./make.sh cmd      $bldparm"
  bash -c "./make.sh partmgr  $bldparm"
  bash -c "./make.sh vmtrr    $bldparm"
  bash -c "./make.sh cache    $bldparm"
  bash -c "./make.sh vdisk    $bldparm"
  bash -c "./make.sh cplib    $bldparm"
  bash -c "./make.sh vhdd     $bldparm"
  bash -c "./make.sh extcmd   $bldparm"
#  bash -c "./make.sh mtlib    $bldparm"
else
  if test -f $1/$1.prj
  then
    # ----- single project build -----
    cd $1
    # creating dirs (else spprj will fail to write misc files before build)
    spprj -b -w -nb $1.prj 0 makedirs
    # writing makefile for reference
    spprj -nb $1.prj 0 bld/$1.mak
    # and build it
    bldarg=$2
    if test -z $bldarg
    then
      bldarg=\*
    fi
    spprj -b -bl -w $BCKEY -es $1.prj 0 "$bldarg"
    cd ..
  else
    echo Project \"$1\" is not exist!
  fi
fi
