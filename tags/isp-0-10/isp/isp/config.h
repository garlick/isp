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

#ifndef _CONFIG_H
#define _CONFIG_H

/* Configures external error functions in list.c
 */
#undef WITH_PTHREADS               
#undef WITH_LSD_FATAL_ERROR_FUNC     /* only matters if WITH_PTHREADS */
#undef WITH_LSD_NOMEM_ERROR_FUNC     /* return NULL on out of memory */

/* Configures poll vs select in util.c.
 */
#define HAVE_POLL                   1

/* Configures util_md5_digest() in util.c.
 */
#define HAVE_OPENSSL                1

#endif /* _CONFIG_H */

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
