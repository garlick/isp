#!/bin/bash -x

i=0
while test $i -lt $1; do
	filename=`printf "%-4.4d.txt" $i`
	cp /etc/passwd $filename
	i=`expr $i + 1`
done
ls
find . -name \*.txt | ispcat | ispexec sort | ispexec bzip2 \
	     | ispstats | isprename >out.xml || exit 1

test `grep '<unit>' out.xml | wc -l` -eq $1 || exit 1

exit 0
