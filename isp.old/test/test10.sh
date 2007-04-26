#!/bin/bash -x

test -x /usr/bin/srun || exit 2

srcxml 2 $1 $2 | /usr/bin/srun -u -N1 dd bs=$3 | sinkxml 2 $1 $2 || exit 1

exit 0
