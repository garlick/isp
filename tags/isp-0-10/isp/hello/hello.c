/*****************************************************************************\
 *  $Id$
 *****************************************************************************
 *  Copyright (C) 2005 The Regents of the University of California.
 *  Produced at Lawrence Livermore National Laboratory (cf, DISCLAIMER).
 *  Written by Jim Garlick <garlick@llnl.gov>.
 *  UCRL-CODE-218333
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <isp/isp.h>

static struct isp_stab_struct stab[] = {
    { .name = "x", .type = ISP_INT64, .flags = ISP_REQUIRES },
    { .name = "y", .type = ISP_INT64, .flags = ISP_REQUIRES },
    { .name = "z", .type = ISP_INT64, .flags = ISP_PROVIDES },
    { .name = NULL },
};

static int
multxy(isp_unit_t u, void *arg)
{
    int64_t x, y;
    int res;

    if ((res = isp_meta_get(u, "x", ISP_INT64, &x)) != ISP_ESUCCESS)
        goto done;
    if ((res = isp_meta_get(u, "y", ISP_INT64, &y)) != ISP_ESUCCESS)
        goto done;
    res = isp_meta_source(u, "z", ISP_INT64, x*y);
done:
    return res;
}

int
main(int argc, char *argv[])
{
    int res;
    isp_handle_t h;

    res = isp_init(&h, ISP_SOURCE|ISP_SINK, argc, argv, stab, 1);
    if (res != ISP_ESUCCESS)
        isp_errx(1, "isp_init: %s", isp_errstr(res));

    res = isp_unit_map(h, multxy, NULL);
    if (res != ISP_ESUCCESS)
        isp_errx(1, "isp_unit_map: %s", isp_errstr(res));

    res = isp_fini(h);
    if (res != ISP_ESUCCESS)
        isp_errx(1, "isp_fini: %s", isp_errstr(res));

    exit(0);
}
