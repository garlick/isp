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
#include <sys/types.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>

#include "isp.h"
#include "util.h"
#include "isp_private.h"
#include "macros.h"

#define ERROR_BUFLEN    1024

static void 
_verr(const char *fmt, va_list ap)
{
    char buf[ERROR_BUFLEN];

    (void)vsnprintf(buf, ERROR_BUFLEN, fmt, ap);
    fprintf(stderr, "%s: %s\n", isp_progname_get(), buf);
}

PUBLIC void
isp_errx(int n, const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    _verr(fmt, ap);
    va_end(ap);
    exit(n);
}

PUBLIC void 
isp_err(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    _verr(fmt, ap);
    va_end(ap);
}

PRIVATE void 
isp_dbgfail(const char *fmt, ...)
{
    va_list ap;

    if (isp_dbgfail_get()) {
        va_start(ap, fmt);
        _verr(fmt, ap);
        va_end(ap);
    }
}

typedef struct {
    int n;
    char *s;
} errtab_t;

static errtab_t errtab[] = {
    { .n = ISP_ESUCCESS,    .s = "Success" },
    { .n = ISP_ENOTRUN,     .s = "No result" },
    { .n = ISP_ENOKEY,      .s = "File/metadata key lookup failed" },
    { .n = ISP_EDUPKEY,     .s = "File/metadata key already exists" },
    { .n = ISP_ECORRUPT,    .s = "File corruption detected" },
    { .n = ISP_ENOENT,      .s = "File does not exist" },
    { .n = ISP_EEXITED,     .s = "Subprocess exited with nonzero status" },
    { .n = ISP_ESIGNAL,     .s = "Subprocess died on signal" },
    { .n = ISP_ESTOPPED,    .s = "Subprocess was stopped" },
    { .n = ISP_EWAIT,       .s = "Wait for subprocess failed" },
    { .n = ISP_EEOF,        .s = "EOF on read" },
    { .n = ISP_EREAD,       .s = "Read error" },
    { .n = ISP_EWRITE,      .s = "Write error" },
    { .n = ISP_EWOULDBLOCK, .s = "ISP call would block" },
    { .n = ISP_ENOMEM,      .s = "Out of memory" },
    { .n = ISP_EFCNTL,      .s = "Fcntl error" },
    { .n = ISP_EPARSE,      .s = "XML parse error" },
    { .n = ISP_EPOLL,       .s = "Poll error" },
    { .n = ISP_ENOTCLOSED,  .s = "XML document is still open" },
    { .n = ISP_ETIME,       .s = "Gettimeofday error" },
    { .n = ISP_EMKTMP,      .s = "Mktmp error" },
    { .n = ISP_ECOPY,       .s = "Error copying file" },
    { .n = ISP_EREDIRECT,   .s = "Redirection error" },
    { .n = ISP_EFORK,       .s = "Fork error" },
    { .n = ISP_EEXEC,       .s = "Exec error" },
    { .n = ISP_EPIPE,       .s = "Pipe error" },
    { .n = ISP_ERENAME,     .s = "Rename error" },
    { .n = ISP_EATTR,       .s = "XML attribute error" },
    { .n = ISP_EINVAL,      .s = "Invalid arguments" },
    { .n = ISP_ENOINIT,     .s = "ISP not initialized yet" },
    { .n = ISP_EELEMENT,    .s = "Malformed ISP element" },
    { .n = ISP_EGETCWD,     .s = "Getcwd failure" },
    { .n = ISP_EBADF,       .s = "IO to an invalid handle" },
    { .n = ISP_ERWFILE,     .s = "Unit contains unsinked, read-write files" },
    { .n = ISP_EDOCUMENT,   .s = "Malformed ISP document" },
    { .n = ISP_EBIND,       .s = "Pipeline binding error" },
    { .n = ISP_EUSERFATAL,  .s = "Fatal application error" },
    { .n = ISP_EUSER,       .s = "Application error" },
};

PUBLIC char *
isp_errstr(int errnum)
{
    int i;

    for (i = 0; i < sizeof(errtab)/sizeof(errtab_t); i++) {
        if (errtab[i].n == errnum)
            return errtab[i].s;
    }
    return "Unknown error";
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
