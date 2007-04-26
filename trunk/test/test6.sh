#!/bin/bash -x

# introduce corruption into file in the middle of the pipeline
# isprename detects it and flags it in the xml result
# (not fatal to the pipeline)
cp /etc/passwd foo.txt

export ISP_MD5CHECK=$1

ispcat foo.txt | ispexec sort | ispexec bzip2 \
	     | corruptfile | isprename >out.xml || exit 1

# ISP_ECORRUPT == 4
if test "$1" -eq 1; then
	grep "code=\"4\"" out.xml >/dev/null || exit 1
else
	grep "code=\"4\"" out.xml >/dev/null && exit 1
fi

exit 0
