#!/bin/bash
## 
## @(#) Test025, Client Process Monitor tests
## Currently will test for
## - Auto startup
## - Manual startup
## - Automatic restart
## - Shutdown
##
## @file run.sh
## 
## -----------------------------------------------------------------------------
## Enduro/X Middleware Platform for Distributed Transaction Processing
## Copyright (C) 2015, ATR Baltic, SIA. All Rights Reserved.
## This software is released under one of the following licenses:
## GPL or ATR Baltic's license for commercial use.
## -----------------------------------------------------------------------------
## GPL license:
## 
## This program is free software; you can redistribute it and/or modify it under
## the terms of the GNU General Public License as published by the Free Software
## Foundation; either version 2 of the License, or (at your option) any later
## version.
##
## This program is distributed in the hope that it will be useful, but WITHOUT ANY
## WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
## PARTICULAR PURPOSE. See the GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License along with
## this program; if not, write to the Free Software Foundation, Inc., 525 Temple
## Place, Suite 330, Boston, MA 02111-1307 USA
##
## -----------------------------------------------------------------------------
## A commercial use license is available from ATR Baltic, SIA
## contact@atrbaltic.com
## -----------------------------------------------------------------------------
##

export TESTNO="025"
export TESTNAME_SHORT="cpmsrv"
export TESTNAME="test${TESTNO}_${TESTNAME_SHORT}"

PWD=`pwd`
if [ `echo $PWD | grep $TESTNAME ` ]; then
	# Do nothing 
	echo > /dev/null
else
	# started from parent folder
	pushd .
	echo "Doing cd"
	cd $TESTNAME
fi;

. ../testenv.sh

export TESTDIR="$NDRX_APPHOME/atmitest/$TESTNAME"
export PATH=$PATH:$TESTDIR
# Configure the runtime - override stuff here!
export NDRX_CONFIG=$TESTDIR/ndrxconfig.xml
export NDRX_DMNLOG=$TESTDIR/ndrxd.log
export NDRX_LOG=$TESTDIR/ndrx.log
export NDRX_DEBUG_CONF=$TESTDIR/debug.conf
# Override timeout!
export NDRX_TOUT=13
# Test process count
PROC_COUNT=100

#
# Generic exit function
#
function go_out {
    echo "Test exiting with: $1"
    xadmin stop -y
    xadmin down -y

    # If some alive stuff left...
    killall -25 atmiclt_$TESTNO

    popd 2>/dev/null
    exit $1
}

rm *.log

xadmin down -y
xadmin start -y || go_out 1

xadmin bc -t TESTENV
xadmin bc -t TESTENV2

sleep 4

xadmin pc

# Test for section/subsection passing
OUT=`grep -E '.t TAG1 .s SUBSECTION1' testbin1-1.log`
if [[ "X$OUT" == "X" ]]; then
        echo "TESTERROR! invalid testbin1-1.log content!"
        go_out 1
fi

OUT=`grep -E '.t TAG2 .s SUBSECTION2' testbin1-2.log`
if [[ "X$OUT" == "X" ]]; then
        echo "TESTERROR! invalid testbin1-2.log content!"
        go_out 2
fi

# Test for env override processing
OUT=`grep ___ENV_OVER__1  env1.log`
if [[ "X$OUT" == "X" ]]; then
        echo "TESTERROR! invalid env1.log content!"
        go_out 3
fi

OUT=`grep ___ENV_OVER__2  env2.log`
if [[ "X$OUT" == "X" ]]; then
        echo "TESTERROR! invalid env2.log content!"
        go_out 4
fi

# Test for automatic restarts...
CNT=`wc -l testbin1-1.log`
echo "Process restarts: $CNT"
if [[ ( "$CNT" < 2 ) ]]; then 
        echo "TESTERROR! Automatic process reboots does not work!"
        go_out 5
fi

# Start some tons of processes
for i in `seq 1 $PROC_COUNT`; do
        xadmin bc -t WHILE -s $i
done

sleep 4

# have some sync (wait for startup to complete, print the output)
xadmin pc

CNT=`xadmin pc | grep -E "WHILE.*running pid" | wc -l`
echo "xadmin procs: $CNT"
if [[ ( "$CNT" != $PROC_COUNT ) ]]; then 
        echo "TESTERROR! $PROC_COUNT procs not booted (according to xadmin pc)!"
        go_out 6
fi

CNT=`ps -ef | grep whileproc.sh | grep -v grep | wc -l`
echo "ps -ef procs: $CNT"
if [[ ( "$CNT" != $PROC_COUNT ) ]]; then 
        echo "TESTERROR! $PROC_COUNT procs not booted (according to ps -ef )!"
        go_out 7
fi


# test signle binary shutdown 
xadmin sc -t IGNORE
# We should have a binary which does not react on kill signals
OUT=`grep SIGINT ignore.log`
if [[ "X$OUT" == "X" ]]; then
        echo "TESTERROR! no SIGINT in ignore.log!"
        go_out 9
fi

#boot it back for next test
> ignore.log

xadmin bc -t IGNORE
sleep 2

# Stop the cpmsrv (will do automatic process shutdown)...
xadmin stop -s cpmsrv

# We should have 0 now
CNT=`ps -ef | grep whileproc.sh | grep -v grep | wc -l`
echo "ps -ef procs: $CNT"
if [[ ( "$CNT" != 0 ) ]]; then 
        echo "TESTERROR! not all whileproc.sh stopped!"
        go_out 8
fi

# We should have a binary which does not react on kill signals
OUT=`grep SIGINT ignore.log`
if [[ "X$OUT" == "X" ]]; then
        echo "TESTERROR! no SIGINT in ignore.log!"
        go_out 9
fi

OUT=`grep SIGTERM ignore.log`
if [[ "X$OUT" == "X" ]]; then
        echo "TESTERROR! no SIGTERM in ignore.log!"
        go_out 10
fi

# process must be killed
CNT=`ps -ef | grep ignore.sh | grep -v grep | wc -l`
echo "ps -ef procs: $CNT"
if [[ ( "$CNT" != 0 ) ]]; then 
        echo "TESTERROR! ignore.sh not stopped!"
        go_out 11
fi

go_out 0

