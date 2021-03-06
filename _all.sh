#!/bin/sh
# *********************************
# usage: _all         - build
#        _all clean   - clean
# *********************************

if ! test -f system/src/setup.sh
then
  # actually, user should prepare it
  cp -f system/src/setup.sh.template system/src/setup.sh
fi

# * loader *
cd system/src/init
. ./make.sh $1 $2

# * efi 32-bit code *
cd ../ldrefi
. ./make.sh 1 $1 $2

# * ldi *
cd ../ldrapps
. ./make.sh $1 $2

# * biostab.exe * just useful
cd ../usrapps/biostab
. ./make.sh $1 $2

cd ../../../..
