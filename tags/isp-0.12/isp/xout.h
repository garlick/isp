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

#ifndef _XOUT_H
#define _XOUT_H

/* xout provides a non-blocking XML output handle abstraction.
 * The XML is presumed to be of the form:
 *   XML header
 *   <document>
 *   element
 *   element
 *   ...
 *   element
 *   </document>
 * The elements can contain sub-elements, attributes, and text (basically
 * free-form XML), but the document is presumed to only contain a stream 
 * of elements which are written one at a time.  The XML header and
 * <document> tag are prepended to the first element written.  The 
 * </document> tag is written when a NULL element is written.
 *
 * Usage:
 *   xout_handle_create(fd, backlog, &handle)
 *   xout_write_el(handle,element)
 *   xout_write_el(handle,element)
 *   ...
 *   xout_write_el(handle,element)
 *   xout_write_el(handle,NULL)
 *   xout_handle_destroy(handle)
 * xout_write_el can return ISP_EWOULDBLOCK.  
 * In that case, a polling loop should be called:
 *   xout_prepoll(handle, pfd)
 *   util_poll(pfd)
 *   xout_postpoll(handle, pfd)
 * and the write should be retried on return.  I/O errors encountered during
 * prepoll/postpoll are deferred until the next xout_write_el() call, or to
 * xout_handle_destroy().
 *
 * Pipeline flow control is achieved by failing upstream xout writes
 * with ISP_EWOULDBLOCK when their maxbacklog is reached as a result of 
 * downstream xin buffers reaching their maxbacklog.  Filters with larger 
 * xout maxbacklogs will be able to keep doing useful work longer in the 
 * event of a downstream stall, however this is at the expense of a larger 
 * memory footprint.
 */

struct xout_handle_struct;
typedef struct xout_handle_struct *xout_handle_t;

#define XOUT_BACKLOG_UNLIMITED  (0)

/* Create an xout handle.  fd is placed in non-blocking mode.
 * backlog specifies the number of elements that can be buffered internally.
 * The backlog can be set to XOUT_BACKLOG_UNLIMITED, however if the backlog 
 * overruns the malloc pool, a fatal ISP_ENOMEM error could be returned.
 */
int     xout_handle_create(int fd, int maxbacklog, xout_handle_t *hp);

int     xout_backlog_set(xout_handle_t h, int backlog);

/* Destroy an xout handle. fd is closed and any backlogged I/O is flushed
 * synchronously.  If this call should not block, ensure that
 * xout_get_backlog() returns zero first.
 */
int     xout_handle_destroy(xout_handle_t h);

/* Write element (and any sub-elements) to output buffer. 
 * A NULL element causes document closing XML to be written (further writes 
 * will fail).  The XML header and document opening XML are prepended to 
 * the first element written.  The element is copied (the caller still 
 * owns the orig).  Return values include:
 * ISP_ESUCCESS - element successfully written or buffered internally.
 * ISP_EWOULDBLOCK - I/O must occur before element can be accepted
 */
int     xout_write_el(xout_handle_t h, xml_el_t el);

/* Return the number of elements in the backlog.
 * This function always succedes.
 */
int     xout_get_backlog(xout_handle_t h);

/* Select/Poll maangement functions.
 * Any errors are deferred to xout_write_*() or xout_handle_destroy().
 */
void    xout_prepoll(xout_handle_t h, pfd_t pfd);
void    xout_postpoll(xout_handle_t h, pfd_t pfd);

#endif /* _XOUT_H */

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
