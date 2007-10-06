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
#include <stdarg.h>
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <expat.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/poll.h>

#include "list.h"
#include "util.h"
#include "isp.h"
#include "xml.h"
#include "xout.h"
#include "macros.h"

#define XML_OPEN  "<?xml version=\"1.0\" standalone=\"yes\"?>\n<document>\n"
#define XML_CLOSE "</document>\n"

#define XOUT_BUFFER_MAGIC   0x12344322
struct buffer_struct {
    int magic;
    char *buf;      /* string (not null terminated) */
    int size;       /* size of string */
    int written;    /* amount of data written to fd */
};
typedef struct buffer_struct *buffer_t;

typedef enum { VIRGIN, DOCOPEN, DOCCLOSED } state_t;

#define XOUT_HANDLE_MAGIC   0x12344321
struct xout_handle_struct {
    int magic;
    int fd;         /* file descriptor - we own it after xout_handle_creat */
    int errnum;     /* deferred i/o error */
    List backlog;   /* queue of buffer_t's pending I/O */
    int maxbacklog; /* maximum queue depth (element count), 0=unlimited */
    state_t state;  /* handle state */
};

static int _set_nonblock(int fd, int nonblockflag);
static int _flush(xout_handle_t h, int noewouldblock);
static int _wait_for_io(xout_handle_t h);

static void _buffer_destroy(buffer_t b)
{
    assert(b->magic = XOUT_BUFFER_MAGIC);
    if (b->size > 0)
        free(b->buf);
    b->magic = 0;
    free(b);
}

static buffer_t
_buffer_create()
{
    buffer_t b = NULL;
    
    if ((b = (buffer_t)calloc(1, sizeof(struct buffer_struct))) != NULL)
        b->magic = XOUT_BUFFER_MAGIC;

    return b;
}

PRIVATE int
xout_get_backlog(xout_handle_t h)
{
    assert(h->magic == XOUT_HANDLE_MAGIC);

    return list_count(h->backlog);
}

PRIVATE int
xout_write_el(xout_handle_t h, xml_el_t el)
{
    int res = ISP_ESUCCESS;
    buffer_t b = NULL;
    char *buf; 
    int size;

    assert(h->magic == XOUT_HANDLE_MAGIC);

    if (h->errnum != ISP_ESUCCESS)
        return h->errnum;
    if (h->maxbacklog > XOUT_BACKLOG_UNLIMITED 
            && xout_get_backlog(h) == h->maxbacklog) {
        res = ISP_EWOULDBLOCK;
        goto error;
    }
    if ((b = _buffer_create()) == NULL) {
        res = ISP_ENOMEM;
        goto error;
    }
    switch (h->state) {
        case VIRGIN:
            /* prepend XML open to element string */
            if ((b->buf = malloc(strlen(XML_OPEN))) == NULL) {
                res = ISP_ENOMEM;
                goto error;
            }
            b->size = strlen(XML_OPEN);
            memcpy(b->buf, XML_OPEN, b->size);
            if ((res = xml_el_to_str(el, &buf, &size)) != ISP_ESUCCESS)
                goto error;
            if ((b->buf = realloc(b->buf, b->size + size)) == NULL) {
                res = ISP_ENOMEM;
                goto error;
            }
            memcpy(b->buf + b->size, buf, size);
            b->size += size;
            h->state = DOCOPEN;
            break;
        case DOCOPEN:
            if (el == NULL) {   /* NULL signifies end of file */
                if ((b->buf = malloc(strlen(XML_CLOSE))) == NULL) {
                    res = ISP_ENOMEM;
                    goto error;
                }
                b->size = strlen(XML_CLOSE);
                memcpy(b->buf, XML_CLOSE, b->size);
                h->state = DOCCLOSED;
                break;
            } else {            /* just write the element string */
                if ((res = xml_el_to_str(el, &b->buf, &b->size)) != ISP_ESUCCESS)
                    goto error;
            } 
            break;
        case DOCCLOSED:
            res = ISP_EBADF;
            goto error;
            break;
    }
    list_enqueue(h->backlog, b);
    if ((res = _flush(h, 1)) != ISP_ESUCCESS)
        goto error;

    return res;
error:
    if (b)
        _buffer_destroy(b);
    return res;
}

PRIVATE int
xout_handle_create(int fd, int maxbacklog, xout_handle_t *hp)
{
    xout_handle_t h = NULL;
    int res = ISP_ESUCCESS;

    if (maxbacklog < 0) {
        res = ISP_EINVAL;
        goto error;
    }
    if (!(h = (xout_handle_t)calloc(1, sizeof(struct xout_handle_struct)))) {
        res = ISP_ENOMEM;
        goto error;
    }
    h->magic = XOUT_HANDLE_MAGIC;
    h->fd = fd;
    h->errnum = ISP_ESUCCESS;
    h->maxbacklog = maxbacklog;
    h->backlog = list_create((ListDelF)_buffer_destroy);
    h->state = VIRGIN;
    if (!h->backlog) {
        res = ISP_ENOMEM;
        goto error;
    }
    res = _set_nonblock(h->fd, 1);

    if (hp)
        *hp = h;
    assert(res == ISP_ESUCCESS);
    return res;
error:
    if (h)
        xout_handle_destroy(h);
    return res;
}

PRIVATE int
xout_backlog_set(xout_handle_t h, int backlog)
{
    assert(h->magic == XOUT_HANDLE_MAGIC);
    h->maxbacklog = backlog;

    return ISP_ESUCCESS;
}


PRIVATE int
xout_handle_destroy(xout_handle_t h)
{
    int res = ISP_ESUCCESS;

    assert(h->magic == XOUT_HANDLE_MAGIC);

    if (h->errnum != ISP_ESUCCESS) {
        res = h->errnum;
        goto done;
    }
    if (h->state == DOCOPEN) {
        res = ISP_ENOTCLOSED;
        goto done;
    }
    if (h->state == DOCCLOSED) {
        while ((res = _flush(h, 0)) == ISP_EWOULDBLOCK) {
            if ((res = _wait_for_io(h)) != ISP_ESUCCESS)
                break;
        }
        if (res != ISP_ESUCCESS)
            goto done;
    }
    if (close(h->fd) < 0 && res == ISP_ESUCCESS) {
        res = ISP_EWRITE;
        goto done;
    }

done:
    if (h->backlog)
        list_destroy(h->backlog);
    h->magic = 0;
    free(h);
    return res;
}

PRIVATE void
xout_prepoll(xout_handle_t h, pfd_t pfd)
{
    assert(h->magic == XOUT_HANDLE_MAGIC);

    if (h->errnum == ISP_ESUCCESS)
        if (list_count(h->backlog) > 0)
            h->errnum = util_pfd_set(pfd, h->fd, POLLOUT); /* defer error */
}

PRIVATE void
xout_postpoll(xout_handle_t h, pfd_t pfd)
{
    short flags; 

    assert(h->magic == XOUT_HANDLE_MAGIC);

    if (h->errnum == ISP_ESUCCESS) {
        flags = util_pfd_revents(pfd, h->fd); 
        if ((flags & POLLHUP) || (flags & POLLERR) || (flags & POLLNVAL))
            h->errnum = ISP_EPOLL;      /* defer error */
        else if ((flags & POLLOUT))
            h->errnum = _flush(h, 1);
    }
}

/* Flush buffer to file descriptor.  Can return ISP_EWOULDBLOCK.
 */
static int
_flush(xout_handle_t h, int noewouldblock)
{
    int res = ISP_ESUCCESS;
    buffer_t b;
    int n;
    ListIterator itr;

    if (h->errnum != ISP_ESUCCESS)
        return h->errnum;

    if ((itr = list_iterator_create(h->backlog)) == NULL) {
        res = ISP_ENOMEM;
        goto done;
    }
    while ((b = list_next(itr))) {
        while (b->written < b->size) {
            n = write(h->fd, b->buf + b->written, b->size - b->written);
            if (n < 0) {
                res = (errno == EWOULDBLOCK) ? ISP_EWOULDBLOCK : ISP_EWRITE;
                goto done;
            }
            b->written += n;
        }
        n = list_delete(itr);
        assert(n == 1);
    }
    list_iterator_destroy(itr);
done:
    if (res == ISP_EWOULDBLOCK && noewouldblock)
        res = ISP_ESUCCESS;
    return res;
}

static int
_wait_for_io(xout_handle_t h)
{
    pfd_t pfd;
    int res;

    if (h->errnum != ISP_ESUCCESS)
        return h->errnum;

    if ((res = util_pfd_create(&pfd)) == ISP_ESUCCESS) {
        util_pfd_zero(pfd);
        xout_prepoll(h, pfd);
        if ((res = util_poll(pfd, NULL)) == ISP_ESUCCESS)
            xout_postpoll(h, pfd);
        util_pfd_destroy(pfd);
    }
    return res;
}

static int
_set_nonblock(int fd, int nonblockflag)
{
    int flags;
    int res = ISP_ESUCCESS;

    flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        res = ISP_EFCNTL;
        goto done;
    }
    if (nonblockflag)
        flags |= O_NONBLOCK;
    else
        flags &= ~O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) < 0) {
        res = ISP_EFCNTL;
        goto done;
    }
done:
    return res;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
