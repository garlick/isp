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

/* Given a number of "expected" units, show an ascii progress bar
 * on stderr indicating percentage complete.
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
#include <unistd.h>
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <math.h>

#include <isp/isp.h>
#include "progress.h"

#define OPT_STRING "n:l:"
static const struct option long_options[] = {
    {"numunits", required_argument, 0, 'n'},
    {"label", required_argument, 0, 'l'},
    {0,0,0,0},
};
static const struct option *longopts = long_options;

static char *progname = NULL;
static prog_t pctx;
static char *label = "progress";

static void 
usage(void)
{
    fprintf(stderr, "Usage: %s -n numunits [-l label]\n", progname);
    exit(1);
}

static int
progress(isp_unit_t u, void *arg)
{
    unsigned long numunits = *(unsigned long *)arg;
    static unsigned long count = 0;

    count++;
    progress_update(pctx, (double)count/numunits);

    return ISP_ESUCCESS;
}

int 
main(int argc, char *argv[])
{
    int c;
    int longindex;
    int res;
    isp_handle_t h;
    unsigned long numunits = 0;
   
    opterr = 0;
    progname = basename(argv[0]);

    while ((c = getopt_long(argc, argv, OPT_STRING, longopts, 
                    &longindex)) != -1) { 
        switch (c) { 
            case 'n':   /* --numunits */
                numunits = strtoul(optarg, NULL, 10);
                if (numunits == ULONG_MAX && errno == ERANGE) {
                    fprintf(stderr, "%s: numunits value overflow", progname);
                    exit(1);
                }
                break;
            case 'l':   /* --label */
                label = optarg;
                break;
            default:
                usage();
                /*NOTREACHED*/
        }
    }
    if (numunits == 0) 
        usage();
    if (optind < argc)
        usage();

    res = isp_init(&h, ISP_SOURCE|ISP_SINK, argc, argv, NULL, 1);
    if (res != ISP_ESUCCESS)
        isp_errx(1, "isp_init: %s", isp_errstr(res));

    fprintf(stderr,"%s: ", label);
    progress_create(&pctx, 79 - strlen(label) - 2);

    if ((res = isp_unit_map(h, progress, &numunits)) != ISP_ESUCCESS)
        isp_errx(1, "isp_map: %s", isp_errstr(res));

    progress_destroy(pctx);

    if ((res = isp_fini(h)) != ISP_ESUCCESS)
        isp_errx(1, "isp_fini: %s", isp_errstr(res));

    exit(0);
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
