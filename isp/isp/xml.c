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
#include <stdarg.h>
#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <expat.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>

/* XXX not seeing this prototype in stdio.h for some reason... */
int vsscanf(const char *str, const char *fmt, va_list ap); 

#include "isp.h"
#include "list.h"
#include "xml.h"
#include "macros.h"

#define XML_EL_MAGIC 0x42424242
struct xml_el_struct {
    int magic;
    char *name;
    List attrs;     /* list of attributes for this element */
    List els;       /* list of elements defined within this element */
    struct xml_el_struct *parent;
};

#define XML_ATTR_MAGIC 0x43434343
struct xml_attr_struct {
    int magic;
    char *name;
    char *value;
};

#define XML_ATTR_ITERATOR_MAGIC 0x45454545
struct xml_attr_iterator_struct {
    int magic;
    ListIterator itr;
};

#define XML_EL_ITERATOR_MAGIC 0x46464646
struct xml_el_iterator_struct {
    int magic;
    ListIterator itr;
};

PRIVATE int
xml_el_create(const char *name, xml_el_t *elp) 
{
    xml_el_t new;
    int size = sizeof(struct xml_el_struct);
   
    if (!(new = (xml_el_t)calloc(1, size)))
        goto nomem;
    new->magic = XML_EL_MAGIC;
    if (!(new->name = strdup(name)))
        goto nomem;
    if (!(new->attrs = list_create((ListDelF)xml_attr_destroy)))
        goto nomem;
    if (!(new->els = list_create((ListDelF)xml_el_destroy)))
        goto nomem;

    if (elp)
        *elp = new;
    return ISP_ESUCCESS;
nomem:
    if (new)
        xml_el_destroy(new);
    return ISP_ENOMEM;
}

/* helper for xml_el_copy */
static int
_copy_attributes(xml_el_t dst, xml_el_t src)
{
    xml_attr_iterator_t ai;
    xml_attr_t attr, anew;
    int res;

    if ((res = xml_attr_iterator_create(src, &ai)) == ISP_ESUCCESS) {
        while ((attr = xml_attr_next(ai))) {
            if ((res = xml_attr_copy(&anew, attr)) != ISP_ESUCCESS)
                break;
            if ((res = xml_attr_append(dst, anew)) != ISP_ESUCCESS)
                break;
        }
        xml_attr_iterator_destroy(ai);
    }
    return res;
}

/* helper for xml_el_copy */
static int
_copy_elements(xml_el_t dst, xml_el_t src)
{
    xml_el_iterator_t ei;
    xml_el_t elem, enew;
    int res;

    if ((res = xml_el_iterator_create(src, &ei)) == ISP_ESUCCESS) {
        while ((elem = xml_el_next(ei))) {
            if ((res = xml_el_copy(&enew, elem)) != ISP_ESUCCESS)
                break;
            if ((res = xml_el_append(dst, enew)) != ISP_ESUCCESS)
                break;
        }
        xml_el_iterator_destroy(ei);
    }
    return res;
}

PRIVATE int
xml_el_copy(xml_el_t *elp, xml_el_t el)
{
    xml_el_t new;
    int res;

    assert(el != NULL);
    assert(el->magic == XML_EL_MAGIC);

    if ((res = xml_el_create(el->name, &new)) != ISP_ESUCCESS)
        goto error;
    if ((res = _copy_attributes(new, el)) != ISP_ESUCCESS)
        goto error;
    if ((res = _copy_elements(new, el)) != ISP_ESUCCESS)
        goto error;

    if (elp)
        *elp = new;
    return res;
error:
    if (new)
        xml_el_destroy(new);
    return res;
}

PRIVATE void
xml_el_destroy(xml_el_t el) 
{
    assert(el != NULL);
    assert(el->magic == XML_EL_MAGIC);
    el->magic = 0;

    if (el->name)
        free(el->name);
    if (el->attrs)
        list_destroy(el->attrs);
    if (el->els)
        list_destroy(el->els);
    free(el);
}

PRIVATE int
xml_attr_create(char *name, xml_attr_t *attrp, char *fmt, ...)
{
    va_list ap;
    xml_attr_t new;
   
    if (!(new = (xml_attr_t)calloc(1, sizeof(struct xml_attr_struct))))
        goto nomem;
    new->magic = XML_ATTR_MAGIC;
    if (name && !(new->name = strdup(name)))
        goto nomem;
    if (fmt) {
        va_start(ap, fmt);
        if (vasprintf(&new->value, fmt, ap) < 0)
            goto nomem;
        va_end(ap);
        /* FIXME: need to escape embedded quotes */
        assert(strchr(new->value, '"') == NULL); 
    }

    if (attrp)
        *attrp = new;
    return ISP_ESUCCESS;
nomem:
    if (new)
        xml_attr_destroy(new);
    return ISP_ENOMEM;
}

PRIVATE int
xml_attr_copy(xml_attr_t *ap, xml_attr_t a)
{
    int res;
    xml_attr_t new;

    res = xml_attr_create(a->name, &new, "%s", a->value);
    if (res == ISP_ESUCCESS && ap)
        *ap = new;
    return res;
}

PRIVATE void
xml_attr_destroy(xml_attr_t attr)
{
    assert(attr != NULL);
    assert(attr->magic == XML_ATTR_MAGIC);
    attr->magic = 0;

    if (attr->name)
        free(attr->name);
    if (attr->value)
        free(attr->value);
    free(attr);
}

PRIVATE int
xml_attr_iterator_create(xml_el_t el, xml_attr_iterator_t *ip)
{
    xml_attr_iterator_t new;
    int size = sizeof(struct xml_attr_iterator_struct);

    assert(el != NULL);
    assert(el->magic == XML_EL_MAGIC);

    if (!(new = (xml_attr_iterator_t)calloc(1, size)))
        goto nomem;
    new->magic = XML_ATTR_ITERATOR_MAGIC;
    if (!(new->itr = list_iterator_create(el->attrs)))
        goto nomem;

    if (ip)
        *ip = new;
    return ISP_ESUCCESS;
nomem:
    if (new)
        xml_attr_iterator_destroy(new);
    return ISP_ENOMEM;
}

PRIVATE void
xml_attr_iterator_destroy(xml_attr_iterator_t itr)
{
    assert(itr != NULL);
    assert(itr->magic == XML_ATTR_ITERATOR_MAGIC);

    itr->magic = 0;
    if (itr->itr)
        list_iterator_destroy(itr->itr);
    free(itr);
}

PRIVATE xml_attr_t 
xml_attr_next(xml_attr_iterator_t itr)
{
    assert(itr != NULL);
    assert(itr->magic == XML_ATTR_ITERATOR_MAGIC);

    return list_next(itr->itr);

}

PRIVATE char * 
xml_el_name(xml_el_t el)
{
    assert(el != NULL);
    assert(el->magic == XML_EL_MAGIC);

    return el->name;
}

static int
_match_attr_name(xml_attr_t attr, char *name)
{
    assert(attr->name != NULL);

    return !strcmp(attr->name, name);
}

PRIVATE int
xml_el_attr_val(xml_el_t el, char *name, char **valp)
{
    xml_attr_t attr;

    assert(el != NULL);
    assert(el->magic == XML_EL_MAGIC);

    attr = list_find_first(el->attrs, (ListFindF)_match_attr_name, name);
    if (attr && valp)
        *valp = attr->value;

    return (attr ? ISP_ESUCCESS : ISP_ENOKEY);
}

PRIVATE int
xml_el_attr_scanval(xml_el_t el, int n, char *name, char *fmt, ...)
{
    va_list ap;
    int res;
    char *val;

    assert(el != NULL);
    assert(el->magic == XML_EL_MAGIC);

    if ((res = xml_el_attr_val(el, name, &val)) == ISP_ESUCCESS) {
        assert(val != NULL);
        va_start(ap, fmt);
        if (vsscanf(val, fmt, ap) != n)
            res = ISP_EATTR;
        va_end(ap);
    }

    return res;
}

PRIVATE int
xml_el_attr_setval(xml_el_t el, char *name, char *fmt, ...)
{
    va_list ap;
    xml_attr_t attr;
    int res = ISP_ESUCCESS;

    assert(el != NULL);
    assert(el->magic == XML_EL_MAGIC);

    attr = list_find_first(el->attrs, (ListFindF)_match_attr_name, name);
    if (attr) {
        assert(attr->value);
        free(attr->value);
        va_start(ap, fmt);
        if (vasprintf(&attr->value, fmt, ap) < 0)
            res = ISP_ENOMEM;
        va_end(ap);
    } else
        res = ISP_ENOKEY;

    return res;
}

PRIVATE int
xml_attr_append(xml_el_t el, xml_attr_t attr)
{
    int res = ISP_ESUCCESS;

    assert(el != NULL);
    assert(el->magic == XML_EL_MAGIC);
    assert(attr != NULL);
    assert(attr->magic == XML_ATTR_MAGIC);

    if (list_append(el->attrs, attr) == NULL)
        res = ISP_ENOMEM;

    return res;
}

PRIVATE int
xml_el_append(xml_el_t el, xml_el_t el2)
{
    int res = ISP_ESUCCESS;

    assert(el != NULL);
    assert(el->magic == XML_EL_MAGIC);
    assert(el2 != NULL);
    assert(el2->magic == XML_EL_MAGIC);

    if (list_append(el->els, el2) == NULL)
        res = ISP_ENOMEM;
    else
        el2->parent = el;

    return res;
}

PRIVATE int
xml_el_push(xml_el_t el, xml_el_t el2)
{
    int res = ISP_ESUCCESS;

    assert(el != NULL);
    assert(el->magic == XML_EL_MAGIC);
    assert(el2 != NULL);
    assert(el2->magic == XML_EL_MAGIC);

    if (list_push(el->els, el2) == NULL)
        res = ISP_ENOMEM;
    else
        el2->parent = el;

    return res;
}

PRIVATE xml_el_t
xml_el_pop(xml_el_t el)
{
    xml_el_t e;

    assert(el != NULL);
    assert(el->magic == XML_EL_MAGIC);

    e = list_pop(el->els);
    if (e)
        e->parent = NULL;
    return e;
}

PRIVATE xml_el_t
xml_el_peek(xml_el_t el)
{
    assert(el != NULL);
    assert(el->magic == XML_EL_MAGIC);

    return list_peek(el->els); /* pop/peek ref same element */
}

PRIVATE int
xml_el_iterator_create(xml_el_t el, xml_el_iterator_t *ip)
{
    xml_el_iterator_t new;
    int size = sizeof(struct xml_el_iterator_struct);

    assert(el != NULL);
    assert(el->magic == XML_EL_MAGIC);

    if (!(new = (xml_el_iterator_t)calloc(1, size)))
        goto nomem;
    new->magic = XML_EL_ITERATOR_MAGIC;
    if (!(new->itr = list_iterator_create(el->els)))
        goto nomem;

    if (ip)
        *ip = new;
    return ISP_ESUCCESS;
nomem:
    if (new)
        xml_el_iterator_destroy(new);
    return ISP_ENOMEM;
}

PRIVATE void        
xml_el_iterator_destroy(xml_el_iterator_t itr)
{
    assert(itr != NULL);
    assert(itr->magic == XML_EL_ITERATOR_MAGIC);
    itr->magic = 0;

    if (itr->itr)
        list_iterator_destroy(itr->itr);
    free(itr);
}

PRIVATE xml_el_t     
xml_el_next(xml_el_iterator_t itr)
{
    assert(itr != NULL);
    assert(itr->magic == XML_EL_ITERATOR_MAGIC);

    return list_next(itr->itr);
}

PRIVATE xml_attr_t  
xml_attr_find_first(xml_el_t el, xml_attr_match_t fun, void *key)
{
    assert(el != NULL);
    assert(el->magic == XML_EL_MAGIC);

    return (xml_attr_t)list_find_first(el->attrs, (ListFindF)fun, key);
}

PRIVATE xml_el_t
xml_el_find_first(xml_el_t el, xml_el_match_t fun, void *key)
{
    assert(el != NULL);
    assert(el->magic == XML_EL_MAGIC);

    return (xml_el_t)list_find_first(el->els, (ListFindF)fun, key);
}

PRIVATE int
xml_el_count(xml_el_t el)
{
    assert(el != NULL);
    assert(el->magic == XML_EL_MAGIC);

    return list_count(el->els);
}

/* helper for xml_el_to_str() */
static int
_printf(char **bufp, int *sizep, char *fmt, ...)
{
    char *str = NULL;
    va_list ap;
    int res = ISP_ESUCCESS;
    int len;

    va_start(ap, fmt);
    if (vasprintf(&str, fmt, ap) < 0) {
        res = ISP_ENOMEM;
        goto done;
    }
    va_end(ap);
    len = strlen(str);

    if (*sizep == 0)
        *bufp = malloc(len);
    else
        *bufp = realloc(*bufp, *sizep + len);
    if (*bufp == NULL) {
        res = ISP_ENOMEM;
    } else {
        memcpy(*bufp + *sizep, str, len);
        *sizep += len;
    }

done:
    if (str)
        free(str);
    return res;
}

/* helper for xml_el_to_str */
static int
_indent(xml_el_t el)
{
    if (el->parent != NULL)
        return _indent(el->parent) + 2;

    return 0;
}

/* helper for xml_el_to_str */
static int
_put_el_attrs(char **bufp, int *sizep, xml_el_t el)
{
    xml_attr_iterator_t itr;
    xml_attr_t attr;
    int res = ISP_ESUCCESS;

    res = xml_attr_iterator_create(el, &itr);
    if (res != ISP_ESUCCESS)
        goto done;
    while ((attr = xml_attr_next(itr))) {
        res = _printf(bufp, sizep, " %s=\"%s\"", attr->name, attr->value);
        if (res != ISP_ESUCCESS)
            goto done;
    }
    xml_attr_iterator_destroy(itr);

done:
    return res;
}

/* helper for xml_el_to_str */
static int
_put_el(char **bufp, int *sizep, xml_el_t el)
{
    int res = ISP_ESUCCESS;

    if (list_is_empty(el->els)) {
        res = _printf(bufp, sizep, "%*s<%s", _indent(el), "", el->name);
        if (res != ISP_ESUCCESS)
            goto done;
        res = _put_el_attrs(bufp, sizep, el);
        if (res != ISP_ESUCCESS)
            goto done;
        res = _printf(bufp, sizep, "%s", "/>\n");
        if (res != ISP_ESUCCESS)
            goto done;
    } else {
        xml_el_iterator_t i;
        xml_el_t e;

        res = _printf(bufp, sizep, "%*s<%s", _indent(el), "", el->name);
        if (res != ISP_ESUCCESS)
            goto done;
        res = _put_el_attrs(bufp, sizep, el);
        if (res != ISP_ESUCCESS)
            goto done;
        res = _printf(bufp, sizep, "%s", ">\n");
        if (res != ISP_ESUCCESS)
            goto done;

        res = xml_el_iterator_create(el, &i);
        if (res != ISP_ESUCCESS)
            goto done;
        while (res == ISP_ESUCCESS && (e = xml_el_next(i)) != NULL)
            res = _put_el(bufp, sizep, e); /* RECURSE */
        xml_el_iterator_destroy(i);

        if (res == ISP_ESUCCESS) {
            if ((res = _printf(bufp, sizep, "%*s</%s>\n", 
                            _indent(el), "", el->name)) != ISP_ESUCCESS)
                goto done;
        }
    }
done:
    return res;
}

PRIVATE int
xml_el_to_str(xml_el_t el, char **bufp, int *sizep)
{
    char *buf = NULL;
    int size = 0;
    int res = ISP_ESUCCESS;

    assert(el != NULL);

    if ((res = _put_el(&buf, &size, el)) == ISP_ESUCCESS) {
        *bufp = buf;
        *sizep = size;
    } else if (size > 0) {
        free(buf);
    }

    return res;
}

PRIVATE int 
xml_attr_str_append(xml_el_t e, char *name, char *val)
{
    int res = ISP_ESUCCESS;
    xml_attr_t a;

    if ((res = xml_attr_create(name, &a, "%s", val)) != ISP_ESUCCESS)
        goto done;
    if ((res = xml_attr_append(e, a)) != ISP_ESUCCESS) {
        xml_attr_destroy(a);
        goto done;
    }
done:
    return res;
}

PRIVATE int 
xml_attr_int_append(xml_el_t e, char *name, int val)
{
    int res = ISP_ESUCCESS;
    xml_attr_t a;

    if ((res = xml_attr_create(name, &a, "%d", val)) != ISP_ESUCCESS)
        goto done;
    if ((res = xml_attr_append(e, a)) != ISP_ESUCCESS) {
        xml_attr_destroy(a);
        goto done;
    }
done:
    return res;
}

PRIVATE int 
xml_attr_ulong_append(xml_el_t e, char *name, unsigned long val)
{
    int res = ISP_ESUCCESS;
    xml_attr_t a;

    if ((res = xml_attr_create(name, &a, "%lu", val)) != ISP_ESUCCESS)
        goto done;
    if ((res = xml_attr_append(e, a)) != ISP_ESUCCESS) {
        xml_attr_destroy(a);
        goto done;
    }
done:
    return res;
}

PRIVATE xml_el_t
xml_el_parent_get(xml_el_t el)
{
    assert(el && el->magic == XML_EL_MAGIC);

    return el->parent;
}

PRIVATE void
xml_el_parent_set(xml_el_t el, xml_el_t parent)
{
    assert(el && el->magic == XML_EL_MAGIC);

    el->parent = parent;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
