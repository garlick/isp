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

#ifndef _ISP_H
#define _HSP_H

#ifdef __cplusplus
  extern "C" {
#endif

/* flags for isp_init */
#define ISP_SOURCE              0x01 /* generate stdout */
#define ISP_SINK                0x02 /* parse stdin */
#define ISP_IGNERR              0x04 /* run map fn regardless of upstream err */
#define ISP_PREPARSE            0x08 /* preparse stdin XML */
#define ISP_NONBLOCK            0x40 /* internal use only */
#define ISP_PROXY               0x80 /* internal use only */

/* flags for isp_file_source(), isp_file_access() */
#define ISP_RDWR                1
#define ISP_RDONLY              2

/* Incremented when the protocol changes in an incompatable way */
#define ISP_PROTO_VER           1

/* flags for isp_stab_set() */
#define ISP_PROVIDES            1
#define ISP_REQUIRES            2
#define ISP_REMOVES             4

typedef enum {
    ISP_FILE, ISP_STR, ISP_DOUBLE, ISP_UINT64, ISP_INT64,
} isp_type_t;

struct isp_stab_struct { 
    char        *name;
    isp_type_t   type;
    int          flags;
};

typedef struct xml_el_struct     *isp_unit_t;
typedef struct isp_handle_struct *isp_handle_t;

typedef int (*isp_mapfun_t)(isp_unit_t u, void *arg);

int   isp_init(isp_handle_t *h, int flags, int argc, char *argv[], 
               struct isp_stab_struct stab[], int splitfactor);
int   isp_fini(isp_handle_t h);

int   isp_unit_map(isp_handle_t h, isp_mapfun_t mapfun, void *arg);

int   isp_unit_read(isp_handle_t h, isp_unit_t *u);
int   isp_unit_write(isp_handle_t h, isp_unit_t u);

int   isp_unit_init(isp_unit_t u);
int   isp_unit_fini(isp_unit_t u, int result);

int   isp_unit_create(isp_unit_t *up);
int   isp_unit_destroy(isp_unit_t u);
int   isp_unit_copy(isp_unit_t *up, isp_unit_t u);

int   isp_result_upstream_get(isp_unit_t u, int *code);
int   isp_rwfile_check(isp_unit_t u);

int   isp_file_source(isp_unit_t u, char *key, char *path, int flags);
int   isp_file_sink(isp_unit_t u, char *key);
int   isp_file_access(isp_unit_t u, char *key, char **pathp, int flags);
int   isp_file_rename(isp_unit_t u, char *key, char *newpath);

int   isp_meta_source(isp_unit_t u, char *key, isp_type_t type, ...);
int   isp_meta_sink(isp_unit_t u, char *key);
int   isp_meta_get(isp_unit_t u, char *key, isp_type_t type, ...);
int   isp_meta_set(isp_unit_t u, char *key, isp_type_t type, ...);

void  isp_errx(int exitval, const char *fmt, ...);
void  isp_err(const char *fmt, ...);
char *isp_errstr(int errnum);

#define ISP_ESUCCESS        0   /* Success */
#define ISP_ENOTRUN         1   /* No result */
#define ISP_ENOKEY          2   /* File/metadata key lookup failed */
#define ISP_EDUPKEY         3   /* File/metadata key already exists */
#define ISP_ECORRUPT        4   /* File corruption detected */
#define ISP_ENOENT          5   /* File does not exist */
#define ISP_EEXITED         6   /* Subprocess exited with nonzero status */
#define ISP_ESIGNAL         7   /* Subprocess died on signal */
#define ISP_ESTOPPED        8   /* Subprocess was stopped */
#define ISP_EWAIT           9   /* Wait for subprocess failed */
#define ISP_EEOF            10  /* EOF on read */
#define ISP_EREAD           11  /* Read error */
#define ISP_EWRITE          12  /* Write error */
#define ISP_EWOULDBLOCK     13  /* Read operation would block */
#define ISP_ENOMEM          14  /* Out of memory */
#define ISP_EFCNTL          15  /* Fcntl error */
#define ISP_EPARSE          16  /* XML parse error */
#define ISP_EPOLL           17  /* Poll error */
#define ISP_ENOTCLOSED      18  /* XML document is still open */
#define ISP_ETIME           19  /* Gettimeofday error */
#define ISP_EMKTMP          20  /* mktmp error */
#define ISP_ECOPY           21  /* error copying file */
#define ISP_EREDIRECT       22  /* redirection error */
#define ISP_EFORK           23  /* fork error */
#define ISP_EEXEC           24  /* exec error */
#define ISP_EPIPE           25  /* pipe error */
#define ISP_ERENAME         26  /* rename error */
#define ISP_EATTR           27  /* XML attribute error */
#define ISP_EINVAL          28  /* invalid arguments */
#define ISP_ENOINIT         29  /* ISP not initialized yet */
#define ISP_EELEMENT        30  /* malformed ISP element */
#define ISP_EGETCWD         31  /* getcwd failure */
#define ISP_EBADF           32  /* I/O to a bad handle */
#define ISP_ERWFILE         33  /* Unit contains unsinked read-write files */
#define ISP_EDOCUMENT       34  /* Malformed ISP document */
#define ISP_EBIND           35  /* Pipeline binding error */
#define ISP_EUSERFATAL      1024 /* Application generic fatal error */
#define ISP_EUSER           1025 /* Application generic non-fatal error */
#define ISP_USER_ERROR_BASE 1026

#ifdef __cplusplus
  }
#endif

#endif /* _ISP_H */

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
