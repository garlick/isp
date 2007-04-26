#!/bin/bash -x

test -x /usr/bin/xmllint || exit 2

touch testfile1 testfile2 testfile3

ispcat testfile1 testfile2 testfile3 | tee xml.out | xmllint -noout - || exit 1
exit 0
