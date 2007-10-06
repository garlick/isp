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
#include <stdio.h>
#include <string.h>
#include <expat.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <sys/poll.h>
#include <fcntl.h>
#include <limits.h>

#include "list.h"
#include "util.h"
#include "isp.h"
#include "xml.h"
#include "xin.h"
#include "macros.h"

#define XIN_HANDLE_MAGIC   0x22344322
struct xin_handle_struct {
    int magic;
    int fd;
    int errnum;         /* deferred error to be returned on read */
    int maxbacklog;     /* max elements to buffer (0 = unlimited) */
    xml_el_t document;  /* XML 'document' we build up as we go */
    xml_el_t current;   /* current element we are building */
    XML_Parser parser;
    int end;            /* 1 when complete document has been parsed */
    int count;          /* count of complete document level els in backlog */
};

static int _set_nonblock(int fd, int nonblockflag);

/* XXX: more than h->maxbacklog elements will be buffered if they
 * are ready and fit in one XML_BUFSIZE.
 */
#ifndef PIPE_BUF
#define XML_BUFSIZE 4096
#else
#define XML_BUFSIZE PIPE_BUF
#endif

/* Expat callback for element instantiation (with attributes)
 */
static void 
_parse_start(void *data, const char *name, const char **attr)
{
    xin_handle_t h = (xin_handle_t)data;
    const char **pp;
    xml_el_t new;

    assert(h->magic == XIN_HANDLE_MAGIC);

    h->errnum = xml_el_create(name, &new);
    if (h->errnum != ISP_ESUCCESS)
        return;
    for (pp = attr; *pp; pp += 2) {
        xml_attr_t nattr;

        h->errnum = xml_attr_create((char *)pp[0], &nattr, "%s", (char *)pp[1]);
        if (h->errnum != ISP_ESUCCESS)
            return;
        h->errnum = xml_attr_append(new, nattr);
        if (h->errnum != ISP_ESUCCESS)
            return;
    }

    /* We are starting a new element.  The element's parent will be the 
     * "current" element (NULL if we are opening <document>).
     */
    xml_el_parent_set(new, h->current);

    /* If we are opening <document> then set h->document to point to it.
     */
    if (h->current == NULL) {
        assert(h->document == NULL);
        h->document = new;
    /* If we are opening a level below <document>, append it to the current
     * element.
     */
    } else {
        h->errnum = xml_el_append(h->current, new);
        if (h->errnum != ISP_ESUCCESS)
            return;
    }
    /* The current element is now the new one until it is closed or a deeper
     * level is opened.
    */
    h->current = new;
}

/* Expat callback to close the current element.
 */
static void 
_parse_end(void *data, const char *name)
{
    xin_handle_t h = (xin_handle_t)data;

    assert(h->magic == XIN_HANDLE_MAGIC);
    assert(h->current != NULL);

    /* Current element is closed, set current to the element containing
     * the one we just closed.
     */
    h->current = xml_el_parent_get(h->current);

    /* If we just closed an element and the element's parent is NULL,
     * that means we just closed <document> and we are all done.
     */
    if (h->current == NULL)
        h->end = 1;
    /* If we just closed an element and the element's parent is <document>,
     * then we have a complete element that will satisfy a read request, 
     * so increment h->count.
     */
    else if (h->current == h->document)
        h->count++;
}

PRIVATE int
xin_read_el(xin_handle_t h, xml_el_t *elp)
{
    xml_el_t el = NULL;
    int res = ISP_ESUCCESS;

    assert(h->magic == XIN_HANDLE_MAGIC);

    if (h->count > 0) {
        assert(h->document != NULL);
        el = xml_el_pop(h->document);
        assert(el != NULL);
        h->count--;
    } else {
        if (h->errnum != ISP_ESUCCESS)
            res = h->errnum;
        else if (h->end)
            res = ISP_EEOF;
        else
            res = ISP_EWOULDBLOCK;
    }

    if (res == ISP_ESUCCESS) {
        if (elp)
            *elp = el;
        else 
            xml_el_destroy(el);
    } 
    return res;
}

PRIVATE int
xin_get_backlog(xin_handle_t h)
{
    assert(h->magic == XIN_HANDLE_MAGIC);

    return h->count;
}

PRIVATE int
xin_handle_create(int fd, int maxbacklog, xin_handle_t *hp)
{
    xin_handle_t h = (xin_handle_t)calloc(1, sizeof(struct xin_handle_struct));

    if (h == NULL)
        return ISP_ENOMEM;

    h->magic = XIN_HANDLE_MAGIC;
    h->fd = fd;
    h->errnum = ISP_ESUCCESS;
    h->maxbacklog = maxbacklog;
    h->parser = XML_ParserCreate(NULL);
    if (h->parser == NULL) {
        free(h);
        return ISP_ENOMEM;
    }
    XML_SetElementHandler(h->parser, _parse_start, _parse_end);
    XML_SetUserData(h->parser, h);
    if (_set_nonblock(h->fd, 1) != ISP_ESUCCESS) {
        XML_ParserFree(h->parser);
        free(h);
        return ISP_EFCNTL;
    }

    assert(hp != NULL);
    *hp = h;

    return ISP_ESUCCESS;
}

PRIVATE int
xin_backlog_set(xin_handle_t h, int backlog)
{
    assert(h->magic == XIN_HANDLE_MAGIC);
    h->maxbacklog = backlog;

    return ISP_ESUCCESS;
}

PRIVATE int
xin_handle_destroy(xin_handle_t h)
{
    int res = ISP_ESUCCESS;

    assert(h->magic == XIN_HANDLE_MAGIC);

    if (close(h->fd) < 0)
        res = ISP_EREAD;
    XML_ParserFree(h->parser);
    if (h->document)
        xml_el_destroy(h->document);
    h->magic = 0;
    free(h);

    return res;
}

PRIVATE int
xin_preparse(xin_handle_t h)
{
    int res = ISP_ESUCCESS;
    pfd_t pfd;
    int savemaxbacklog;

    assert(h->magic == XIN_HANDLE_MAGIC);

    if ((res = util_pfd_create(&pfd)) == ISP_ESUCCESS) {
        savemaxbacklog = h->maxbacklog;
        h->maxbacklog = 0; /* unlimited for this function call */
        while (h->errnum == ISP_ESUCCESS) {
            util_pfd_zero(pfd);
            xin_prepoll(h, pfd);
            if ((res = util_poll(pfd, NULL)) == ISP_ESUCCESS)
                break;
            xin_postpoll(h, pfd);
        }
        h->maxbacklog = savemaxbacklog;
        util_pfd_destroy(pfd);
    }

    return res;
}

PRIVATE void
xin_prepoll(xin_handle_t h, pfd_t pfd)
{
    assert(h->magic == XIN_HANDLE_MAGIC);

    if (h->errnum == ISP_ESUCCESS && 
            (h->maxbacklog == 0 || h->count < h->maxbacklog))
        h->errnum = util_pfd_set(pfd, h->fd, POLLIN);
}

PRIVATE void
xin_postpoll(xin_handle_t h, pfd_t pfd)
{
    short flags; 
    int r;

    assert(h->magic == XIN_HANDLE_MAGIC);

    if (h->errnum == ISP_ESUCCESS && 
            (h->maxbacklog == 0 || h->count < h->maxbacklog)) {
        flags = util_pfd_revents(pfd, h->fd); 
        if ((flags & POLLERR) || (flags & POLLNVAL))
            h->errnum = ISP_EPOLL;
        else if ((flags & POLLIN) || (flags & POLLHUP)) {
            do {
                void *buf = XML_GetBuffer(h->parser, XML_BUFSIZE);
                
                r = util_read(h->fd, buf, XML_BUFSIZE);
                if (r < 0 && errno == EWOULDBLOCK)
                    break;
                if (r < 0) {
                    h->errnum = ISP_EREAD;
                    break;
                }
                if (r == 0)
                    h->errnum = ISP_EEOF;
                if (!XML_ParseBuffer(h->parser, r, (h->errnum == ISP_EEOF))) {
                    h->errnum = ISP_EPARSE;
                    break;
                }
            } while (r > 0 && (h->maxbacklog == 0 || h->count < h->maxbacklog));
        }
    }
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
