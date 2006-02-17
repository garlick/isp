#!/bin/bash -x

srcxml 2 100 100 | sinkxml 2 100 100 2 || exit 1

exit 0
