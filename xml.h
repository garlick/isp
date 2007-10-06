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

/* Functions to manipulate XML elements and attributes.
 */

#ifndef _XML_H
#define _XML_H

struct xml_el_struct;
struct xml_attr_struct;
struct xml_attr_iterator_struct;
struct xml_el_iterator_struct;

typedef struct xml_el_struct             *xml_el_t;
typedef struct xml_attr_struct           *xml_attr_t;
typedef struct xml_attr_iterator_struct  *xml_attr_iterator_t;
typedef struct xml_el_iterator_struct    *xml_el_iterator_t;

typedef int (*xml_el_match_t)(xml_el_t x, void *key);   /* 0 == match */
typedef int (*xml_attr_match_t)(xml_el_t x, void *key); /* 0 == match */

/* NOTE:  All functions with int return value return ISP_ESUCCESS or
 * other ISP_E* error code.
 */

/* Create/destroy an element 
 */
int         xml_el_create(const char *name, xml_el_t *elp);
void        xml_el_destroy(xml_el_t el);
int         xml_el_copy(xml_el_t *elp, xml_el_t el);

/* Create/destroy an attribute.
 */
int         xml_attr_create(char *name, xml_attr_t *ap, char *fmt, ...);
void        xml_attr_destroy(xml_attr_t attr);
int         xml_attr_copy(xml_attr_t *ap, xml_attr_t a);

/* Create/destroy/next an attribute iterator (like in list.h).
 */
int         xml_attr_iterator_create(xml_el_t el, xml_attr_iterator_t *ip);
void        xml_attr_iterator_destroy(xml_attr_iterator_t itr);
xml_attr_t  xml_attr_next(xml_attr_iterator_t itr);

xml_attr_t  xml_attr_find_first(xml_el_t el, xml_attr_match_t fun, void *key);

/* Create/destroy/next an element iterator (like in list.h).
 */
int         xml_el_iterator_create(xml_el_t el, xml_el_iterator_t *ip);
void        xml_el_iterator_destroy(xml_el_iterator_t itr);
xml_el_t    xml_el_next(xml_el_iterator_t itr);

xml_el_t    xml_el_find_first(xml_el_t el, xml_el_match_t fun, void *key);

char       *xml_el_name(xml_el_t el);
int         xml_el_attr_val(xml_el_t el, char *name, char **valp);
int         xml_el_attr_scanval(xml_el_t el, int n, char *name, char *fmt, ...);

int         xml_el_attr_setval(xml_el_t el, char *name, char *fmt, ...);

int         xml_attr_append(xml_el_t el, xml_attr_t attr);

int         xml_el_append(xml_el_t el, xml_el_t el2); /* to tail */
int         xml_el_push(xml_el_t el, xml_el_t el2);   /* to head */
xml_el_t    xml_el_pop(xml_el_t el);                 /* from head */
xml_el_t    xml_el_peek(xml_el_t el);                /* from head */
int         xml_el_count(xml_el_t el);

xml_el_t    xml_el_parent_get(xml_el_t el);
void        xml_el_parent_set(xml_el_t el, xml_el_t parent);

/* Convert an element to a string.
 */
int         xml_el_to_str(xml_el_t el, char **bufp, int *sizep);


int         xml_attr_str_append(xml_el_t e, char *name, char *val);
int         xml_attr_int_append(xml_el_t e, char *name, int val);
int         xml_attr_ulong_append(xml_el_t e, char *name, unsigned long val);

#endif /* _XML_H */

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
