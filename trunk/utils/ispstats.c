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

/* Summarize execution stats for each filter.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#define _GNU_SOURCE
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <assert.h>

#include <isp/util.h>
#include <isp/isp.h>
#include <isp/isp_private.h>

typedef struct {
    int count;              /* count of units sucessfully passed thru filter */
    int scale;              /*   (actual count is count/scale) */

    unsigned long smin;     /* system time min(x) */
    unsigned long smax;     /* system time max(x) */
    double ssum;            /* system time sum(x) */
    double ssqsum;          /* system time sum(x^2) */

    unsigned long umin;     /* user time min(x) */
    unsigned long umax;     /* user time max(x) */
    double usum;            /* user time sum(x) */
    double usqsum;          /* user time sum(x^2) */

    unsigned long rmin;     /* real time min(x) */
    unsigned long rmax;     /* real time max(x) */
    double rsum;            /* real time sum(x) */
    double rsqsum;          /* real time sum(x^2) */
} stats_t;

typedef struct {
    stats_t *stats;
    int nel;
} args_t;

static stats_t *
_stats_create(int nel)
{
    int i;
    int size = nel * sizeof(stats_t);
    stats_t *new = (stats_t *)malloc(size);

    if (new) {
        memset(new, 0, size); /* max values initialized to zero */
        for (i = 0; i < nel; i++) {
            new[i].umin = ULONG_MAX;
            new[i].smin = ULONG_MAX;
            new[i].rmin = ULONG_MAX;
        }
    }

    return new;
}

static void 
_stats_destroy(stats_t *s)
{
    free(s);
}

#if 0
/* presumes normal distribution */
static double 
_stddev(int N, double sum, double sqsum)
{
    assert(N > 1);
    return sqrt( (N*sqsum - pow(sum,2)) / pow(N,2) );
}
#else
static double 
_stddev(int N, double sum, double sqsum)
{
    double mean, variance;

    assert(N > 1);

    mean = sum/(double)N;
    variance = (1.0/((double)N-1.0)) * (sqsum - (sum*sum)/(double)N);

    return sqrt(variance);
}
#endif

static double 
_mean(int N, double sum)
{
    if (N < 1)
        return 0.0;
    return (sum / N);
}

static void 
_printheader(void)
{
    isp_err("%-9s %-5s %-9s %-9s %-9s %-9s", 
        "fid-type", "units", "min(s)", "mean(s)", "max(s)", "stddev(s)");
}

static void 
_printnull(int totflag, int fid, char *str, int count)
{
    char *fidstr;
    int n;
   
    if (totflag)
        n = asprintf(&fidstr, "tot-%s", str);
    else
        n = asprintf(&fidstr, "%d-%s", fid, str);
    if (n < 0)
        isp_errx(1, "out of memory");
    isp_err("%-9s %-5d %-9s %-9s %-9s %-9s", 
                fidstr, count, "-", "-", "-", "-");
    free(fidstr);
}

static void 
_printline(int totflag, int fid, char *str, int count, int scale,
                        double min, double mean, double max, double stddev)
{
    int n;
    char *fidstr;
   
    if (totflag)
        n = asprintf(&fidstr, "tot-%s", str);
    else
        n = asprintf(&fidstr, "%d-%s", fid, str);
    if (n < 0)
        isp_errx(1, "out of memory");
    isp_err("%-9s %-5d %-9.2f %-9.2f %-9.2f %-9.2f", 
            fidstr, count/scale, min, mean, max, stddev);
    free(fidstr);
}

static void 
summarize_stats(args_t *a)
{
    int fid;

    _printheader();

    for (fid = 0; fid < a->nel; fid++) {
        double usd, ssd, rsd;
        double um, sm, rm;
        int tflag = (fid == a->nel - 1);

        if (a->stats[fid].count > 0) {
        
            um = _mean(a->stats[fid].count, a->stats[fid].usum);
            sm = _mean(a->stats[fid].count, a->stats[fid].ssum);
            rm = _mean(a->stats[fid].count, a->stats[fid].rsum);
                
            usd = _stddev(a->stats[fid].count, a->stats[fid].usum, 
                          a->stats[fid].usqsum);
            ssd = _stddev(a->stats[fid].count, a->stats[fid].ssum, 
                          a->stats[fid].ssqsum);
            rsd = _stddev(a->stats[fid].count, a->stats[fid].rsum, 
                          a->stats[fid].rsqsum);

            _printline(tflag, fid, "user", 
                    a->stats[fid].count, a->stats[fid].scale,
                    (double)a->stats[fid].umin/1000, 
                    um/1000, 
                    (double)a->stats[fid].umax/1000,
                    usd/1000);

            _printline(tflag, fid, "sys", 
                    a->stats[fid].count, a->stats[fid].scale,
                    (double)a->stats[fid].smin/1000, 
                    sm/1000, 
                    (double)a->stats[fid].smax/1000,
                    ssd/1000);

            _printline(tflag, fid, "real", 
                    a->stats[fid].count, a->stats[fid].scale,
                    (double)a->stats[fid].rmin/1000, 
                    rm/1000, 
                    (double)a->stats[fid].rmax/1000,
                    rsd/1000);
        } else {
            _printnull(tflag, fid, "user", 0);
            _printnull(tflag, fid, "sys", 0);
            _printnull(tflag, fid, "real", 0);
        }
    }
}

static int 
do_stats(isp_unit_t u, void *arg)
{
    args_t *a = (args_t *)arg;
    int fid;
    unsigned long utime, stime, rtime;
    unsigned long tutime = 0, tstime = 0, trtime = 0;
    int error = 0;
    int res;
    int result;

    for (fid = 0; fid < a->nel; fid++) {

        if (fid < a->nel - 1) {
            res = isp_result_get(u, fid, &utime, &stime, &rtime, &result);
            if (res != ISP_ESUCCESS)
                isp_errx(1, "isp_result_get: %s", isp_errstr(res));
            if (result != ISP_ESUCCESS) {
                /* upstream failure - take note and don't munge stats */
                error++;
                continue;
            }
            tutime += utime;
            tstime += stime;
            trtime += rtime;
        } else {
            if (error > 0)
                continue;
            utime = tutime;
            stime = tstime;
            rtime = trtime;
        }
        a->stats[fid].count++;

        if (a->stats[fid].umin > utime)
            a->stats[fid].umin = utime;
        if (a->stats[fid].smin > stime)
            a->stats[fid].smin = stime;
        if (a->stats[fid].rmin > rtime)
            a->stats[fid].rmin = rtime;

        if (a->stats[fid].umax < utime)
            a->stats[fid].umax = utime;
        if (a->stats[fid].smax < stime)
            a->stats[fid].smax = stime;
        if (a->stats[fid].rmax < rtime)
            a->stats[fid].rmax = rtime;

        a->stats[fid].usum += (double)utime;
        a->stats[fid].ssum += (double)stime;
        a->stats[fid].rsum += (double)rtime;

        a->stats[fid].usqsum += pow((double)utime,2);
        a->stats[fid].ssqsum += pow((double)stime,2);
        a->stats[fid].rsqsum += pow((double)rtime,2);

    }

    return res;
}

/* Find the total scaling for the pipeline (units out/units in).
 */
static int 
_totscale(void)
{
    int res;
    isp_init_t i;
    isp_filter_t f;
    int fid, maxfid = isp_filterid_get();
    int factor, totscale = 1;
    
    if ((res = isp_init_get(&i)) != ISP_ESUCCESS)
        isp_errx(1, "isp_init_get: %s", isp_errstr(res));

    for (fid = 0; fid <= maxfid; fid++) {
        if ((res = isp_init_find(i, fid, &f)) != ISP_ESUCCESS)
            isp_errx(1, "isp_init_find: %s", isp_errstr(res));
        if ((res = isp_filter_splitfactor_get(f, &factor)) != ISP_ESUCCESS)
            isp_errx(1, "isp_filter_splitfactor_get: %s", isp_errstr(res));
        totscale *= factor;
    }
    return totscale;
}

/* Set scale for each a->stats[] element.
 * In calculating statistics, the unit count will be divided by the scale.
 * Example: filter 0: 1 in, 1 out
 *          filter 1: 1 in, 10 out (improperly multiplies filter 0's results)
 *          filter 2: 1 in, 10 out (improperly multiplies filter 0,1's results)
 *          filter 3: 1 in, 1 out
 * Whole pipeline:    1 in, 100 out
 * Filter 0's unit count should be divided by 100.
 * Filter 1's unit count should be divided by 10.
 * Filter 2's unit count should be taken at face value.
 * Filter 3's unit count should be taken at face value.
 */
static void
_stats_setscale(args_t *a)
{
    int res;
    isp_init_t i;
    isp_filter_t f;
    int fid, maxfid = isp_filterid_get();
    int factor, scale = _totscale();

    if ((res = isp_init_get(&i)) != ISP_ESUCCESS)
        isp_errx(1, "isp_init_get: %s", isp_errstr(res));

    assert(maxfid == a->nel - 1);
    for (fid = 0; fid <= maxfid; fid++) {
        if ((res = isp_init_find(i, fid, &f)) != ISP_ESUCCESS)
            isp_errx(1, "isp_init_find: %s", isp_errstr(res));
        if ((res = isp_filter_splitfactor_get(f, &factor)) != ISP_ESUCCESS)
            isp_errx(1, "isp_filter_splitfactor_get: %s", isp_errstr(res));
        scale /= factor;
        a->stats[fid].scale = scale;
    }
    assert(scale == 1);
}

int 
main(int argc, char *argv[])
{
    args_t a;
    int res;
    isp_handle_t h;
    int flags = ISP_SOURCE | ISP_SINK | ISP_IGNERR;

    res = isp_init(&h, flags, argc, argv, NULL, 1);
    if (res != ISP_ESUCCESS)
        isp_errx(1, "isp_init: %s", isp_errstr(res));

    a.nel = isp_filterid_get() + 1; /* 0 thru fid - 1 plus summary */
    if (!(a.stats = _stats_create(a.nel)))
        isp_errx(1, "out of memory");

    _stats_setscale(&a);

    if ((res = isp_unit_map(h, do_stats, &a)) != ISP_ESUCCESS)
        isp_errx(1, "isp_unit_map: %s", isp_errstr(res));

    summarize_stats(&a);
    _stats_destroy(a.stats);

    if ((res = isp_fini(h)) != ISP_ESUCCESS)
        isp_errx(1, "isp_fini: %s", isp_errstr(res));

    exit(0);
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
