#!/bin/bash -x

srcxml 2 100 100 2 | sinkxml 2 100 100 || exit 1

exit 0
