#!/bin/bash

runtest()
{
	tmpdir=`mktemp -d -p $TESTDIR isptest.XXXXXXXXXX`
	cd $tmpdir || exit 1
	printf "%.50s" "$1...................................................."
	shift
	eval $* 2>log.err 1>log.out
	case $? in
		0) 	echo "[PASS]"
			cd .. || exit 1
			#rm -r $tmpdir
			;;
		1) 	echo "[FAIL] see `basename $tmpdir`"
			;;
		2) 	echo "[NOTRUN]"
			;;
		*) 	echo "[UNKNOWN]"
			;;
	esac
}

export PATH=`pwd`/../utils:`pwd`/../slurm:`pwd`/../test:$PATH
export TESTDIR=`pwd`
export ISP_DBGFAIL=1

runtest "ispcat emits well-formed XML"                   test1.sh
runtest "isprename copies read-only files"               test2.sh
runtest "isprename doesn't copy read-write files"        test3.sh
runtest "run 10 files thru a pipeline"                   test4.sh 10
runtest "run 10 files thru a direct || pipeline"         test5.sh 10 --direct
runtest "run 10 files thru a slurm || pipeline"          test5.sh 10 --srun
runtest "catch file corruption with ISP_MD5CHECK=1"      test6.sh 1
runtest "ignore file corruption with ISP_MD5CHECK=0"     test6.sh 0
runtest "src|dd|sink 1000 XML elements (bs=10)"          test7.sh 1000 100 10
runtest "src|dd|sink 1000 XML elements (bs=100)"         test7.sh 1000 100 100
runtest "src|dd|sink 1000 XML elements (bs=4k)"          test7.sh 1000 100 4k
runtest "test for SIGPIPE on writer close delay"         test8.sh
runtest "test for errors on reader delay"                test9.sh
runtest "src|srun dd|sink 1000 XML elements (bs=10)"     test10.sh 1000 100 10
runtest "src|srun dd|sink 1000 XML elements (bs=100)"    test10.sh 1000 100 100
runtest "src|srun dd|sink 1000 XML elements (bs=4k)"     test10.sh 1000 100 4k

exit 0
