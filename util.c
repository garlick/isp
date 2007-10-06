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
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <errno.h>
#include <pwd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <sys/time.h>

#if HAVE_OPENSSL
#include <openssl/md5.h>
#endif

#include "util.h"
#include "xml.h"
#include "isp.h"
#include "isp_private.h"
#include "macros.h"

#ifndef MAX
#define MAX(a,b)    ((a) > (b) ? (a) : (b))
#endif

#define POLLFD_ALLOC_CHUNK  16
#define POLLFD_MAGIC    0x56452334
struct pfd_struct {
    int magic;
#if HAVE_POLL
    unsigned int nfds;
    unsigned int ufds_size;
    struct pollfd *ufds;
#else /* select */
    int maxfd;
    fd_set rset;
    fd_set wset;
#endif
};

/* Copy path to open file descriptor.
 * Returns ISP_ESUCCESS or other error code.
 */
static int _copyfile(char *path, int nfd)
{
    char buf[8192];
    int fd; 
    int res = ISP_ESUCCESS;
    int n;

    if ((fd = open(path, O_RDONLY)) < 0) {
        isp_dbgfail("_copyfile: open O_RDONLY %s: %m", path);
        res = ISP_ECOPY;
        goto done;
    }
    do {
        if ((n = util_read(fd, buf, sizeof(buf))) < 0) {
            isp_dbgfail("_copyfile: read %s: %m", path);
            res = ISP_ECOPY;
            goto done;
        }
        if (n > 0 && (n = util_write(nfd, buf, n)) <= 0) {
            isp_dbgfail("_copyfile: write: %m");
            res = ISP_ECOPY;
            goto done;
        }
    } while (n > 0);

done:
    if (fd >= 0 && close(fd) < 0) {
        isp_dbgfail("_copyfile: close %s: %m", path);
        res = ISP_ECOPY;
    }
    return res;
}

/* Copy path to npath.
 * Returns ESUCCESS or other error code.
 */
PUBLIC int 
util_mkcopy(char *path, char *npath)
{
    int nfd;
    int res = ISP_ESUCCESS;

    if ((nfd = open(npath, O_WRONLY | O_CREAT, 0600)) < 0) {
        isp_dbgfail("util_mkcopy: open O_WRONLY | O_CREAT %s: %m", npath);
        res = ISP_ECOPY;
        goto done;
    }
    if ((res = _copyfile(path, nfd)) != ISP_ESUCCESS)
        goto done;

done:
    if (nfd >= 0 && close(nfd) < 0) {
        isp_dbgfail("util_mkcopy: close %s: %m", npath);
        res = ISP_ECOPY;
    }
    return res;
}

#define FILE_TMPL "/isptmpXXXXXX"
/* Make a tmp copy of file.  If fdp is non-NULL assign open file
 * descriptor.  If path is non-NULL, assign path (caller must free).
 * Returns ESUCCESS or other error code.
 */
PUBLIC int 
util_mktmp(int *fdp, char **pathp)
{
    char buf[MAXPATHLEN+1];
    char *path;
    int fd;
    int res = ISP_ESUCCESS;

    path = getcwd(buf, MAXPATHLEN+1);
    if (path == NULL)  {
        isp_dbgfail("util_mktmp: getcwd: %m");
        res = ISP_EGETCWD;
        goto done;
    }
    strcat(path, FILE_TMPL);
    if ((fd = mkstemp(path)) < 0) {
        isp_dbgfail("util_mktmp: mkstemp %s: %m", path);
        res = ISP_EMKTMP;
        goto done;
    }
    if (pathp) {
        *pathp = strdup(path);
        if (!*pathp) {
            res = ISP_ENOMEM;
            (void)close(fd);
            (void)unlink(path);
            goto done;
        }
    }
    if (fdp)
        *fdp = fd;
    else {
        if (close(fd) < 0) {
            isp_dbgfail("util_mktmp: close %s: %m", path);
            res = ISP_EMKTMP;
            if (pathp)
                free(*pathp);
            (void)close(fd);
            (void)unlink(path);
            goto done;
        }
    }

done:
    return res;
}   

/* Make a temp copy of file (opath).  If fdp is non-NULL assign open
 * file descriptor.  If pathp is non-NULL assign path (caller must free).
 * Returns ESUCCESS or other error code.
 */
PUBLIC int 
util_mktmp_copy(char *opath, int *fdp, char **pathp)
{
    char *path;
    int fd;
    int res = ISP_ESUCCESS;

    res = util_mktmp(&fd, &path);
    if (res != ISP_ESUCCESS)
        goto done;
    res = _copyfile(opath, fd);
    if (res != ISP_ESUCCESS) {
        (void)close(fd);
        (void)unlink(path);
        free(path);
        goto done;
    }
    if (fdp) { 
        if (lseek(fd, 0L, SEEK_SET) < 0) {
            isp_dbgfail("util_mktmp_copy: lseek %s: %m", path);
            (void)close(fd);
            (void)unlink(path);
            free(path);
            goto done;
        }
        *fdp = fd;
    } else
        if (close(fd) < 0) {
            isp_dbgfail("util_mktmp_copy: close %s: %m", path);
            (void)unlink(path);
            free(path);
            goto done;
        }
    if (pathp)
        *pathp = path;

done:
    return res;
}

#if HAVE_OPENSSL
static char *
_hexify(unsigned char *d, int len)
{
    char *str = malloc(len*2 + 1);
    int i;

    if (str != NULL) {
        for (i = 0; i < len; i++)
            sprintf(&str[i*2], "%-2.2x", d[i]);
        str[len*2] = '\0';
    }

    return str;
}
#endif

PUBLIC int 
util_md5_digest(char *path, char **digestp)
{
    int res = ISP_ESUCCESS;
#if HAVE_OPENSSL
    unsigned char digest[MD5_DIGEST_LENGTH];
    char buf[8192];
    MD5_CTX ctx;
    int fd, n;

    MD5_Init(&ctx);

    if ((fd = open(path, O_RDONLY)) < 0) {
        isp_dbgfail("util_md5_digest: open %s: %m", path);
        res = ISP_ENOENT;
        goto done;
    }
    do {
        n = util_read(fd, buf, sizeof(buf));
        if (n < 0) {
            isp_dbgfail("util_md5_digest: read %s: %m", path);
            res = ISP_EREAD;
            (void)close(fd);
            goto done;
        }
        if (n > 0)
            MD5_Update(&ctx, buf, n);
    } while (n > 0);
    if (close(fd) < 0) {
        isp_dbgfail("util_md5_digest: close %s: %m", path);
        res = ISP_EREAD;
        goto done;
    }

    MD5_Final(digest, &ctx);

    *digestp = _hexify(digest, MD5_DIGEST_LENGTH);
#else
    *digestp = strdup("");
#endif
    if (*digestp == NULL) {
        res = ISP_ENOMEM;
        goto done;
    }
done:
    return res;
}

static int 
_redirect_fd(int nfd, int ofd)
{
    int res = ISP_ESUCCESS;

    switch (nfd) {
        case FDCLOSE:
            if (close(ofd) < 0)
                res = ISP_EREDIRECT;
            break;
        case FDIGNORE:
            break;
        default:
            assert(nfd >= 0);
            if (close(ofd) < 0) {
                res = ISP_EREDIRECT;
                break;
            }
            if (dup2(nfd, ofd) < 0)
                res = ISP_EREDIRECT;
            break;
    }
    return res;
}

PUBLIC int 
util_runcmd(char **argv, int *wstat, int ifd, int ofd, int efd)
{
    pid_t pid;
    int res = ISP_ESUCCESS;
    int s;

    switch ((pid = fork())) {
        case -1:/* error */
            res = ISP_EFORK;
            break;
            
        case 0: /* child */
            if ((res = _redirect_fd(ifd, 0)) != ISP_ESUCCESS)
                exit(1);
            if ((res = _redirect_fd(ofd, 1)) != ISP_ESUCCESS)
                exit(1);
            if ((res = _redirect_fd(efd, 2)) != ISP_ESUCCESS)
                exit(1);
            /* exec program */
            if (execvp(argv[0], argv) < 0) {
                exit(1);
            }
            /*NOTREACHHED*/
            break;

        default: /* parent */
            if (util_waitpid(pid, &s, 0) < 0) {
                res = ISP_EWAIT;
                if (wstat)
                    *wstat = errno;
            } else {
                if (WIFEXITED(s) && WEXITSTATUS(s) != 0)
                    res = ISP_EEXITED;
                else if (WIFSIGNALED(s))
                    res = ISP_ESIGNAL;
                else if (WIFSTOPPED(s))
                    res = ISP_ESTOPPED;
                if (wstat)
                    *wstat = s;
            }
            break;
    }

    return res;
}

PUBLIC int 
util_runcoproc(char **argv, pid_t *pidp, int *ifd, int *ofd, int *efd)
{
    pid_t pid;
    int c0[2]; /* child stdin */
    int c1[2]; /* child stdout */
    int c2[2]; /* child stderr */
    int res = ISP_ESUCCESS;

    if (ifd) {
        if (pipe(c0) < 0) {
            res = ISP_EPIPE;
            goto done;
        }
        *ifd = c0[1];
    }
    if (ofd) {
        if (pipe(c1) < 0) {
            if (ifd) { /* error - clean up child stdin */
                (void)close(c0[0]);
                (void)close(c0[1]);
            }
            res = ISP_EPIPE;
            goto done;
        }
        *ofd = c1[0];
    }
    if (efd) {
        if (pipe(c2) < 0) {
            if (ifd) { /* error - clean up child stdin */
                (void)close(c0[0]);
                (void)close(c0[1]);
            }
            if (ofd) { /* error - clean up child stdout */
                (void)close(c1[0]);
                (void)close(c1[1]);
            }
            res = ISP_EPIPE;
            goto done;
        }
        *efd = c2[0];
    }

    switch ((pid = fork())) {
        case -1:/* error */
            res = ISP_EFORK;
            break;
            
        case 0: /* child */
            /* close the "wrong end" of pipes */
            if (ifd && close(c0[1]) < 0)
                exit(1);
            if (ofd && close(c1[0]) < 0)
                exit(1);
            if (efd && close(c2[0]) < 0)
                exit(1);

            if ((res = _redirect_fd(ifd ? c0[0] : FDCLOSE, 0)) != ISP_ESUCCESS)
                exit(1);
            if ((res = _redirect_fd(ofd ? c1[1] : FDCLOSE, 1)) != ISP_ESUCCESS)
                exit(1);
            if ((res = _redirect_fd(efd ? c2[1] : FDIGNORE, 2)) != ISP_ESUCCESS)
                exit(1);

            /* exec program */
            if (execvp(argv[0], argv) < 0)
                exit(1);
            /*NOTREACHED*/
            break;

        default: /* parent */
            /* close the "wrong end" of pipes */
            if (ifd && close(c0[0]) < 0) {
                res = ISP_EPIPE;
                goto done;
            }
            if (ofd && close(c1[1]) < 0) {
                res = ISP_EPIPE;
                goto done;
            }
            if (efd && close(c2[1]) < 0) {
                res = ISP_EPIPE;
                goto done;
            }
            break;
    }

done:
    if (res == ISP_ESUCCESS && pidp)
        *pidp = pid;
    return res;
}

PUBLIC void 
util_argvfree(char **av)
{
    int i = 0;

    while (av[i] != NULL) {
        free(av[i]);
        av[i] = NULL;
    }
    free(av);
}

PUBLIC int 
util_argvlen(char **av)
{
    int i = 0;

    while (av[i] != NULL)
        i++;

    return i;
}

PUBLIC int 
util_argvcat(char **av1, char **av2, char ***avp)
{
    int l1 = util_argvlen(av1);
    int l2 = util_argvlen(av2);
    char **nargv = (char **)malloc(sizeof(char *)*(l1 + l2 + 1));
    int i;
    int res = ISP_ESUCCESS;

    if (!nargv) {
        res = ISP_ENOMEM;
        goto done;
    }

    for (i = 0; i < l1; i++)
        nargv[i] = av1[i];
    for (i = 0; i < l2; i++)
        nargv[i + l1] = av2[i];
    nargv[l2 + l1] = NULL;

done:
    if (avp)
        *avp = nargv;
    return res;
}

PUBLIC int 
util_argvdup(char **av, char ***avp)
{
    int l = util_argvlen(av);
    char **nargv = (char **)malloc(sizeof(char *)*(l + 1));
    int i;
    int res = ISP_ESUCCESS;

    if (!nargv) {
        res = ISP_ENOMEM;
        goto done;
    }
    for (i = 0; i < l; i++)
        nargv[i] = av[i];
    nargv[l] = NULL;

done:
    if (avp)
        *avp = nargv;
    return res;
}

PUBLIC int 
util_argvdupc(int ac, char **av, char ***avp)
{
    char **nargv = (char **)malloc(sizeof(char *)*(ac + 1));
    int i;
    int res = ISP_ESUCCESS;

    if (!nargv) {
        res = ISP_ENOMEM;
        goto done;
    }
    for (i = 0; i < ac; i++)
        nargv[i] = av[i];
    nargv[ac] = NULL;

done:
    if (avp && res == ISP_ESUCCESS)
        *avp = nargv;
    return res;
}

PUBLIC int 
util_argstr(char **av, char **strp)
{
    char *str = NULL;
    char *old;
    int res = ISP_ESUCCESS;

    while (*av != NULL) {
        if (str == NULL) {
            str = strdup(*av);
            if (!str) {
                res = ISP_ENOMEM;
                goto done;
            }
        } else {
            old = str;
            if (asprintf(&str, "%s %s", old, *av) < 0) {
                res = ISP_ENOMEM;
                free(old);
                goto done;
            }
            free(old);
        }
        av++;
    }

done:
    if (strp && res == ISP_ESUCCESS)
        *strp = str;
    return res;
}

PUBLIC int 
util_read(int fd, void *p, int max)
{
    int n;

    do {
        n = read(fd, p, max);
    } while (n < 0 && errno == EINTR);

    return n;
}

PUBLIC int 
util_write(int fd, void *p, int max)
{
    int n;

    do {
        n = write(fd, p, max);
    } while (n < 0 && errno == EINTR);

    return n;
}

PUBLIC pid_t 
util_waitpid(pid_t pid, int *status, int options)
{
    pid_t n;

    do {
        n = waitpid(pid, status, options);
    } while (n < 0 && errno == EINTR);

    return n;
}

PUBLIC int 
util_poll(pfd_t pfd, struct timeval *tv)
{
    struct timeval tv_cpy, start, end, delta;
    int n;
    int res = ISP_ESUCCESS;

    if (tv) {
        if (gettimeofday(&start, NULL) < 0) {
            res = ISP_ETIME;
            goto done;
        }
        tv_cpy = *tv;
    } else {
#ifndef NDEBUG
        /* Better have POLLIN | POLLOUT set at least one fd or select/poll 
         * will block forever.
         */
        int ok = 0;
        int i;
#if HAVE_POLL
        for (i = 0; i < pfd->nfds; i++)
            if ((pfd->ufds[i].events & (POLLIN | POLLOUT))) {
                ok = 1;
                break;
            }
#else
        for (i = 0; i <= pfd->maxfd; i++)
            if (FD_ISSET(i, &pfd->rset) || FD_ISSET(i, &pfd->wset)) {
                ok = 1;
                break;
            }
#endif
        assert(ok);
#endif /*NDEBUG*/
    }

    do {
#if HAVE_POLL
        /* -1 == infinite timeout */
        int tv_msec = tv ? (tv_cpy.tv_sec * 1000 + tv_cpy.tv_usec / 1000) : -1;

        n = poll(pfd->ufds, pfd->nfds, tv_msec);
#else
        /* NULL == infinite timeout */
        n = select(pfd->maxfd + 1, &pfd->rset, &pfd->wset, NULL, 
                tv ? &tv_cpy : NULL);
#endif
        if (n < 0 && errno == EINTR && tv != NULL) {
            if (gettimeofday(&end, NULL) < 0) {
                res = ISP_ETIME;
                goto done;
            }
            timersub(&end, &start, &delta);     /* delta = end-start */
            timersub(tv, &delta, &tv_cpy);      /* tv_cpy = tv-delta */
        }
    } while (n < 0 && errno == EINTR);

    if (n < 0) {
        res = ISP_EPOLL;
        goto done;
    }

done:
    return res;
}

#if HAVE_POLL
static int _grow_pollfd(pfd_t pfd, int n)
{
    int res = ISP_ESUCCESS;

    assert(pfd->magic == POLLFD_MAGIC);

    while (res == ISP_ESUCCESS && pfd->ufds_size < n) {
        pfd->ufds_size += POLLFD_ALLOC_CHUNK;
        pfd->ufds = (struct pollfd *)realloc((char *)pfd->ufds, 
                     sizeof(struct pollfd) * pfd->ufds_size);
        if (pfd->ufds == NULL)
            res = ISP_ENOMEM;
    }
    return res;
}
#endif

PUBLIC int 
util_pfd_create(pfd_t *pfdp)
{
    pfd_t pfd;
   
    if (!(pfd = (pfd_t)calloc(1, sizeof(struct pfd_struct))))
        goto nomem;
    pfd->magic = POLLFD_MAGIC;
#if HAVE_POLL
    pfd->ufds_size = POLLFD_ALLOC_CHUNK;
    if (!(pfd->ufds = (struct pollfd *)calloc(1, 
                        sizeof(struct pollfd) * pfd->ufds_size)))
        goto nomem;
    pfd->nfds = 0;
#else
    pfd->maxfd = 0;
    FD_ZERO(&pfd->rset);
    FD_ZERO(&pfd->wset);
#endif

    if (pfdp)
        *pfdp = pfd;
    return ISP_ESUCCESS;
nomem:
    if (pfd)
        util_pfd_destroy(pfd);
    return ISP_ENOMEM;
}

PUBLIC void 
util_pfd_destroy(pfd_t pfd)
{
    assert(pfd->magic == POLLFD_MAGIC);
    pfd->magic = 0;
#if HAVE_POLL
    if (pfd->ufds);
        free(pfd->ufds);
#endif
    free(pfd);
}

PUBLIC void 
util_pfd_zero(pfd_t pfd)
{
    assert(pfd->magic == POLLFD_MAGIC);
#if HAVE_POLL
    pfd->nfds = 0;
#else
    FD_ZERO(&pfd->rset);
    FD_ZERO(&pfd->wset);
    pfd->maxfd = 0;
#endif
}

PUBLIC int 
util_pfd_set(pfd_t pfd, int fd, short events)
{
    int res = ISP_ESUCCESS;
#if HAVE_POLL
    int i;

    assert(pfd->magic == POLLFD_MAGIC);
    for (i = 0; i < pfd->nfds; i++) {
        if (pfd->ufds[i].fd == fd) {
            pfd->ufds[i].events |= events;
            break;
        }
    }
    if (i == pfd->nfds) { /* not found */
        if ((res = _grow_pollfd(pfd, ++pfd->nfds)) == ISP_ESUCCESS) {
            pfd->ufds[i].fd = fd;
            pfd->ufds[i].events = events;
        }
    }
#else
    assert(pfd->magic == POLLFD_MAGIC);
    assert(fd < FD_SETSIZE);
    if (events & POLLIN)
        FD_SET(fd, &pfd->rset);
    if (events & POLLOUT)
        FD_SET(fd, &pfd->wset);
    pfd->maxfd = MAX(pfd->maxfd, fd);
#endif

    return res;
}

PUBLIC char *
util_pfd_str(pfd_t pfd, char *str, int len)
{
    int i;
#if HAVE_POLL
    int maxfd = -1;
#endif

    assert(pfd->magic == POLLFD_MAGIC);
#if HAVE_POLL
    memset(str, '.', len);
    for (i = 0; i < pfd->nfds; i++) {
        int fd = pfd->ufds[i].fd;
        short revents = pfd->ufds[i].revents;

        if (fd < len - 1) {
            if (revents) {
                if (revents & (POLLNVAL | POLLERR | POLLHUP))
                    str[fd] = 'E';
                else if (revents & POLLIN)
                    str[fd] = 'I';
                else if (revents & POLLOUT)
                    str[fd] = 'O';
            } 
            if (fd > maxfd)
                maxfd = fd;
        }
    }
    assert(maxfd + 1 < len);
    str[maxfd + 1] = '\0';
#else
    for (i = 0; i <= pfd->maxfd; i++) {
        if (FD_ISSET(i, &pfd->rset))
            str[i] = 'I';
        else if (FD_ISSET(i, &pfd->wset))
            str[i] = 'O';
        else
            str[i] = '.';
    }
    assert(i < len);
    str[i] = '\0';
#endif
    return str;
}

PUBLIC short 
util_pfd_revents(pfd_t pfd, int fd)
{
    short flags = 0;
#if HAVE_POLL
    int i;

    assert(pfd->magic == POLLFD_MAGIC);
    for (i = 0; i < pfd->nfds; i++) {
        if (pfd->ufds[i].fd == fd) {
            flags = pfd->ufds[i].revents;
            break;
        }
    }
#else
    assert(pfd->magic == POLLFD_MAGIC);
    if (FD_ISSET(fd, &pfd->rset))
        flags |= POLLIN;
    if (FD_ISSET(fd, &pfd->wset))
        flags |= POLLOUT;
#endif
    return flags;
} 

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
