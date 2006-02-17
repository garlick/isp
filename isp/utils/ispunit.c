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

/* Create a single work unit according to arguments (or dup n times).
 * FIXME: error in optarg conversion isn't reported until after init handshake.
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

#define OPT_STRING "i:d:s:f:u:n:"
static const struct option long_options[] = {
    {"uint64", required_argument, 0, 'u'},
    {"int64", required_argument, 0, 'i'},
    {"double", required_argument, 0, 'd'},
    {"string", required_argument, 0, 's'},
    {"file", required_argument, 0, 'f'},
    {"numunits", required_argument, 0, 'n'},
    {0,0,0,0},
};
static const struct option *longopts = long_options;

static char *progname = NULL;

struct mystab_struct {
    struct isp_stab_struct *stab;
    char **val;
    int len;
};
typedef struct mystab_struct *mystab_t;

static void
_mystab_create(mystab_t *sp)
{
    mystab_t s = calloc(1, sizeof(mystab_t));

    if (!s) {
        fprintf(stderr, "%s: out of memory\n", progname);
        exit(1);
    }
    *sp = s;
}

static void
_mystab_destroy(mystab_t s)
{
    int i;

    for (i = 0; i < s->len; i++) {
        free(s->val[i]);
        free(s->stab[i].name);
    }
    free(s->val);
    free(s->stab);
    free(s);
}

static void
_parse(char *str, char **key, char **val)
{
    *key = str;
    *val = strchr(str, '=');
    if (*val == NULL) {
        fprintf(stderr, "%s: %s: not in key=value format\n", progname, str);
        exit(1);
    }
    **val = '\0';
    (*val)++;
    if (strlen(*key) == 0 || strlen(*val) == 0) {
        fprintf(stderr, "%s: %s=%s: not in key=value format\n", progname, *key, *val);
        exit(1);
    }
}

static void 
_mystab_add(mystab_t s, char *str, isp_type_t type)
{
    if (s->len == 0) {
        s->len = 1;
        s->val = (char **)malloc(sizeof(char *) * s->len);
        s->stab = (struct isp_stab_struct *)malloc(sizeof(struct isp_stab_struct) * s->len);
        if (!s->val || !s->stab) {
            fprintf(stderr, "%s: out of memory\n", progname);
            exit(1);
        }
    } else {
        s->len++;
        s->val = realloc(s->val, sizeof(char *) * s->len);
        s->stab = realloc(s->stab, sizeof(struct isp_stab_struct) * s->len);
        if (!s->val || !s->stab) {
            fprintf(stderr, "%s: out of memory\n", progname);
            exit(1);
        }
    }
    if (str) {
        char *key, *val;

        _parse(str, &key, &val);

        if (!(s->val[s->len - 1] = strdup(val))) {
            fprintf(stderr, "%s: out of memory\n", progname);
            exit(1);
        }
        if (!(s->stab[s->len - 1].name = strdup(key))) {
            fprintf(stderr, "%s: out of memory\n", progname);
            exit(1);
        }
        s->stab[s->len - 1].type = type;
        s->stab[s->len - 1].flags = ISP_PROVIDES;
    } else {
        s->stab[s->len - 1].name = NULL;
    }
}

static void 
usage(void)
{
    fprintf(stderr, "Usage: %s [-n #] [-i|u|d|s|f] key=value ...\n", progname);
    exit(1);
}

static void
_source(isp_unit_t u, char *key, char *val, isp_type_t type)
{
    int res;

    switch (type) {
        case ISP_FILE:
            if ((res = isp_file_source(u, key, val, 1)) != ISP_ESUCCESS)
                isp_errx(1, "isp_file_source: %s", isp_errstr(res));
            break;
        case ISP_INT64:
            {
                long long v = strtoll(val, NULL, 10);

                if (v == LONG_LONG_MIN && errno == ERANGE)
                    isp_errx(1, "int64 value underflow");
                if (v == LONG_LONG_MAX && errno == ERANGE)
                    isp_errx(1, "int64 value overflow");

                res = isp_meta_source(u, key, ISP_INT64, v);
            }
            break;
        case ISP_UINT64:
            {
                unsigned long long v = strtoull(val, NULL, 10);

                if (v == ULONG_LONG_MAX && errno == ERANGE)
                    isp_errx(1, "uint64 value overflow");

                res = isp_meta_source(u, key, ISP_UINT64, v);
            }
            break;
        case ISP_DOUBLE:
            {
                double v = strtod(val, NULL);

                if (fabs(v) == HUGE_VAL && errno == ERANGE)
                    isp_errx(1, "double value overflow");
                if (v == 0 && errno == ERANGE)
                    isp_errx(1, "double value underflow");

                res = isp_meta_source(u, key, ISP_DOUBLE, v);
            }
            break;
        case ISP_STR:
            res = isp_meta_source(u, key, ISP_STR, val);
            break;
    }
    if (res != ISP_ESUCCESS)
        isp_errx(1, "isp_meta_source: %s", isp_errstr(res));
}

int 
main(int argc, char *argv[])
{
    int c;
    int longindex;
    int res;
    isp_unit_t u;
    isp_handle_t h;
    mystab_t mystab;
    int numunits = 1;
    int i, j;
   
    opterr = 0;
    progname = basename(argv[0]);

    _mystab_create(&mystab);

    while ((c = getopt_long(argc, argv, OPT_STRING, longopts, 
                    &longindex)) != -1) { 
        switch (c) { 
            case 'f':   /* --file */
                _mystab_add(mystab, optarg, ISP_FILE);
                break;
            case 's':   /* --string */
                _mystab_add(mystab, optarg, ISP_STR);
                break;
            case 'd':   /* --double */
                _mystab_add(mystab, optarg, ISP_DOUBLE);
                break;
            case 'i':   /* --int64 */
                _mystab_add(mystab, optarg, ISP_INT64);
                break;
            case 'u':   /* --uint64 */
                _mystab_add(mystab, optarg, ISP_UINT64);
                break;
            case 'n':   /* --numunits */
                numunits = strtoul(optarg, NULL, 10);
                if (numunits == ULONG_MAX && errno == ERANGE) {
                    fprintf(stderr, "%s: numunits value overflow", progname);
                    exit(1);
                }
                break;
            default:
                usage();
                /*NOTREACHED*/
        }
    }
    _mystab_add(mystab, NULL, 0); /* add terminating null */

    res = isp_init(&h, ISP_SOURCE, argc, argv, mystab->stab, 1);
    if (res != ISP_ESUCCESS)
        isp_errx(1, "isp_init: %s", isp_errstr(res));

    for (j = 0; j < numunits; j++) {
        if ((res = isp_unit_create(&u)) != ISP_ESUCCESS)
            isp_errx(1, "isp_unit_create: %s", isp_errstr(res));
        if ((res = isp_unit_init(u)) != ISP_ESUCCESS)
            isp_errx(1, "isp_unit_init: %s", isp_errstr(res));
        for (i = 0; i < mystab->len - 1; i++) {
            _source(u, mystab->stab[i].name, mystab->val[i],  
                    mystab->stab[i].type);
        }
        if ((res = isp_unit_fini(u, ISP_ESUCCESS)) != ISP_ESUCCESS)
            isp_errx(1, "isp_unit_fini: %s", isp_errstr(res));

        if ((res = isp_unit_write(h, u)) != ISP_ESUCCESS)
            isp_errx(1, "isp_unit_write: %s", isp_errstr(res));
        if ((res = isp_unit_destroy(u)) != ISP_ESUCCESS)
            isp_errx(1, "isp_unit_destroy: %s", isp_errstr(res));
    }

    if ((res = isp_unit_write(h, NULL)) != ISP_ESUCCESS)
        isp_errx(1, "isp_unit_write: %s", isp_errstr(res));
    if ((res = isp_fini(h)) != ISP_ESUCCESS)
        isp_errx(1, "isp_fini: %s", isp_errstr(res));


    _mystab_destroy(mystab);

    exit(0);
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
