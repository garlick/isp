/*
 * $Id$
 *
 * Say we want a file read-only, then write to it to trigger
 * a downstream ISP_ECORRUPT.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <stdio.h>

#include <isp/isp.h>

static int 
mapfun(isp_unit_t u, void *arg)
{
    int res = ISP_ESUCCESS;
    char *path, c;
    int fd;

    if ((res = isp_file_access(u, "file", &path, ISP_RDONLY)) != ISP_ESUCCESS)
        isp_errx(1, "isp_file_access: %s", isp_errstr(res));
    if ((fd = open(path, O_RDWR)) < 0)
        isp_errx(1, "open: %m");
    if (lseek(fd, 0L, SEEK_SET) < 0)
        isp_errx(1, "lseek: %m");
    if (read(fd, &c, 1) < 0) 
        isp_errx(1, "read: %m");
    c++;
    if (write(fd, &c, 1) < 0) 
        isp_errx(1, "write: %m");
    if (close(fd) < 0) 
        isp_errx(1, "close: %m");

    return res;
}

int 
main(int argc, char *argv[])
{
    int res;
    isp_handle_t h;

    if ((res = isp_init(&h, ISP_SOURCE | ISP_SINK, argc, argv, NULL, 1)) != ISP_ESUCCESS)
        isp_errx(1, "isp_init: %s", isp_errstr(res));
    if ((res = isp_unit_map(h, mapfun, NULL)) != ISP_ESUCCESS)
        isp_errx(1, "isp_unit_map: %s", isp_errstr(res));
    if ((res = isp_fini(h)) != ISP_ESUCCESS)
        isp_errx(1, "isp_fini: %s", isp_errstr(res));

    exit(0);
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
