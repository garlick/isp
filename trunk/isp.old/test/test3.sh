#!/bin/bash -x

touch testfile1
ispcat testfile1 | ispexec cat | isprename >out.xml || exit 1
test -f testfile1 || exit 1
test -f testfile1.out || exit 1
test `ls -1 isptmp* | wc -l` -eq 0 || exit 1

exit 0
