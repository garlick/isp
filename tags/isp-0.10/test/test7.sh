#!/bin/bash -x

srcxml 4 $1 $2 | dd bs=$3 | sinkxml 4 $1 $2 || exit 1

exit 0
