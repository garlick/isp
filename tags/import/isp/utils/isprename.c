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

/* Rename files to <basename>.<extn>.
 * For example, "isprename -e log -f log" might rename a log file associated 
 * with img4234.fits to img4234.log:
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#define _GNU_SOURCE
#include <stdlib.h>
#include <libgen.h>
#include <stdio.h>
#include <getopt.h>
#include <stdarg.h>

#include <isp/isp.h>

#define OPT_STRING "f:b:e:"
static const struct option long_options[] = {
    {"filekey", required_argument, 0, 'f'},
    {"basekey", required_argument, 0, 'b'},
    {"extension", required_argument, 0, 'e'},
    {0, 0, 0, 0},
};
static const struct option *longopts = long_options;

static char *basekey = "basename";
static char *filekey = "file";
static char *extension = "out";

/* renames file referenced by key to basename.extn */
static int
_file_rename(isp_unit_t u, char *key, char *basename, char *extn)
{
    char *newpath = NULL;
    int res = ISP_ESUCCESS;

    if (asprintf(&newpath, "%s.%s", basename, extn) < 0) {
        isp_err("out of memory");
        res = ISP_ENOMEM;
        goto done;
    }
    if ((res = isp_file_rename(u, key, newpath)) != ISP_ESUCCESS) {
        isp_err("isp_file_rename: %s", isp_errstr(res));
        goto done;
    }

done:
    if (newpath)
        free(newpath);
    return res;
}

static int 
rename_files(isp_unit_t u, void *arg)
{
    int res = ISP_ESUCCESS;
    char *basename = NULL;

    if ((res = isp_meta_get(u, basekey, ISP_STR, &basename)) != ISP_ESUCCESS)
        isp_err("isp_meta_get %s: %s", basekey, isp_errstr(res));
    else
        res = _file_rename(u, filekey, basename, extension);
    if (basename)
        free(basename);
    return res;
}

static void 
usage(void)
{
    fprintf(stderr, "Usage: isprename [-b basekey] [-f filekey] [-e extn]\n");
    exit(1);
}

static void
_initialize(isp_handle_t *hp, int flags, int argc, char *argv[])
{
    struct isp_stab_struct stab[] = {
        {filekey, ISP_FILE, ISP_REQUIRES },
        {basekey, ISP_STR,  ISP_REQUIRES },
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
    int res;
    isp_handle_t h;
    int flags = ISP_SOURCE | ISP_SINK;

    opterr = 0;
    while ((c = getopt_long(argc, argv, OPT_STRING, longopts,
                                            &longindex)) != -1) {
        switch (c) {
            case 'b':       /* --basekey */
                basekey = optarg;
                break;
            case 'f':       /* --filekey */
                filekey = optarg;
                break;
            case 'e':       /* --extension */
                extension = optarg;
                break;
            default:
                usage();
        }
    }
    if (optind < argc)
        usage();

    _initialize(&h, flags, argc, argv);

    if ((res = isp_unit_map(h, rename_files, NULL)) != ISP_ESUCCESS)
        isp_errx(1, "isp_unit_map: %s", isp_errstr(res));

    _finalize(h);

    exit(0);
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
