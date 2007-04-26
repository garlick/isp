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

/* Read one unit in, write 'factor' units out.
 * Put the split index in metadata called "split" (normally zero origin).
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#define _GNU_SOURCE
#include <stdarg.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdint.h>

#include <isp/util.h>
#include <isp/isp.h>

#define OPT_STRING "k:z:f:"
static const struct option long_options[] = {
    {"key", required_argument, 0, 'k'},
    {"zero", required_argument, 0, 'z'},
    {"factor", required_argument, 0, 'f'},
    {0,0,0,0},
};
static const struct option *longopts = long_options;

static char *prog = NULL;
static char *key = "split";

static void 
usage(void) 
{
    fprintf(stderr, "Usage %s [-k val] [-z N] -f N\n", prog);
    exit(1);
}

static void
_initialize(isp_handle_t *hp, int flags, int argc, char *argv[], 
            int factor)
{
    struct isp_stab_struct stab[] = {
        {key, ISP_UINT64, ISP_PROVIDES},
        {0,0,0},
    };
    int res;

    if ((res = isp_init(hp, flags, argc, argv, stab, factor)) != ISP_ESUCCESS)
        isp_errx(1, "isp_init: %s", isp_errstr(res));
}

static void
_finalize(isp_handle_t h)
{
    int res;

    if ((res = isp_unit_write(h, NULL)) != ISP_ESUCCESS)
        isp_errx(1, "isp_unit_write: %s", isp_errstr(res));
    if ((res = isp_fini(h)) == ISP_ESUCCESS)
        isp_errx(1, "isp_fini: %s", isp_errstr(res));
}

int 
main(int argc, char *argv[])
{
    int factor = 0; 
    int zero = 0;
    int res;
    isp_handle_t h;
    isp_unit_t u, new;
    int i;
    int c;
    int longindex;
    int flags = ISP_SOURCE | ISP_SINK | ISP_IGNERR;

    /* Parse options */
    prog = basename(argv[0]);
    while ((c = getopt_long(argc, argv, OPT_STRING, longopts, 
                    &longindex)) != -1) {
        switch (c) {
            case 'k':   /* --key */
                key = optarg;
                break;
            case 'z':   /* --zero */
                zero = strtol(optarg, NULL, 10);
                break;
            case 'f':   /* --factor */
                factor = strtol(optarg, NULL, 10);
                if (factor < 1)
                    usage();
                break;
            default:
                usage();
                break;
        }
    }
    if (factor == 0)
        usage();

    _initialize(&h, flags, argc, argv, factor);

    while ((res = isp_unit_read(h, &u)) == ISP_ESUCCESS) {

        /* Replicating references to read-write files could cause races,
         * so we turn that into a hard error here.
         */
        if (isp_rwfile_check(u) != ISP_ESUCCESS)
            isp_errx(1, "isp_rwfile_check: %s", isp_errstr(res));

        for (i = 0; i < factor; i++) {
            if ((res = isp_unit_copy(&new, u)) != ISP_ESUCCESS)
                isp_errx(1, "isp_unit_copy: %s", isp_errstr(res));

            if ((res = isp_unit_init(new)) != ISP_ESUCCESS)
                isp_errx(1, "isp_unit_init: %s", isp_errstr(res));

            if ((res = isp_meta_source(new, key, ISP_UINT64, (uint64_t)i+zero)) != ISP_ESUCCESS)
                isp_errx(1, "isp_meta_source: %s", isp_errstr(res));

            if ((res = isp_unit_fini(new, ISP_ESUCCESS)) != ISP_ESUCCESS)
                isp_errx(1, "isp_unit_init: %s", isp_errstr(res));

            if ((res = isp_unit_write(h, new)) != ISP_ESUCCESS)
                isp_errx(1, "isp_unit_write: %s", isp_errstr(res));

            isp_unit_destroy(new);
        }
        isp_unit_destroy(u);
    }

    _finalize(h);

    exit(0);
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
