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

/* Parallelize an ISP stream by starting multiple copies of a filter
 * as coprocesses and feeding each of them one unit to work on concurrently.
 */

/* NOTE:
 * We can also run on a SLURM cluster by starting 'srun filter' instead of
 * 'filter' as coprocesses.  It should be straightforward to add support for 
 * other cluster resource managers.  We just need to be able to spawn a 
 * process and have it block until resources are available.
 */

/* NOTE: 
 * We configure stdout with unlimited backlog, so there is a danger that our
 * stdout backlog will grow to an obscene size.  On the other hand, we avoid
 * deadlock that would occur should stdout stall and we stop reading stdin of
 * the sruns--perhaps those sruns need to terminate so CPU's can be allocated
 * downstream to consume the stdout backlog.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdlib.h>
#define _GNU_SOURCE
#include <getopt.h>
#include <assert.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <libgen.h>

#include <isp/util.h>
#include <isp/isp.h>
#include <isp/isp_private.h>
#include <isp/list.h>

#define PATH_SRUN       "/usr/bin/srun"

#define DEFAULT_FANOUT  4

/* coprocess backlog limits */
#define IBACKLOG 1 /* stdin, coproc stdout: 1 unit */
#define OBACKLOG 0 /* stdout, coproc stdin: unlimited */

typedef struct par_handle_struct *par_handle_t;
typedef enum { 
    PROC_STARTING, /* coproc spawned but may be blocked waiting for resources */
    PROC_RUNNING,  /* !consuming && producing (unit=-1 has emerged) */
    PROC_COMPLETE, /* !consuming && !producing */
} procstate_t;

#define PAR_HANDLE_MAGIC 0xffea0012
struct par_handle_struct {
    int magic;
    pid_t pid;
    isp_handle_t h;
    int ifd;            /* pipe to coproc's stdin */
    int ofd;            /* pipe to coproc's stdout */
    int res;            /* last isp_unit_read result from coproc */
    procstate_t state;
};

typedef enum { RUNCMD_SRUN, RUNCMD_DIRECT } runcmd_t;

#define OPT_STRING "sdf:"
static const struct option long_options[] = {
    {"direct", no_argument, 0, 'd'},
    {"srun", no_argument, 0, 's'},
    {"fanout", required_argument, 0, 'f'},
    {0,0,0,0},
};
static const struct option *longopts = long_options;

void
usage(void)
{
    fprintf(stderr, "Usage: isprun [-f #] [-s|-d] -- isp filter [args]\n");
    exit(1);
}

/* Run the coproc through srun, letting slurm do the allocation.
 */
void
runcmd_srun(char **cmdargv, pid_t *pidp, int *ifdp, int *ofdp)
{
    char *srun[] = { PATH_SRUN, "--unbuffered", "--ntasks=1", NULL };
    char **av;
    int res;

    if ((res = util_argvcat(srun, cmdargv, &av)) != ISP_ESUCCESS)
        isp_errx(1, "util_argvcat: %s", isp_errstr(res));

    res = util_runcoproc(av, pidp, ifdp, ofdp, NULL);
    if (res != ISP_ESUCCESS)
        isp_errx(1, "util_runcoproc: %s", isp_errstr(res));

    free(av);
}

/* Run the coproc directly.
 */
void
runcmd_direct(char **cmdargv, pid_t *pidp, int *ifdp, int *ofdp)
{
    int res;

    res = util_runcoproc(cmdargv, pidp, ifdp, ofdp, NULL);
    if (res != ISP_ESUCCESS)
        isp_errx(1, "util_runcoproc: %s", isp_errstr(res));
}

static par_handle_t 
par_handle_create(runcmd_t how, char **cmdargv, isp_init_t i, isp_unit_t u)
{
    size_t size = sizeof(struct par_handle_struct);
    par_handle_t ph;
    int res;

    if ((ph = (par_handle_t)calloc(1, size)) == NULL)
        isp_errx(1, "par_handle_create: out of memory");
    ph->magic = PAR_HANDLE_MAGIC;

    assert(u != NULL && i != NULL);

    /* start the coprocess */
    switch (how) { 
        case RUNCMD_SRUN:
            runcmd_srun(cmdargv, &ph->pid, &ph->ifd, &ph->ofd);
            break;
        case RUNCMD_DIRECT:
            runcmd_direct(cmdargv, &ph->pid, &ph->ifd, &ph->ofd);
            break;
    }
    res = isp_handle_create(&ph->h, 
            ISP_SOURCE | ISP_SINK | ISP_NONBLOCK | ISP_PROXY, 
            IBACKLOG, OBACKLOG, ph->ofd, ph->ifd);
    if (res != ISP_ESUCCESS)
        isp_errx(1, "isp_handle_create: %s", isp_errstr(res));

#if (OBACKLOG != 0 && OBACKLOG < 3)
#error OBACKLOG must be 0 or >= 3
#endif
    /* Write init element, work unit, and NULL.
     * NOTE: even though we are in ISP_NONBLOCK mode, none of these calls 
     * should fail with ISP_EWOULDBLOCK because OBACKLOG is >= 3 and we 
     * haven't written anything before.
     */
    if ((res = isp_init_write(ph->h, i)) != ISP_ESUCCESS)
        isp_errx(1, "isp_init_write: %s", isp_errstr(res));
    if ((res = isp_unit_write(ph->h, u)) != ISP_ESUCCESS)
        isp_errx(1, "isp_unit_write: %s", isp_errstr(res));
    if ((res = isp_unit_write(ph->h, NULL)) != ISP_ESUCCESS)
        isp_errx(1, "isp_unit_write: %s", isp_errstr(res));

    if ((res = isp_unit_destroy(u)) != ISP_ESUCCESS)
        isp_errx(1, "isp_unit_destroy: %s", isp_errstr(res));

    ph->res = ISP_ESUCCESS;
    ph->state = PROC_STARTING;

    return ph;
}

static void
par_handle_destroy(par_handle_t ph)
{
    int s, n;

    assert(ph->magic == PAR_HANDLE_MAGIC);
    assert(ph->res == ISP_EEOF);

    if ((n = util_waitpid(ph->pid, &s, 0)) != ph->pid)
        isp_errx(1, "util_waitpid: %m");
    if (WIFEXITED(s)) {
        if (WEXITSTATUS(s) != 0)
            isp_errx(1, "util_waitpid: coproc exited with %d", WEXITSTATUS(s));
    } else if (WIFSIGNALED(s))
        isp_errx(1, "util_waitpid: coproc died on signal %d", WTERMSIG(s));
    else if (WIFSTOPPED(s))
        isp_errx(1, "util_waitpid: coproc stopped on signal %d", WSTOPSIG(s));

    isp_handle_destroy(ph->h);

    ph->magic = 0;
    free(ph);
}

/* Block waiting for activity on stdin, stdout, or any spawned
 * coprocesses, then perform the I/O.  runpipe() will then check the
 * buffers for new data to process.
 */
static void
_wait_for_pio(isp_handle_t h, List phl)
{
    pfd_t pfd;
    ListIterator itr;
    par_handle_t ph;
    int res;

    assert(phl != NULL);

    if (!(itr = list_iterator_create(phl)))     /* init */
        isp_errx(1, "_wait_for_pio: out of memory");
    if ((res = util_pfd_create(&pfd)) != ISP_ESUCCESS)
        isp_errx(1, "util_pfd_create: %s", isp_errstr(res));

    util_pfd_zero(pfd);                         /* prepare pfd for poll call */
    while ((ph = list_next(itr)) != NULL)
        isp_handle_prepoll(ph->h, pfd);
    if (h)
        isp_handle_prepoll(h, pfd);
                                                /* wait 'till ready */
    if ((res = util_poll(pfd, NULL)) != ISP_ESUCCESS)   
        isp_errx(1, "util_poll: %s", isp_errstr(res));

    list_iterator_reset(itr);                   /* perform the io */
    while ((ph = list_next(itr)) != NULL)
        isp_handle_postpoll(ph->h, pfd);
    if (h)
        isp_handle_postpoll(h, pfd);

    util_pfd_destroy(pfd);                      /* fini */
    list_iterator_destroy(itr);
}

static void
par_handle_io(isp_handle_t h, par_handle_t ph)
{
    isp_init_t i;
    isp_unit_t u;
    int res;

    /* read and discard coproc init element (i) */
    if (ph->state == PROC_STARTING) {
        if ((ph->res = isp_init_read(ph->h, &i)) == ISP_ESUCCESS) {
            if ((res = isp_init_destroy(i)) != ISP_ESUCCESS)
                isp_errx(1, "isp_init_destroy: %s", isp_errstr(res));
            ph->state = PROC_RUNNING;
        } else if (ph->res != ISP_EWOULDBLOCK)
            isp_errx(1, "isp_init_read (coproc %lu): %s", 
                    ph->pid, isp_errstr(ph->res));
    }

    /* read units from coproc and write them to stdout */
    if (ph->state == PROC_RUNNING) {
        while ((ph->res = isp_unit_read(ph->h, &u)) == ISP_ESUCCESS) {
            if ((res = isp_unit_write(h, u)) != ISP_ESUCCESS)
                isp_errx(1, "isp_unit_write (stdout): %s", isp_errstr(res));
            /* write backlog unlimited so we should not see ISP_EWOULDBLOCK */
        }
        if (ph->res == ISP_EEOF) {
            ph->state = PROC_COMPLETE;
        } else if (ph->res != ISP_EWOULDBLOCK)
            isp_errx(1, "isp_unit_read (coproc %lu): %s", 
                    ph->pid, isp_errstr(ph->res));
    }
}

static void 
runpipe(isp_handle_t h, runcmd_t how, List phl, char **cmdargv, isp_init_t i, 
        unsigned long fanout)
{
    par_handle_t ph;
    isp_unit_t u;
    ListIterator itr;
    int inres = ISP_ESUCCESS;
    int res;

    while (inres != ISP_EEOF || !list_is_empty(phl)) {
        /* Manage stdin - start one coproc per unit.
         */
        if (inres != ISP_EEOF) {
            while ((!fanout || list_count(phl) < fanout) 
                    && (inres = isp_unit_read(h, &u)) == ISP_ESUCCESS)  {
                ph = par_handle_create(how, cmdargv, i, u);
                if (list_append(phl, ph) == NULL) {
                    res = ISP_ENOMEM;
                    isp_errx(1, "list_append: %s", isp_errstr(res));
                }
            }
            if (inres != ISP_EWOULDBLOCK && inres != ISP_EEOF && inres != ISP_ESUCCESS)
                isp_errx(1, "isp_unit_read (stdin): %s", isp_errstr(inres));
        }

        /* Manage coprocesses.
         */
        itr = list_iterator_create(phl);
        while ((ph = list_next(itr)) != NULL) {
            assert(ph->magic == PAR_HANDLE_MAGIC);
            par_handle_io(h, ph);
            if (ph->state == PROC_COMPLETE)
                list_delete(itr); /* calls par_handle_destroy() */
        }
        list_iterator_destroy(itr);

        if (fanout && inres == ISP_ESUCCESS) {
            if (list_count(phl) == fanout)
                _wait_for_pio(NULL, phl);
        } else if (inres != ISP_EEOF || !list_is_empty(phl))
            _wait_for_pio(h, phl);
    }   

    assert(list_is_empty(phl));
    assert(inres == ISP_EEOF);
}

/* This is the initial handshake with the pipeline.
 */
static void
init_handshake(isp_handle_t h, isp_init_t *ip, int flags, char **cmdargv, List phl)
{
    int fid, res, n, s, ifd, ofd;
    isp_init_t i, i2;
    isp_filter_t ftmp;
    isp_handle_t ch;
    pid_t pid;

    assert(ip != NULL);
    assert((flags & ISP_SOURCE) && (flags & ISP_SINK)); /* FIXME */

    /* Read init element from stdin (i).
     */
    if (flags & ISP_SINK) {
        if ((res = isp_init_read(h, &i)) != ISP_ESUCCESS)
            isp_errx(1, "isp_init_read (pipeline): %s", isp_errstr(res));
        if ((res = isp_init_peek(i, &ftmp)) != ISP_ESUCCESS)
            isp_errx(1, "isp_init_peek (pipeline): %s", isp_errstr(res));
        if ((res = isp_filter_fid_get(ftmp, &fid)) != ISP_ESUCCESS)
            isp_errx(1, "isp_filter_fid_get (pipeline): %s", isp_errstr(res));
        isp_filterid_set(fid + 1);
    } else {
        isp_filterid_set(0);
    }

    /* Start coprocess and give it an isp handle (ch).
     */
    if ((res = util_runcoproc(cmdargv, &pid, &ifd, &ofd, NULL)) != ISP_ESUCCESS)
        isp_errx(1, "util_runcoproc: %s", isp_errstr(res));
    if ((res = isp_handle_create(&ch, ISP_SOURCE | ISP_SINK, 
                    IBACKLOG, OBACKLOG, ofd, ifd)) != ISP_ESUCCESS)
        isp_errx(1, "isp_handle_create: %s", isp_errstr(res));

    /* Write init element (i) to coprocess (ch) and write a NULL unit (EOF).
     */
    if ((res = isp_init_write(ch, i)) != ISP_ESUCCESS)
        isp_errx(1, "isp_init_write: %s", isp_errstr(res));
    if ((res = isp_unit_write(ch, NULL)) != ISP_ESUCCESS)
        isp_errx(1, "isp_unit_write: %s", isp_errstr(res));

    /* Read init element (i2) from coprocess.
     */
    if ((res = isp_init_read(ch, &i2)) != ISP_ESUCCESS)
        isp_errx(1, "isp_init_read (coproc): %s", isp_errstr(res));

    /* Wait for coprocess to terminate (triggered by writing NULL above)
     */
    if ((n = util_waitpid(pid, &s, 0)) != pid)
        isp_errx(1, "util_waitpid: %m");
    if (WIFEXITED(s)) {
        if (WEXITSTATUS(s) != 0)
            isp_errx(1, "util_waitpid: coproc exited with %d", WEXITSTATUS(s));
    } else if (WIFSIGNALED(s))
        isp_errx(1, "util_waitpid: coproc died on signal %d", WTERMSIG(s));
    else if (WIFSTOPPED(s))
        isp_errx(1, "util_waitpid: coproc stopped on signal %d", WSTOPSIG(s));
    isp_handle_destroy(ch);

    /* Write processed init element (i2) to stdout and destroy it.
     */
    if (flags & ISP_SOURCE) {
        if ((res = isp_init_write(h, i2)) != ISP_ESUCCESS)
            isp_errx(1, "isp_init_write: %s", isp_errstr(res));
    }
    if ((res = isp_init_destroy(i2)) != ISP_ESUCCESS)
        isp_errx(1, "isp_init_destroy: %s", isp_errstr(res));

    /* Store a copy of the unprocessed init element for future use.
     */
    if (ip)
        *ip = i;
}

int 
main(int argc, char *argv[])
{
    int res;
    isp_handle_t h;
    isp_init_t i;
    List phl;
    char **cmdargv;
    int c, longindex;
    runcmd_t how = RUNCMD_DIRECT;
    int flags = ISP_PROXY | ISP_SOURCE | ISP_SINK;
    unsigned long fanout = DEFAULT_FANOUT;
    char *progname;

    progname = basename(argv[0]);
    while ((c = getopt_long(argc, argv, OPT_STRING, longopts, 
            &longindex)) != -1) { 
        switch (c) { 
            case 's':   /* --srun */
                how = RUNCMD_SRUN;
                break;
            case 'd':   /* --direct */
                how = RUNCMD_DIRECT;
                break;
            case 'f':   /* --fanout */
                fanout = strtoul(optarg, NULL, 10);
                if (fanout == ULONG_MAX && errno == ERANGE) {
                    fprintf(stderr, "%s: fanout value overflow", progname);
                    exit(1);
                }
                break;
            default:
                usage();
                break;
        }
    }
    if (argc == optind)
        usage();

    /* Initialize.
     */
    if ((res = isp_init(&h, flags, argc, argv, NULL, 1)) != ISP_ESUCCESS)
        isp_errx(1, "isp_init: %s", isp_errstr(res));

    if ((res = isp_handle_backlog_set(h, IBACKLOG, OBACKLOG)) != ISP_ESUCCESS)
        isp_errx(1, "isp_handle_backlog_set: %s", isp_errstr(res));

    argc -= optind;
    argv += optind;
    if ((res = util_argvdupc(argc, argv, &cmdargv)) != ISP_ESUCCESS)
        isp_errx(1, "util_argvdupc: %s", isp_errstr(res));

    if (!(phl = list_create((ListDelF)par_handle_destroy)))
        isp_errx(1, "list_create: out of memory");

    /* Perform the initial handshake with the pipeline.
     */
    init_handshake(h, &i, flags, cmdargv, phl);

    /* Process the pipeline with coprocesses.
     * Put stdin/stdout handle in non-blocking mode during this phase.
     */
    if ((res = isp_handle_flags_set(h, flags | ISP_NONBLOCK)) != ISP_ESUCCESS)
        isp_errx(1, "isp_handle_flags_set", isp_errstr(res));

    runpipe(h, how, phl, cmdargv, i, fanout);

    if ((res = isp_handle_flags_set(h, flags)) != ISP_ESUCCESS)
        isp_errx(1, "isp_handle_flags_set", isp_errstr(res));

    /* Clean up.
     */
    list_destroy(phl);
    if ((res = isp_init_destroy(i)) != ISP_ESUCCESS)
        isp_errx(1, "isp_init_destroy: %s", isp_errstr(res));
    if ((res = isp_handle_write(h, NULL)) != ISP_ESUCCESS)
        isp_errx(1, "isp_handle_write: %s", isp_errstr(res));
    if ((res = isp_fini(h)) != ISP_ESUCCESS)
        isp_errx(1, "isp_fini: %s", isp_errstr(res));
    free(cmdargv);

    exit(0);
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
