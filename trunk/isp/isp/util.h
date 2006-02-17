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

#ifndef _UTIL_H
#define _UTIL_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef __cplusplus
  extern "C" {
#endif

/* All int functions return ISP_ESUCCESS or other error code
 * unless otherwise noted.
 */

int     util_mkcopy(char *path, char *npath);
int     util_mktmp(int *fdp, char **pathp);
int     util_mktmp_copy(char *opath, int *fdp, char **pathp);

int     util_md5_digest(char *path, char **digestp);

/* special fd values for util_runcmd() */
#define FDCLOSE    (-1)
#define FDIGNORE   (-2)

int     util_runcmd(char **argv, int *wstat, int ifd, int ofd, int efd);
int     util_runcoproc(char **argv, pid_t *pidp, int *ifd, int *ofd, int *efd);

/* Routines for manipulating null-terminated arrays of strings.
 */

int     util_argvlen(char **av); /* returns array length not including NULL */
int     util_argvcat(char **av1, char **av2, char ***avp);
int     util_argvdup(char **av, char ***avp);
int     util_argvdupc(int ac, char **av, char ***avp);
int     util_argstr(char **av, char **strp);

/* These are wrappers around the read, write, and waitpid system
 * calls that retry on EINTR.  Return values are same as the system calls.
 */
int     util_read(int fd, void *p, int max);
int     util_write(int fd, void *p, int max);
pid_t   util_waitpid(pid_t pid, int *status, int options);

/* This poll wrapper retries on EINTR and uses handy pfd_t.
 * It can also emulate poll using select.
 * Int functions return ISP_ESUCCESS or other error.
 */
typedef struct pfd_struct *pfd_t;

int     util_poll(pfd_t pfd, struct timeval *timeout);

int     util_pfd_create(pfd_t *pfdp);
void    util_pfd_destroy(pfd_t pfd);
void    util_pfd_zero(pfd_t pfd);
int     util_pfd_set(pfd_t pfd, int fd, short events);
short   util_pfd_revents(pfd_t pfd, int fd);

/* Create a string representation of a pfd within the allotted space.
 * No error possible.
 */
char   *util_pfd_str(pfd_t pfd, char *str, int len);

#if !HAVE_POLL /* need these for poll emulation with select */
#ifndef POLLIN
#define POLLIN      1
#define POLLOUT     2
#define POLLHUP     4
#define POLLERR     8
#define POLLNVAL    16
#endif
#endif

#ifdef __cplusplus
  }
#endif

#endif /* ! _UTIL_H */

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
