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

#ifndef _XIN_H
#define _XIN_H

/* xin provides a non-blocking XML input handle abstraction.
 * The XML is presumed to be of the form:
 * XML header
 * <document>
 * element
 * element
 * ...
 * element
 * </document>
 * The elements can contain sub-elements, attributes, and text (basically
 * free-form XML), but the document is presumed to only contain a stream 
 * of elements which are read one at a time.  The XML header and 
 * <document>, </document> XML tags are silently discarded (only the 
 * elements are returned).  Usage:
 *   xin_handle_create(fd, backlog, &handle)
 *   xin_read_el(handle, &element)
 *   xin_read_el(handle, &element)
 *   ...
 *   xin_read_el(handle, &element)
 * on ISP_EEOF:
 *   xout_handle_destroy(handle)
 * xin_read_el() can return ISP_EWOULDBLOCK when no elements are available.
 * In that case, a polling loop should be called that includes:
 *   xin_prepoll(handle, pfd)
 *   util_poll(pfd)
 *   xin_postpoll(handle, pfd)
 * and the xin_read_el() should be retried on return.  I/O errors encountered
 * during prepoll/postpoll are deferred until the next xin_read_el() call.
 */

struct xin_handle_struct;
typedef struct xin_handle_struct *xin_handle_t;

#define XIN_BACKLOG_UNLIMITED (0)

/* Create XML input handle.  Sets fd to nonblocking mode.
 * At most maxbacklog elements will be buffered (though it is possible for
 * more elements to be buffered if they fit in a read buffer (~4K, see xin.c)).
 * maxbacklog may be set to XIN_BACKLOG_UNLIMITED, but this may result
 * in a fatal ISP_ENOMEM if the backlog overruns the malloc pool.
 */
int     xin_handle_create(int fd, int maxbacklog, xin_handle_t *h);

int     xin_backlog_set(xin_handle_t h, int backlog);

/* Destroy XML input handle.  Closes fd.  Any unread data is discarded.
 */
int     xin_handle_destroy(xin_handle_t h);

/* Read next element from input.  Caller must free result.  Return value:
 * ISP_EWOULDBLOCK means no elements are currently available.
 * ISP_EEOF is returned on end of document.
 * ISP_EPARSE is returned on parse error or if fd EOF's before
 * </document> is parsed.
 * The XML header, document opening, and document closing are not returned.
 */
int     xin_read_el(xin_handle_t h, xml_el_t *el);

/* Return the number of elements in the backlog.
 * This function always succedes.
 */
int     xin_get_backlog(xin_handle_t h);

/* Preparse input, buffering entire document using blocking I/O.
 * The elements can then be read with xin_read_el() until ISP_EEOF as usual.
 */
int     xin_preparse(xin_handle_t h);

/* Select/poll management functions.
 * Errors are deferred to next xin_read_el() call.
 */
void    xin_prepoll(xin_handle_t h, pfd_t pfd);
void    xin_postpoll(xin_handle_t h, pfd_t pfd);

#endif /* _ISP_XIN_H */

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
