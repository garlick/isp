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

/* The init element is always the first one in the XML stream.  There is only
 * one.  It contains a stack of 'filter' elements.  Each filter in the pipeline
 * reads the init element from upstream, pushes its filter element,
 * then passes the init element downstream.  This handshake occurs in 
 * isp_init_handshake() (called from isp_init()).
 *
 * isp.c caches the processed init element and makes it available via
 * the isp_init() accessor.  "our" filter element will be at the top of the
 * stack.
 *
 * Filters contain stabs, and also some other stuff like argv and environ.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#define _GNU_SOURCE
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/param.h>
#include <unistd.h>

#include "util.h"
#include "xml.h"
#include "isp.h"
#include "isp_private.h"
#include "macros.h"

/**
 ** Symbol table functions (part of filter element)
 **/

/* The symbol table provides a rudimentary binding between filters.  
 * By looking at the sybmol tables for upstream filters
 * (by walking the stack of filter elements), a filter can determine 
 * if all its inputs will be present before it starts execution.  
 *
 * The symbol table could also be read by a higher level "workflow language"
 * to establish bindings within the language for a collection of filters.
 */

static int
_sym_check(xml_el_t e)
{
    return (!e || strcmp(xml_el_name(e), "sym")) ? 0 : 1;
}

static int
_sym_match(xml_el_t e, char *key)
{
    char *k;

    if (!_sym_check(e))
        return 0;
    if (xml_el_attr_val(e, "key", &k) != ISP_ESUCCESS)
        return 0;
    if (strcmp(k, key) == 0)
        return 1; /* match */
    return 0; /* no match */
}

static int
_sym_find(xml_el_t stab, char *key, isp_type_t *tp, int *fp)
{
    xml_el_t e;
    int res = ISP_ESUCCESS;

    if (!(e = xml_el_find_first(stab, (xml_el_match_t)_sym_match, key))) {
        res = ISP_ENOKEY;
        goto done;
    }
    if (tp) {
        res = xml_el_attr_scanval(e, 1, "type", "%d", tp);
        if (res != ISP_ESUCCESS)
            goto done;
    }
    if (fp) {
        res = xml_el_attr_scanval(e, 1, "flags", "%d", fp);
        if (res != ISP_ESUCCESS)
            goto done;
    }

done:
    return res;
}

static int
_stab_addsym(xml_el_t s, char *key, isp_type_t type, int flags)
{
    int res = ISP_ESUCCESS;
    xml_el_t e = NULL;

    if (_sym_find(s, key, NULL, NULL) == ISP_ESUCCESS)
        return ISP_EDUPKEY;
    if ((res = xml_el_create("sym", &e)) != ISP_ESUCCESS)
        goto done;
    if ((res = xml_attr_str_append(e, "key", key)) != ISP_ESUCCESS)
        goto done;
    if ((res = xml_attr_int_append(e, "type", type)) != ISP_ESUCCESS)
        goto done;
    if ((res = xml_attr_int_append(e, "flags", flags)) != ISP_ESUCCESS)
        goto done;
    if ((res = xml_el_append(s, e)) != ISP_ESUCCESS)
        goto done;

done:
    if (res != ISP_ESUCCESS) {
        if (e)
            xml_el_destroy(e);
    }
    return res;
}
    
static int
_stab_create(xml_el_t *ep, struct isp_stab_struct *tab)
{
    struct isp_stab_struct *tp = tab ? &tab[0] : NULL;
    xml_el_t e;
    int res;

    res = xml_el_create("stab", &e);
    while (res == ISP_ESUCCESS && tp && tp->name) {
        res = _stab_addsym(e, tp->name, tp->type, tp->flags);
        tp++;
    }
    if (res == ISP_ESUCCESS)
        *ep = e;
    return res;
}

static int
_stab_match(xml_el_t e, char *key)
{
    if (e && strcmp(xml_el_name(e), key) == 0)
        return 1; /* match */
    return 0; /* no match */
}

/* Find stab element inside filter element and return a pointer in sp.
 */
static int
_stab_find(isp_filter_t f, xml_el_t *sp)
{
    xml_el_t el;
    int res = ISP_ESUCCESS;
   
    el = xml_el_find_first(f, (xml_el_match_t)_stab_match, "stab");
    if (!el)
        res = ISP_ENOKEY;
    *sp = el;
    return res;
}

/* Verify that someone upstream ISP_PROVIDES the specified symbol,
 * and that nobody after that ISP_REMOVES it.
 * FIXME: also need to verify that symbols we ISP_PROVIDE are not provided
 * upstream, or are ISP_REMOVED before we see them.
 */
static int
_stab_verify_upstream(isp_init_t i, int fid, char *name, isp_type_t type)
{
    isp_filter_t f;
    xml_el_t stab;
    int res = ISP_EBIND;
    int id;
    isp_type_t ntype;
    int flags;

    /* Search backward thru filter id's numbered lower than us, until
     * we find our symbol being removed (immediate failure) or our symbol
     * being provided (immediate success).
     */
    for (id = fid - 1; id >= 0; id--) {
        res = isp_init_find(i, id, &f);     /* must find the filter id */
        if (res != ISP_ESUCCESS)
            break;
        res = _stab_find(f, &stab);         /* no stab, keep looking */
        if (res != ISP_ESUCCESS) {
            res = ISP_EBIND;
            continue; 
        }
        res = _sym_find(stab, name, &ntype, &flags);
        if (res != ISP_ESUCCESS) {          /* no symbol entry, keep looking */
            res = ISP_EBIND;
            continue;
        }
        if (ntype != type) {                /* right name, wrong type */
            res = ISP_EBIND;
            break;
        }
        if (flags & ISP_PROVIDES) {         /* provides: immediate success */
            res = ISP_ESUCCESS;
            break;
        }
        if (flags & ISP_REMOVES) {          /* removes: immediate failure */
            res = ISP_EBIND;
            break;
        }
    }

    return res;
}

static char *
_strtotype(isp_type_t t)
{
    char *str = "unknown type";

    switch (t) {
        case ISP_FILE:
            str = "file";
            break;
        case ISP_STR:
            str = "string";
            break;
        case ISP_DOUBLE:
            str = "double";
            break;
        case ISP_UINT64:
            str = "uint64";
            break;
        case ISP_INT64:
            str = "int64";
            break;
    }
    return str;
}

/* Verify that upstream ISP_PROVIDES all symbols that
 * we tag ISP_REQUIRES, and the types match.
 */
static int
_stab_verify(isp_init_t i, int fid, struct isp_stab_struct *tab)
{
    struct isp_stab_struct *tp;
    int res = ISP_ESUCCESS;

    for (tp = &tab[0]; tp->name != NULL; tp++) {
        if (tp->flags & ISP_REQUIRES) {
            int r = _stab_verify_upstream(i, fid, tp->name, tp->type);

            if (r != ISP_ESUCCESS) {
                res = r;
                isp_err("requires key ``%s'' (%s) not found upstream", 
                        tp->name, _strtotype(tp->type));
            }
        }
    }

    return res;
}

/**
 ** Filter functions
 **/

static int
_filter_check(xml_el_t e)
{
    return (!e || strcmp(xml_el_name(e), "filter")) ? 0 : 1;
}

PRIVATE int
isp_filter_fid_get(xml_el_t e, int *fidp)
{
    if (!fidp || !_filter_check(e))
        return ISP_EINVAL;
    return xml_el_attr_scanval(e, 1, "fid", "%d", fidp);
}

PRIVATE int
isp_filter_splitfactor_get(xml_el_t e, int *sfp)
{
    if (!sfp || !_filter_check(e))
        return ISP_EINVAL;
    return xml_el_attr_scanval(e, 1, "splitfactor", "%d", sfp);
}

/* helper for _filter_create */
static int
_array_create(xml_el_t *evp, char *evname, char *ename, int argc, char *argv[])
{
    int res = ISP_ESUCCESS;
    xml_el_t ev = NULL;
    xml_el_t e = NULL;
    int i;

    if (argv == NULL && argc > 0)
        return ISP_EINVAL;

    if ((res = xml_el_create(evname, &ev)) != ISP_ESUCCESS)
        goto done;
    for (i = 0; i < argc; i++) {
        if ((res = xml_el_create(ename, &e)) != ISP_ESUCCESS)
            goto done;
        if ((res = xml_el_append(ev, e)) != ISP_ESUCCESS) {
            xml_el_destroy(e);
            goto done;
        }
        if ((res = xml_attr_int_append(e, "key", i)) != ISP_ESUCCESS)
            goto done;
        if ((res = xml_attr_str_append(e, "val", argv[i])) != ISP_ESUCCESS)
            goto done;
    }
done:
    if (res == ISP_ESUCCESS)
        *evp = ev;
    else if (ev)
        xml_el_destroy(ev);
    return res;
}

static int 
_filter_create(xml_el_t *ep, int fid, struct isp_stab_struct stab[], 
                  int argc, char *argv[], int sf)
{
    int res;
    xml_el_t e;
    char cwd[MAXPATHLEN+1];
    xml_el_t tmp;

    if (!ep || fid < 0 || !argv || sf < 1)
        return ISP_EINVAL;
    if (getcwd(cwd, sizeof(cwd)) == NULL)
        return ISP_EGETCWD;

    if ((res = xml_el_create("filter", &e)) != ISP_ESUCCESS)
        goto error;
    if ((res = xml_attr_int_append(e, "fid", fid)) != ISP_ESUCCESS)
        goto error;
    if ((res = xml_attr_str_append(e, "cwd", cwd)) != ISP_ESUCCESS)
        goto error;
    if ((res = xml_attr_int_append(e, "uid", getuid())) != ISP_ESUCCESS)
        goto error;
    if ((res = xml_attr_int_append(e, "gid", getgid())) != ISP_ESUCCESS)
        goto error;
    if ((res = xml_attr_int_append(e, "proto", ISP_PROTO_VER)) != ISP_ESUCCESS)
        goto error;
    if ((res = xml_attr_int_append(e, "splitfactor", sf)) != ISP_ESUCCESS)
        goto error;

    if ((res = _array_create(&tmp, "argv", "arg", argc, argv)) != ISP_ESUCCESS)
            goto error;
    if ((res = xml_el_append(e, tmp)) != ISP_ESUCCESS) {
        xml_el_destroy(tmp);
        goto error;
    }

#if 0
    res = _array_create(&tmp, "envv", "env", util_argvlen(environ), environ);
    if (res != ISP_ESUCCESS)
        goto error;
    if ((res = xml_el_append(e, tmp)) != ISP_ESUCCESS) {
        xml_el_destroy(tmp);
        goto error;
    }
#endif

    if ((res = _stab_create(&tmp, stab)) != ISP_ESUCCESS) {
        isp_dbgfail("problem with symbol table");
        goto error;
    }
    if ((res = xml_el_append(e, tmp)) != ISP_ESUCCESS) {
        xml_el_destroy(tmp);
        goto error;
    }

    if (ep)
        *ep = e;
    return ISP_ESUCCESS;
error:
    if (e)
        xml_el_destroy(e);
    return res;
}

/**
 ** Filter functions
 **/

static int
_init_check(isp_init_t i)
{
    return (!i || strcmp(xml_el_name(i), "init")) ? 0 : 1;
}

PRIVATE int
isp_init_destroy(isp_init_t i)
{
    if (!_init_check(i))
        return ISP_EINVAL;
    xml_el_destroy(i);
    return ISP_ESUCCESS;
}

static int
_init_create(isp_init_t *ip)
{
    if (!ip)
        return ISP_EINVAL;
    return xml_el_create("init", ip);
}

PRIVATE int
isp_init_peek(isp_init_t i, isp_filter_t *fp)
{
    isp_filter_t f;

    if (!_init_check(i) || !fp)
        return ISP_EINVAL;
    if (!(f = xml_el_peek(i)) || !_filter_check(f)) /* stack top */
        return ISP_EELEMENT;
    *fp = f;
    return ISP_ESUCCESS;
}

static int
_filter_match(xml_el_t el, int *fidp)
{
    int fid;

    if (!_filter_check(el))
        return 0;
    if (xml_el_attr_scanval(el, 1, "fid", "%d", &fid) != ISP_ESUCCESS)
        return 0;
    assert(fidp != NULL);
    if (fid == *fidp)
        return 1; /* match */
    return 0; /* no match */
}

PRIVATE int
isp_init_find(isp_init_t i, int fid, isp_filter_t *fp)
{
    isp_filter_t f;

    if (!(f = xml_el_find_first(i, (xml_el_match_t)_filter_match, &fid)))
        return ISP_ENOKEY;
    *fp = f;

    return ISP_ESUCCESS;
}

static int 
_init_push(isp_init_t i, isp_filter_t f)
{
    if (!_init_check(i) || !_filter_check(f))
        return ISP_EINVAL;
    return xml_el_push(i, f);
}

PRIVATE int
isp_init_read(isp_handle_t h, isp_init_t *ip)
{
    isp_init_t i;
    int res;

    if (!ip)
        return ISP_EINVAL;
    if ((res = isp_handle_read(h, &i)) != ISP_ESUCCESS)
        return res;
    if (!_init_check(i))
        return ISP_EDOCUMENT; /* FIXME leaked an element */

    *ip = i;
    return ISP_ESUCCESS;
}

PRIVATE int
isp_init_write(isp_handle_t h, isp_init_t i)
{
    if (!_init_check(i))
        return ISP_EINVAL;
    return isp_handle_write(h, i);
}



PRIVATE int 
isp_init_handshake(isp_handle_t h, struct isp_stab_struct stab[], 
                   int argc, char *argv[], int sf)
{
    int res = ISP_ESUCCESS;
    isp_init_t i = NULL;
    isp_filter_t f;
    int fid = NO_FID;
    int flags;

    if ((res = isp_handle_flags_get(h, &flags)) != ISP_ESUCCESS)
        goto done;
    assert(!(flags & ISP_NONBLOCK));

    /* Read init element from stdin or create it from scratch.
     */
    if (flags & ISP_SINK) {
        isp_filter_t ftmp;

        if ((res = isp_init_read(h, &i)) != ISP_ESUCCESS)
            goto done;
        if ((res = isp_init_peek(i, &ftmp)) != ISP_ESUCCESS)
            goto done;
        if ((res = isp_filter_fid_get(ftmp, &fid)) != ISP_ESUCCESS)
            goto done; 
        fid++;
    } else {
        if ((res = _init_create(&i)) != ISP_ESUCCESS)    
            goto done;
        fid = 0;
    }
    isp_filterid_set(fid);

    /* Check upstream symbol tables against ours.
     */
    if (stab) {
        res = _stab_verify(i, fid, stab);
        if (res != ISP_ESUCCESS)
            goto done;
    }

    /* Push our filter element onto init element.
     * Init element is stored for future use.
     */
    res = _filter_create(&f, fid, stab, argc, argv, sf);
    if (res != ISP_ESUCCESS)
        goto done;
    if ((res = _init_push(i, f)) != ISP_ESUCCESS)
        goto done;
    isp_init_set(i);

    if (flags & ISP_SOURCE) {
        if ((res = isp_init_write(h, i)) != ISP_ESUCCESS)
            goto done;
    }
done:
    if (res != ISP_ESUCCESS && i)
        isp_init_destroy(i);

    return res;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
