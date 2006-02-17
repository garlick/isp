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

/* Delay some number of seconds for each unit.
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

#define OPT_STRING "d:"
static const struct option long_options[] = {
    {"delay", required_argument, 0, 'd'},
    {0,0,0,0},
};
static const struct option *longopts = long_options;

static char *progname = NULL;

static void 
usage(void)
{
    fprintf(stderr, "Usage: %s [-d seconds]\n", progname);
    exit(1);
}

static int
delay(isp_unit_t u, void *arg)
{
    unsigned int delay_sec = *(unsigned int *)arg;

    if (delay_sec > 0)
        sleep(delay_sec);

    return ISP_ESUCCESS;
}

int 
main(int argc, char *argv[])
{
    int c;
    int longindex;
    int res;
    isp_handle_t h;
    unsigned int delay_sec = 0;
   
    opterr = 0;
    progname = basename(argv[0]);

    while ((c = getopt_long(argc, argv, OPT_STRING, longopts, 
                    &longindex)) != -1) { 
        switch (c) { 
            case 'd':   /* --delay */
                delay_sec = strtol(optarg, NULL, 10);
                if (delay_sec == LONG_MAX && errno == ERANGE) {
                    fprintf(stderr, "%s: delay value overflow", progname);
                    exit(1);
                }
                if (delay_sec == LONG_MIN && errno == ERANGE) {
                    fprintf(stderr, "%s: delay value underflow", progname);
                    exit(1);
                }
                break;
            default:
                usage();
                /*NOTREACHED*/
        }
    }
    if (optind < argc)
        usage();

    res = isp_init(&h, ISP_SOURCE|ISP_SINK, argc, argv, NULL, 1);
    if (res != ISP_ESUCCESS)
        isp_errx(1, "isp_init: %s", isp_errstr(res));

    if ((res = isp_unit_map(h, delay, &delay_sec)) != ISP_ESUCCESS)
        isp_errx(1, "isp_map: %s", isp_errstr(res));

    if ((res = isp_fini(h)) != ISP_ESUCCESS)
        isp_errx(1, "isp_fini: %s", isp_errstr(res));

    exit(0);
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
