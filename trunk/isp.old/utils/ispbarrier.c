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

/* This is a "null" filter except it reads stdin to eof before starting to 
 * write stdout.
 */

#ifdef have_config_h
#include "config.h"
#endif
#include <stdlib.h>
#include <isp/isp.h>

int 
main(int argc, char *argv[])
{
    int res;
    isp_handle_t h;
    int flags = ISP_SOURCE | ISP_SINK | ISP_PREPARSE;

    if ((res = isp_init(&h, flags, argc, argv, NULL, 1)) != ISP_ESUCCESS)
        isp_errx(1, "isp_init: %s", isp_errstr(res));
    if ((res = isp_unit_map(h, NULL, NULL)) != ISP_ESUCCESS)
        isp_errx(1, "isp_map: %s", isp_errstr(res));
    if ((res = isp_fini(h)) != ISP_ESUCCESS)
        isp_errx(1, "isp_fini: %s", isp_errstr(res));

    exit(0);
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
