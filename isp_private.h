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

/* This interface is only exposed to the filters supplied with ISP.
 */

#ifndef _ISP_PRIVATE_H
#define _ISP_PRIVATE_H

typedef struct xml_el_struct *isp_init_t;
typedef struct xml_el_struct *isp_filter_t;

#define NO_FID	(-1)
#define BACKLOG_UNLIMITED (0)

/* handle.c */
int   isp_handle_create(isp_handle_t *hp, int flags, 
                        int ibacklog, int obacklog, int ifd, int ofd);
int   isp_handle_destroy(isp_handle_t h);
int   isp_handle_check(isp_handle_t h);
int   isp_handle_read(isp_handle_t h, struct xml_el_struct **ep);
int   isp_handle_write(isp_handle_t h, struct xml_el_struct *e);
void  isp_handle_prepoll(isp_handle_t h, pfd_t pfd);
void  isp_handle_postpoll(isp_handle_t h, pfd_t pfd);
int   isp_handle_flags_get(isp_handle_t h, int *fp);
int   isp_handle_flags_set(isp_handle_t h, int f);
int   isp_handle_backlog_set(isp_handle_t h, int ibacklog, int obacklog);

/* isp.c */
int   isp_filterid_get(void);
void  isp_filterid_set(int fid);
void  isp_init_set(isp_init_t i);
int   isp_init_get(isp_init_t *ip);
char *isp_progname_get(void);
int   isp_dbgfail_get(void);
int   isp_md5check_get(void);
char *isp_hostname_get(void);

/* error.c */
void isp_dbgfail(const char *fmt, ...);

/* unit.c */
int isp_result_get(isp_unit_t u, int fid, unsigned long *utime, 
         unsigned long *stime, unsigned long *rtime, int *result);

/* init.c */
int isp_init_handshake(isp_handle_t h, struct isp_stab_struct stab[], 
                       int argc, char *argv[], int sf);
int isp_init_destroy(isp_init_t i);
int isp_init_read(isp_handle_t h, isp_init_t *ip);
int isp_init_write(isp_handle_t h, isp_init_t i);
int isp_init_peek(isp_init_t i, isp_filter_t *fp);
int isp_init_find(isp_init_t i, int fid, isp_filter_t *fp);
int isp_filter_fid_get(isp_filter_t f, int *fidp);
int isp_filter_splitfactor_get(isp_filter_t f, int *sfp);
/* more filter accessors to be added */

#endif /* _ISP_PRIVATE_H */

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
