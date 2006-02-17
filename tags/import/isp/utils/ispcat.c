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

/* Create a stream of units from a file glob on command line,
 * or a list of files on stdin.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <unistd.h>
#include <assert.h>
#include <dirent.h>

#include <isp/isp.h>

#define OPT_STRING "rf:b:"
static const struct option long_options[] = {
    {"recursive", no_argument, 0, 'r'},
    {"filekey", required_argument, 0, 'f'},
    {"basekey", required_argument, 0, 'b'},
    {0,0,0,0},
};
static const struct option *longopts = long_options;

static char *filekey = "file";
static char *basekey = "basename";

static char *progname = NULL;

static void 
usage(void)
{
    fprintf(stderr, 
            "Usage: %s [-r] [-f filekey] [-b basekey] file1 [file2...]\n", 
            progname);
    exit(1);
}

static void 
ispcat(isp_handle_t h, char *path, int recursive)
{
    struct stat sb;

    if (stat(path, &sb) < 0)
        isp_errx(1, "%s: %m", path);

    if (recursive && S_ISDIR(sb.st_mode)) {
        DIR *dir;
        struct dirent *d;
        char *newpath;

        if (!(dir = opendir(path)))
            isp_errx(1, "%s: %m", path);

        while ((d = readdir(dir))) {
            if (!strcmp(d->d_name, ".") || !strcmp(d->d_name, ".."))
                continue;
            if (asprintf(&newpath, "%s/%s", path, d->d_name) < 0)
                isp_errx(1, "out of memory");
            ispcat(h, newpath, recursive); /* RECURSE */
            free(newpath);
        }
        if (closedir(dir) < 0)
            isp_errx(1, "%s: %m", path);

    } else if (! S_ISREG(sb.st_mode)) {
        isp_errx(1, "%s: not a regular file", path);

    } else { /* regular file */
        isp_unit_t u;
        int res;
        char *p, *base;

        if (access(path, R_OK) != 0)
            isp_errx(1, "%s: no read access", path);
        if ((res = isp_unit_create(&u)) != ISP_ESUCCESS)
            isp_errx(1, "isp_unit_create: %s", isp_errstr(res));
        if ((res = isp_unit_init(u)) != ISP_ESUCCESS)
            isp_errx(1, "isp_unit_init: %s", isp_errstr(res));
        if ((res = isp_file_source(u, filekey, path, ISP_RDONLY)) != ISP_ESUCCESS)
            isp_errx(1, "isp_file_source: %s", isp_errstr(res));
        if (!(base = strdup(path)))
            isp_errx(1, "out of memory");
        if ((p = strrchr(base, '.')) && p != base)
            *p = '\0';
        if ((res = isp_meta_source(u, basekey, ISP_STR, basename(base))) != ISP_ESUCCESS)
            isp_errx(1, "isp_meta_source: %s", isp_errstr(res));
        free(base);
        if ((res = isp_unit_fini(u, ISP_ESUCCESS)) != ISP_ESUCCESS)
            isp_errx(1, "isp_unit_fini: %s", isp_errstr(res));
        if ((res = isp_unit_write(h, u)) != ISP_ESUCCESS)
            isp_errx(1, "isp_unit_write: %s", isp_errstr(res));
        isp_unit_destroy(u);
    }
}

static void
_initialize(isp_handle_t *hp, int flags, int argc, char *argv[])
{
    struct isp_stab_struct stab[] = {
        {filekey, ISP_FILE, ISP_PROVIDES},
        {basekey, ISP_STR,  ISP_PROVIDES},
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

    if ((res = isp_unit_write(h, NULL)) != ISP_ESUCCESS)
        isp_errx(1, "isp_unit_write: %s", isp_errstr(res));
    if ((res = isp_fini(h)) != ISP_ESUCCESS)
        isp_errx(1, "isp_fini: %s", isp_errstr(res));
}

int 
main(int argc, char *argv[])
{
    int c;
    int longindex;
    int recursive = 0;
    isp_handle_t h;
    int flags = ISP_SOURCE;

    opterr = 0;
    progname = basename(argv[0]);
    while ((c = getopt_long(argc, argv, OPT_STRING, longopts, 
                    &longindex)) != -1) { 
        switch (c) { 
            case 'r':   /* --recursive */
                recursive = 1;
                break;
            case 'f':   /* --filekey */
                filekey = optarg;
                break;
            case 'b':   /* --basekey */
                basekey = optarg;
                break;
            default:
                usage();
                /*NOTREACHED*/
        }
    }
    if (optind == argc && isatty(STDIN_FILENO))
        usage();

    _initialize(&h, flags, argc, argv);

    /* process list of files on cmd line */
    if (optind < argc) {
        while (optind < argc)
            ispcat(h, argv[optind++], recursive);

    /* process list of files on stdin */
    } else {
        char buf[MAXPATHLEN+1];

        while (fgets(buf, sizeof(buf), stdin) != NULL) {
            if (buf[strlen(buf) - 1] == '\n')
                buf[strlen(buf) - 1] = '\0';
            if (strlen(buf) > 0)
                ispcat(h, buf, recursive);
        }
    } 

    _finalize(h);

    exit(0);
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
