#!/bin/bash
export NCPU=`nproc`
make clean && make -j${NCPU} all
make objdump_linux
exit 0

