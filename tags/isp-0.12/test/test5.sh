#!/bin/bash -x

test "$2" = "--srun" && ! test -x /usr/bin/srun && exit 2

i=0
while test $i -lt $1; do
	filename=`printf "%-4.4d.txt" $i`
	cp /etc/passwd $filename
	i=`expr $i + 1`
done

find . -name \*.txt | ispcat \
	     | isprun $2 -- ispexec sort \
	     | isprun $2 -- ispexec bzip2 \
	     | isprename >out.xml || exit 1


test `grep '<unit>' out.xml | wc -l` -eq $1 || exit 1

exit 0
