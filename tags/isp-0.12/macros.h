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

#ifndef _MACROS_H
#define _MACROS_H

/* "hidden" symbols are like "static" except they are visible between 
 * object modules within the DSO.  They cannot be used outside of the DSO.
 * Ref: http://people.redhat.com/drepper/dsohowto.pdf 
 */
#ifdef __GNUC__
#define PRIVATE __attribute__ ((visibility ("hidden")))
#define PUBLIC
#else
#warning Not compiling with gcc: DSO will contain extraneous symbols.
#define PRIVATE
#define PUBLIC
#endif

#endif /* _MACROS_H */
/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
