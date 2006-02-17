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

/* This is a wrapper around xin and xout that bundles two file
 * descriptors together (one for input, one for output), and 
 * adds non-blocking I/O and preparsing options.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#include "util.h"
#include "xml.h"
#include "xin.h"
#include "xout.h"
#include "isp.h"
#include "isp_private.h"
#include "macros.h"

#define ISP_HANDLE_MAGIC 0x55634433
struct isp_handle_struct {
    int magic;
    int flags;          /* ISP_* flags */
    xout_handle_t xout; /* XML output handle */
    xin_handle_t xin;   /* XML input handle */
};

static int
_handle_check(isp_handle_t h)
{
    return (!h || h->magic != ISP_HANDLE_MAGIC) ? 0 : 1;
}

PRIVATE void
isp_handle_prepoll(isp_handle_t h, pfd_t pfd)
{
    if (_handle_check(h)) {
        if ((h->flags & ISP_SINK) && h->xin != NULL)
            xin_prepoll(h->xin, pfd);
        if ((h->flags & ISP_SOURCE && h->xout != NULL))
            xout_prepoll(h->xout, pfd);
    }
}

PRIVATE void
isp_handle_postpoll(isp_handle_t h, pfd_t pfd)
{
    if (_handle_check(h)) {
        if ((h->flags & ISP_SINK) && h->xin != NULL)
            xin_postpoll(h->xin, pfd);
        if ((h->flags & ISP_SOURCE) && h->xout != NULL)
            xout_postpoll(h->xout, pfd);
    }
}

static int
_wait_for_io(isp_handle_t h)
{
    pfd_t pfd;
    int res;

    if ((res = util_pfd_create(&pfd)) == ISP_ESUCCESS) {
        util_pfd_zero(pfd);
        isp_handle_prepoll(h, pfd);
        if ((res = util_poll(pfd, NULL)) == ISP_ESUCCESS)
            isp_handle_postpoll(h, pfd);
        util_pfd_destroy(pfd);
    }

    return res;
}
	
PRIVATE int
isp_handle_destroy(isp_handle_t h)
{
    int res = ISP_ESUCCESS;

    if (!_handle_check(h))
        return ISP_EINVAL;

    if (h->xout)
        if ((res = xout_handle_destroy(h->xout)) != ISP_ESUCCESS)
            goto done;
    if (h->xin)
        if ((res = xin_handle_destroy(h->xin)) != ISP_ESUCCESS)
            goto done;
done:
    free(h);
    return res;
}

PRIVATE int
isp_handle_create(isp_handle_t *hp, int flags, int ibacklog, int obacklog,
                  int ifd, int ofd)
{
    int res = ISP_ESUCCESS;
    isp_handle_t h;

    if (!(h = (isp_handle_t)calloc(1, sizeof(struct isp_handle_struct))))
        return ISP_ENOMEM;

    h->magic = ISP_HANDLE_MAGIC;
    h->flags = flags;
    if (flags & ISP_SOURCE) {
        if ((res = xout_handle_create(ofd, obacklog, &h->xout)) != ISP_ESUCCESS)
            goto error;
    }
    if (flags & ISP_SINK) {
        if ((res = xin_handle_create(ifd, ibacklog, &h->xin)) != ISP_ESUCCESS)
            goto error;
        if (flags & ISP_PREPARSE) {
            if ((res = xin_preparse(h->xin)) != ISP_ESUCCESS)
                goto error;
        }
    }

    if (hp)
        *hp = h;
    return res;
error:
    if (h)
        isp_handle_destroy(h);
    return res;
}

PRIVATE int
isp_handle_read(isp_handle_t h, xml_el_t *ep)
{
    xml_el_t el = NULL;
    int res = ISP_ESUCCESS;

    if (!ep || !_handle_check(h) || !(h->flags & ISP_SINK) || !h->xin)
        return ISP_EINVAL;

    while ((res = xin_read_el(h->xin, &el)) == ISP_EWOULDBLOCK) {
        if (h->flags & ISP_NONBLOCK)
            break;
        if ((res = _wait_for_io(h)) != ISP_ESUCCESS)
            break;
    }

    if (res == ISP_ESUCCESS)
        *ep = el;
    else if (el)
        xml_el_destroy(el);

    return res;
}

/* note: e can be null */
PRIVATE int
isp_handle_write(isp_handle_t h, xml_el_t e)
{
    int res = ISP_ESUCCESS;

    if (!_handle_check(h) || !(h->flags & ISP_SOURCE) || !h->xout)
        return ISP_EINVAL;

    /* if blocking, retry if no room in buffer yet */
    while ((res = xout_write_el(h->xout, e)) == ISP_EWOULDBLOCK) {
        if ((h->flags & ISP_NONBLOCK))
            break;
        if ((res = _wait_for_io(h)) != ISP_ESUCCESS)
            break;
    }

    return res;
}

PRIVATE int
isp_handle_flags_get(isp_handle_t h, int *fp)
{
    if (!fp || !_handle_check(h))
        return ISP_EINVAL;
    *fp = h->flags;
    return ISP_ESUCCESS;
}

PRIVATE int
isp_handle_flags_set(isp_handle_t h, int f)
{
    if (!_handle_check(h))
        return ISP_EINVAL;
    h->flags = f;
    return ISP_ESUCCESS;
}

PRIVATE int
isp_handle_backlog_set(isp_handle_t h, int ibacklog, int obacklog)
{
    int res;

    if (!_handle_check(h))
        return ISP_EINVAL;
    if ((res = xin_backlog_set(h->xin, ibacklog)) != ISP_ESUCCESS)
        return res;
    if ((res = xout_backlog_set(h->xout, obacklog)) != ISP_ESUCCESS)
        return res;
    return ISP_ESUCCESS;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
