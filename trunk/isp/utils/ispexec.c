/*****************************************************************************\
 *  $Id$
 *****************************************************************************
 *  Copyright (C) 2005 The Regents of the University of California.
 *  Produced at Lawrence Livermore National Laboratory (cf, DISCLAIMER).
 *  Written by Jim Garlick <garlick@llnl.gov>.
 *  
 *  This file is part of ISP, a toolkit for constructing pipeline applications.
 *  For details, see <http://isp.sourceforge.net>.

 *  ISP is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *  
 *  ISP is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *  
 *  You should have received a copy of the GNU General Public License along
 *  with ISP; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
\*****************************************************************************/

/* This filter executes "cmd <infile >outfile".
 * "infile" is in the ISP stream on input; "outfile" is added if succesful.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <getopt.h>
#include <libgen.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <assert.h>
#include <signal.h>

#include <isp/util.h>
#include <isp/isp.h>

#define OPT_STRING "f:"
static const struct option long_options[] = {
    {"filekey", no_argument, 0, 'f'},
    {0,0,0,0},
};
static const struct option *longopts = long_options;

static char *progname = NULL;
static char *filekey = "file";

static void 
usage(void)
{
    fprintf(stderr, "Usage: %s [-f filekey] -- command [args...]\n", progname);
    exit(1);
}

static int 
runcmd(isp_unit_t u, void *arg)
{
    char **nargv = (char **)arg;
    int ifd, ofd;
    char *ipath, *opath;
    int errnum;
    int res;

    /* Fetch the input file path name and open it on 'ifd'.
     */
    res = isp_file_access(u, filekey, &ipath, ISP_RDONLY);
    if (res != ISP_ESUCCESS)
        goto done;
    if ((ifd = open(ipath, O_RDONLY)) < 0) {
        res = ISP_ENOENT;
        goto done;
    }

    /* Create the output file and open it on 'ofd'.
     */
    if ((res = util_mktmp(&ofd, &opath)) != ISP_ESUCCESS)
        goto done;

    /* Run the filter with stdin redirected from ifd, 
     * stdout redirected to ofd.  Stderr is not redirected.
     */
    res = util_runcmd(nargv, &errnum, ifd, ofd, FDIGNORE);
    if (res != ISP_ESUCCESS) {
        unlink(opath);
        goto done;
    }

    /* Close the input and output files.
     */
    if (close(ifd) < 0) {
        res = ISP_EREAD; /* unlikeley */
        unlink(opath);
        goto done;
    }
    if (close(ofd) < 0) {
        res = ISP_EWRITE;
        unlink(opath);
        goto done;
    }

    /* Source new file and sink old one.
     */
    if ((res = isp_file_sink(u, filekey)) != ISP_ESUCCESS) {
        unlink(opath);
        goto done;
    }
    res = isp_file_source(u, filekey, opath, ISP_RDWR);
    if (res != ISP_ESUCCESS) {
        unlink(opath);
        goto done;
    }

done:
    return res;
}

static void
_initialize(isp_handle_t *hp, int flags, int argc, char *argv[])
{
    struct isp_stab_struct stab[] = {
        {filekey,  ISP_FILE, ISP_REQUIRES },
        {0,0,0},
    };
    int res;

    if ((res = isp_init(hp, flags, argc, argv, stab, 1)) != ISP_ESUCCESS)
        isp_errx(1, "isp_init: %s", isp_errstr(res));
}

static void
_finalize(isp_handle_t h)
{
    int res;

    if ((res = isp_fini(h)) != ISP_ESUCCESS)
        isp_errx(1, "isp_fini: %s", isp_errstr(res));
}

int 
main(int argc, char *argv[])
{
    int c;
    int longindex;
    int flags = ISP_SOURCE | ISP_SINK;
    int res;
    isp_handle_t h;
    char **nargv;

    progname = basename(argv[0]);
    opterr = 0;
    while ((c = getopt_long(argc, argv, OPT_STRING, longopts, 
                    &longindex)) != -1) { 
        switch (c) { 
            case 'f':   /* --filekey */
                filekey = optarg;
                break;
            default:
                usage();
        }
    }

    if (optind >= argc)
        usage();
    res = util_argvdupc(argc - optind, argv + optind, &nargv);
    if (res != ISP_ESUCCESS)
        isp_errx(1, "util_argvdupc: %s", isp_errstr(res));

    _initialize(&h, flags, argc, argv);

    if ((res = isp_unit_map(h, runcmd, nargv)))
        isp_errx(1, "isp_unit_map: %s", isp_errstr(res));

    _finalize(h);

    free(nargv);

    exit(0);
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */

