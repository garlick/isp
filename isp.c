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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#define _GNU_SOURCE
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <sys/times.h>
#include <string.h>
#include <sys/param.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/time.h>
#include <assert.h>
#include <libgen.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <errno.h>
#include <sys/param.h>

#include "util.h"
#include "list.h"
#include "xml.h"
#include "xin.h"
#include "xout.h"
#include "isp.h"
#include "isp_private.h"
#include "macros.h"

/* XXX not seeing this prototype in stdio.h for some reason... */
int vsscanf(const char *str, const char *fmt, va_list ap); 

#define IBACKLOG   1   /* no. elements to buffer for input (1=min) */
#define OBACKLOG   1   /* no. elements to buffer for output (1=min) */

static char        progbase[MAXPATHLEN+1]     = "<unknown>";
static char        progname[MAXPATHLEN+1]     = "<unknown>";
static char        hostname[MAXHOSTNAMELEN+1] = "localhost";
static int         filterid = NO_FID;
static int         md5check = 0;
static int         dbgfail = 0;

static isp_init_t  init_element = NULL;

PRIVATE int
isp_md5check_get(void)
{
    return md5check;
}

PRIVATE char *
isp_hostname_get(void)
{
    return hostname;
}

PRIVATE char *
isp_progname_get(void)
{
    return progname;
}

PRIVATE int 
isp_filterid_get(void)
{
    return filterid;
}

PRIVATE void
isp_filterid_set(int fid)
{
    filterid = fid;
    snprintf(progname, sizeof(progname), "%s[%d]", progbase, fid);
}

PRIVATE int
isp_dbgfail_get(void)
{
    return dbgfail;
}

PRIVATE void
isp_init_set(isp_init_t i)
{
    init_element = i;
}

PRIVATE int
isp_init_get(isp_init_t *ip)
{
    if (!ip)
        return ISP_EINVAL;
    if (!init_element)
        return ISP_ENOINIT;
    *ip = init_element;
    return ISP_ESUCCESS;
}

/* helper for isp_init */
static void
_getenv_flag(char *flagname, int *flagval)
{
    char *tmpstr = getenv(flagname);

    if (!tmpstr)
        *flagval = 0;
    else
        *flagval = (int)strtol(tmpstr, NULL, 10);
}

PUBLIC int
isp_init(isp_handle_t *hp, int flags, int argc, char *argv[], 
        struct isp_stab_struct stab[], int sf)
{
    int res = ISP_ESUCCESS;
    isp_handle_t h = NULL;

    (void)gethostname(hostname, sizeof(hostname) - 1);
    hostname[sizeof(hostname) - 1] = '\0';

    strncpy(progbase, basename(argv[0]), sizeof(progbase) - 1);
    progbase[sizeof(progbase) - 1] = '\0';
    strcpy(progname, progbase);

    _getenv_flag("ISP_DBGFAIL", &dbgfail);
    _getenv_flag("ISP_MD5CHECK", &md5check);

    if (hp == NULL)
        return ISP_EINVAL;
#if !(HAVE_OPENSSL)
    if (md5check) {
        isp_dbgfail("ISP_MD5CHECK env var set but no support for MD5 in ISP");
        return ISP_EINVAL;
    }
#endif
    if ((flags & ISP_NONBLOCK) && !(flags & ISP_PROXY)) {
        isp_dbgfail("for now ISP_NONBLOCK cannot be set without ISP_PROXY");
        return ISP_EINVAL;
    }
    if (sf < 1) {
        isp_dbgfail("split factor must be >= 1");
        return ISP_EINVAL;
    }

    if ((res = isp_handle_create(&h, flags, IBACKLOG, OBACKLOG, 
                            STDIN_FILENO, STDOUT_FILENO)) != ISP_ESUCCESS)
        return res;

    if (!(flags & ISP_PROXY)) {
        res = isp_init_handshake(h, stab, argc, argv, sf);
        if (res != ISP_ESUCCESS) {
            isp_dbgfail("init handshake failed");
            (void)isp_fini(h);
            return res;
        }
    }

    *hp = h;
    return ISP_ESUCCESS;
}

PUBLIC int
isp_fini(isp_handle_t h)
{
    int res = ISP_ESUCCESS;

    if (h)
        res = isp_handle_destroy(h);

    return res;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
