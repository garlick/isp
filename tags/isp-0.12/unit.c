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

/* Functions that operate on work units and their subsidiary elements: 
 * metadata, files, and results.
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
#include <sys/times.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <assert.h>

#include "util.h"
#include "xml.h"
#include "isp.h"
#include "isp_private.h"
#include "macros.h"

static int _unit_check(isp_unit_t u);

/**
 ** Metadata functions
 **/

static int
_meta_check(xml_el_t e)
{
    return (!e || strcmp(xml_el_name(e), "meta")) ? 0 : 1;
}

static int 
_meta_create(xml_el_t *ep, char *key, isp_type_t type, char *value, 
             int src, int sink)
{
    int res; 
    xml_el_t e = NULL;

    if ((res = xml_el_create("meta", &e)) != ISP_ESUCCESS)
        goto error;
    if ((res = xml_attr_str_append(e, "key", key)) != ISP_ESUCCESS)
        goto error;
    if ((res = xml_attr_int_append(e, "type", type)) != ISP_ESUCCESS)
        goto error;
    if ((res = xml_attr_str_append(e, "val", value)) != ISP_ESUCCESS)
        goto error;
    if ((res = xml_attr_int_append(e, "src", src)) != ISP_ESUCCESS)
        goto error;
    if ((res = xml_attr_int_append(e, "sink", sink)) != ISP_ESUCCESS)
        goto error;

    if (ep)
        *ep = e;
    return ISP_ESUCCESS;
error:
    if (e)
        xml_el_destroy(e);
    return res;
}

static int
_datatostr(char **valp, isp_type_t type, va_list ap)
{
    char *val;
    int res = ISP_ESUCCESS;

    switch (type) {
        case ISP_FILE:
            res = ISP_EINVAL;
            break;
        case ISP_STR:
            if (asprintf(&val, "%s", va_arg(ap, char *)) == -1)
                res = ISP_ENOMEM;
            break;
        case ISP_DOUBLE:
            if (asprintf(&val, "%le", va_arg(ap, double)) == -1)
                res = ISP_ENOMEM;
            break;
        case ISP_UINT64:
            if (asprintf(&val, "%llu", va_arg(ap, unsigned long long)) == -1)
                res = ISP_ENOMEM;
            break;
        case ISP_INT64:
            if (asprintf(&val, "%lld", va_arg(ap, long long)) == -1)
                res = ISP_ENOMEM;
            break;
    }

    if (res == ISP_ESUCCESS)
        *valp = val;
    return res;
}

static int
_strtodata(char *val, isp_type_t type, va_list ap)
{
    int res = ISP_ESUCCESS;

    switch (type) {
        case ISP_FILE:
            res = ISP_EINVAL;
            break;
        case ISP_STR:
            {
                char **v = va_arg(ap, char **);
                char *tmp = strdup(val);

                if (!tmp)
                    res = ISP_ENOMEM;
                else
                    *v = tmp;
            }
            break;
        case ISP_DOUBLE:
            {
                double *v = va_arg(ap, double *);

                if (sscanf(val, "%le", v) != 1)
                    res = ISP_EATTR;
            }
            break;
        case ISP_UINT64:
            {
                unsigned long long *v = va_arg(ap, unsigned long long *);

                if (sscanf(val, "%llu", v) != 1)
                    res = ISP_EATTR;
            }
            break;
        case ISP_INT64:
            {
                long long *v = va_arg(ap, long long *);

                if (sscanf(val, "%lld", v) != 1)
                    res = ISP_EATTR;
            }
            break;
    }
    return res;
}

/* xml_el_match_t function (Ref: xml_el_find_first()).
 * Match "unsinked" metadata of specified key.
 */
static int 
_match_live_meta(xml_el_t e, char *key)
{
    int sink;
    char *k;

    if (!_meta_check(e)) 
        return 0;    
    if (xml_el_attr_scanval(e, 1, "sink", "%d", &sink) != ISP_ESUCCESS)
        return 0;
    if (sink != NO_FID)
        return 0;
    if (xml_el_attr_val(e, "key", &k) != ISP_ESUCCESS)
        return 0;
    if (strcmp(k, key) == 0)
        return 1; /* match */
    return 0; /* no match */
}

PUBLIC int
isp_meta_source(isp_unit_t u, char *key, isp_type_t type, ...)
{
    va_list ap;
    char *val = NULL;
    xml_el_t e;
    int res;
    int fid = isp_filterid_get();

    if (!u || !_unit_check(u) || !key)
        return ISP_EINVAL;
    if (xml_el_find_first(u, (xml_el_match_t)_match_live_meta, key))
        return ISP_EDUPKEY;

    va_start(ap, type);
    if ((res = _datatostr(&val, type, ap)) != ISP_ESUCCESS)
        goto done;
    va_end(ap);
    if ((res = _meta_create(&e, key, type, val, fid, NO_FID)) != ISP_ESUCCESS)
        goto done;
    res = xml_el_push(u, e);

done:
    if (val)
        free(val);
    return res;
}

PUBLIC int
isp_meta_set(isp_unit_t u, char *key, isp_type_t type, ...)
{
    xml_el_t e;
    int res;
    va_list ap;
    char *val = NULL;

    if (!u || !_unit_check(u) || !key)
        return ISP_EINVAL;
    if (!(e = xml_el_find_first(u, (xml_el_match_t)_match_live_meta, key)))
        return ISP_ENOKEY;

    /* FIXME: need to sink old value and source new one for provenance trail */
    va_start(ap, type);
    if ((res = _datatostr(&val, type, ap)) != ISP_ESUCCESS)
        goto done;
    va_end(ap);
    res = xml_el_attr_setval(e, "key", "%s", val); /* frees orig, copies new */

done:
    if (val)
        free(val);
    return res;
}

PUBLIC int
isp_meta_get(isp_unit_t u, char *key, isp_type_t type, ...)
{
    xml_el_t e;
    va_list ap;
    char *val;
    int res;

    if (!u || !_unit_check(u) || !key)
        return ISP_EINVAL;
    if (!(e = xml_el_find_first(u, (xml_el_match_t)_match_live_meta, key)))
        return ISP_ENOKEY;

    if ((res = xml_el_attr_val(e, "val", &val)) != ISP_ESUCCESS)
        return res;
    /* FIXME: check arg type against attr type */
    va_start(ap, type);
    res = _strtodata(val, type, ap);
    va_end(ap);

    return ISP_ESUCCESS;
}

PUBLIC int
isp_meta_sink(isp_unit_t u, char *key)
{
    int fid = isp_filterid_get();
    xml_el_t e;
    int res;

    if (!u || !_unit_check(u) || !key)
        return ISP_EINVAL;
    if (!(e = xml_el_find_first(u, (xml_el_match_t)_match_live_meta, key)))
        return ISP_ENOKEY;

    if ((res = xml_el_attr_setval(e, "sink", "%d", fid)) != ISP_ESUCCESS)
        return res;

    /* FIXME: error if sink is already -1 */

    return ISP_ESUCCESS;
}

static int
_meta_fini_one(xml_el_t e)
{
    int res;
    int src;

    /* unit created before isp_init?  Ours then. */
    if ((res = xml_el_attr_scanval(e, 1, "src", "%d", &src)) == ISP_ESUCCESS)
        if (src == NO_FID)
            res = xml_el_attr_setval(e, "src", "%d", isp_filterid_get());

    return res;
}

static int
_meta_fini(isp_unit_t u)
{
    int res = ISP_ESUCCESS;
    xml_el_iterator_t itr = NULL;
    xml_el_t e;

    if (!u || !_unit_check(u))
        return ISP_EINVAL;
    res = xml_el_iterator_create(u, &itr);
    while (res == ISP_ESUCCESS && (e = xml_el_next(itr)) != NULL) {
        if (_meta_check(e))
            res = _meta_fini_one(e);
    }
    if (itr)
        xml_el_iterator_destroy(itr);
    return res;
}

/**
 ** File functions
 **/

static int
_file_check(xml_el_t f)
{
    return (!f || strcmp(xml_el_name(f), "file")) ? 0 : 1;
}

/* Verify MD5 digest and size for file.
 */
static int 
_verify_file(xml_el_t f)
{
    char *path; 
    char *odigest; 
    char *digest = NULL;
    int res = ISP_ESUCCESS;
    struct stat sb;
    unsigned long size;

    if ((res = xml_el_attr_val(f, "path", &path)) != ISP_ESUCCESS)
        goto done;

    /* First check that the file size matches.
     */
    if (stat(path, &sb) < 0) {
        res = ISP_ENOENT;
        goto done;
    }
    if ((res = xml_el_attr_scanval(f, 1, "size", "%lu", &size)) != ISP_ESUCCESS)
        goto done;
    if (size != sb.st_size) {
        res = ISP_ECORRUPT;
        goto done;
    }

    /* Next check that MD5 digest matches (if configured and present).
     */
    if ((res = xml_el_attr_val(f, "md5", &odigest)) != ISP_ESUCCESS)
        goto done; /* attr should always be there - not necessarily filled in */
    if (strlen(odigest) == 0)
        goto done; /* the not filled in case (no error) */
#if HAVE_OPENSSL
    if (isp_md5check_get()) {
        if ((res = util_md5_digest(path, &digest)) != ISP_ESUCCESS)
            goto done;
        if (strcmp(digest, odigest) != 0) {
            res = ISP_ECORRUPT;
            goto done;
        }
    }
#endif

done: 
    if (digest)
        free(digest);
    return res;
}

static int 
_file_create(xml_el_t *fp, char *key, char *path, char *host, 
             int src, int sink, int flags)
{
    int res;
    xml_el_t e = NULL;

    /* md5 and size will be filled in later (_file_fini) */
   
    if ((res = xml_el_create("file", &e)) != ISP_ESUCCESS) 
        goto error;
    if ((res = xml_attr_str_append(e, "key", key)) != ISP_ESUCCESS)
        goto error;
    if ((res = xml_attr_str_append(e, "path", path)) != ISP_ESUCCESS)
        goto error;
    if ((res = xml_attr_str_append(e, "host", host)) != ISP_ESUCCESS)
        goto error;
    if ((res = xml_attr_ulong_append(e, "size", 0)) != ISP_ESUCCESS)
        goto error;
    if ((res = xml_attr_int_append(e, "src", src)) != ISP_ESUCCESS)
        goto error;
    if ((res = xml_attr_int_append(e, "sink", sink)) != ISP_ESUCCESS)
        goto error;
    if ((res = xml_attr_int_append(e, "flags", flags)) != ISP_ESUCCESS)
        goto error;
    if ((res = xml_attr_str_append(e, "md5", "")) != ISP_ESUCCESS)
        goto error;

    if (fp)
        *fp = e;
    return ISP_ESUCCESS;
error:
    if (e)
        xml_el_destroy(e);
    return res;
}

/* xml_el_match_t function (Ref: xml_el_find_first()).
 * Match "unsinked" file of specified key.
 */
static int 
_match_live_file(xml_el_t el, char *key)
{
    int  sink;
    char *k = NULL;

    if (!_file_check(el))
        return 0;
    if (xml_el_attr_scanval(el, 1, "sink", "%d", &sink) != ISP_ESUCCESS)
        return 0;
    if (sink != NO_FID)
        return 0;
    if (xml_el_attr_val(el, "key", &k) != ISP_ESUCCESS)
        return 0;
    if (strcmp(k, key) == 0)
        return 1; /* match */
    return 0; /* no match */
}

static int 
_match_live_rwfile(xml_el_t el, int *modemaskp)
{
    int sink, mode;

    if (!_file_check(el))
        return 0;
    if (xml_el_attr_scanval(el, 1, "sink", "%d", &sink) != ISP_ESUCCESS)
        return 0;
    if (sink != NO_FID)
        return 0;
    if (xml_el_attr_scanval(el, 1, "mode", "%d", &mode) != ISP_ESUCCESS)
        return 0;
    assert(modemaskp != NULL);
    if (mode & *modemaskp)
        return 1; /* match */
    return 0; /* no match */
}

/* return an error if unit contains any unsinked read-write files
 */
PUBLIC int
isp_rwfile_check(isp_unit_t u)
{
    int res = ISP_ESUCCESS;
    int modemask = ISP_RDWR;

    if (!u || !_unit_check(u))
        return ISP_EINVAL;
    if (xml_el_find_first(u, (xml_el_match_t)_match_live_rwfile, &modemask))
        res = ISP_ERWFILE;

    return res;
}

/* Make a copy of path, ensuring it is fully qualified.
 * Caller must free() the result.
 */
static int
_qualify_path(char *path, char **fqpath)
{
    char *s;
    int res = ISP_ESUCCESS;
    char cwd[MAXPATHLEN+1];

    if (*path == '/') {
        if (!(s = strdup(path)))
            res = ISP_ENOMEM;
    } else {
        if (getcwd(cwd, sizeof(cwd)) == NULL)
            res = ISP_EGETCWD;
        else
            if (asprintf(&s, "%s/%s", cwd, path) < 0)
                res = ISP_ENOMEM;
    }

    assert(fqpath);
    if (fqpath && res == ISP_ESUCCESS)
        *fqpath = s;
    return res;
}

/* Add file to a unit.
 */
PUBLIC int
isp_file_source(isp_unit_t u, char *key, char *path, int flags)
{
    char *npath = NULL;
    xml_el_t f;
    int res;
    struct stat sb;

    if (!u || !_unit_check(u) || !key || !path)
        return ISP_EINVAL;
    if (xml_el_find_first(u, (xml_el_match_t)_match_live_file, key))
        return ISP_EDUPKEY;

    if ((res = _qualify_path(path, &npath)) != ISP_ESUCCESS)
        goto done;
    if (stat(npath, &sb) < 0) {
        res = ISP_ENOENT;
        goto done;
    }
    if ((res = _file_create(&f, key, npath, isp_hostname_get(),
                    isp_filterid_get(), NO_FID, flags)) != ISP_ESUCCESS)
        goto done;
    if ((res = xml_el_push(u, f)) != ISP_ESUCCESS) {
        xml_el_destroy(f);
        goto done;
    }

done:
    if (npath)
        free(npath);
    return res;
}

PUBLIC int
isp_file_access(isp_unit_t u, char *key, char **pathp, int flags)
{
    xml_el_t f;
    char *path = NULL;
    char *npath = NULL;
    int res = ISP_ESUCCESS;

    if (!u || !_unit_check(u) || !key || !pathp)
        return ISP_EINVAL;

    if (!(f = xml_el_find_first(u, (xml_el_match_t)_match_live_file, key))) {
        res = ISP_ENOKEY;
        goto error;
    }
    if ((res = _verify_file(f)) != ISP_ESUCCESS)
        goto error;
    if ((res = xml_el_attr_val(f, "path", &path)) != ISP_ESUCCESS)
        goto error;

    /* If filter will update the file, we sink the old file reference and
     * source a new one.  If the file is read-only, we create a read-write
     * copy in the new file reference.
     */
    if (!(flags & ISP_RDONLY)) {
        int fileflags;
        xml_el_t fnew;
        struct stat sb;

        res = xml_el_attr_scanval(f, 1, "flags", "%d", &fileflags);
        if (res != ISP_ESUCCESS)
            goto error;
        if ((fileflags & ISP_RDONLY)) {
            /* The file cannot be modified in place.
             * Source a new file element with copy of file.
             */
            res = util_mktmp_copy(path, NULL, &npath);
            if (res != ISP_ESUCCESS)
                goto error;
            if (stat(npath, &sb) < 0) {
                res = ISP_ENOENT;
                goto error;
            }
            res = _file_create(&fnew, key, npath, isp_hostname_get(),
                    isp_filterid_get(), NO_FID, ISP_RDWR);
            if (res != ISP_ESUCCESS)
                goto error;
            res = xml_el_attr_val(fnew, "path", &path);
            if (res != ISP_ESUCCESS)
                goto error;
            free(npath);
        } else {
            /* The file can be modified in place.
             * Source a new file element with the same path.
             */
            if (stat(path, &sb) < 0) {
                res = ISP_ENOENT;
                goto error;
            }
            res = _file_create(&fnew, key, path, isp_hostname_get(),
                    isp_filterid_get(), NO_FID, ISP_RDWR);
            if (res != ISP_ESUCCESS)
                goto error;
        }

        res = xml_el_attr_setval(f, "sink", "%d", isp_filterid_get());
        if (res != ISP_ESUCCESS) {
            xml_el_destroy(fnew);
            goto error;
        }
        res = xml_el_push(u, fnew);
        if (res != ISP_ESUCCESS) {
            xml_el_destroy(fnew);
            (void)xml_el_attr_setval(f, "sink", "%d", NO_FID); /* unsink */
            goto error;
        }
    }

    if (pathp)
        *pathp = path;
    return res;
error:
    if (npath) {
        (void)unlink(npath);
        free(npath);
    }
    return res;
}

PUBLIC int
isp_file_rename(isp_unit_t u, char *key, char *npath)
{
    xml_el_t f, fnew;
    char *path = NULL;
    int flags;
    int res = ISP_ESUCCESS;
    struct stat sb;

    if (!u || !_unit_check(u) || !key || !npath)
        return ISP_EINVAL;

    if (!(f = xml_el_find_first(u, (xml_el_match_t)_match_live_file, key))) {
        res = ISP_ENOKEY;
        goto done;
    }
    if ((res = _verify_file(f)) != ISP_ESUCCESS)
        goto done;
    if ((res = xml_el_attr_val(f, "path", &path)) != ISP_ESUCCESS)
        goto done;
    if ((res = xml_el_attr_scanval(f, 1, "flags", "%d", &flags)) != ISP_ESUCCESS)
        goto done;
    if ((flags & ISP_RDONLY)) {
        if ((res = util_mkcopy(path, npath)) != ISP_ESUCCESS)
            goto done;
    } else {
        if (rename(path, npath) < 0) {
            res = ISP_ERENAME;
            goto done;
        }
    }
    if (stat(npath, &sb) < 0) {
        res = ISP_ENOENT;
        goto done;
    }
    res = _file_create(&fnew, key, npath, isp_hostname_get(),
            isp_filterid_get(), NO_FID, ISP_RDWR);
    if (res != ISP_ESUCCESS)
        goto error;
    res = xml_el_attr_setval(f, "sink", "%d", isp_filterid_get());
    if (res != ISP_ESUCCESS) {
        xml_el_destroy(fnew);
        goto error;
    }
    res = xml_el_push(u, fnew);
    if (res != ISP_ESUCCESS) {
        xml_el_destroy(fnew);
        (void)xml_el_attr_setval(f, "sink", "%d", NO_FID); /* unsink */
        goto error;
    }

done:
    return res;
error:
    if (flags & ISP_RDONLY)
        (void)unlink(npath);
    else
        (void)rename(npath, path);
    return res;

}

PUBLIC int
isp_file_sink(isp_unit_t u, char *key)
{
    xml_el_t f;
    int res;
    int flags;
    char *path;

    if (!u || !_unit_check(u) || !key)
        return ISP_EINVAL;

    if (!(f = xml_el_find_first(u, (xml_el_match_t)_match_live_file, key))) {
        res = ISP_ENOKEY;
        goto done;
    }
    res = xml_el_attr_scanval(f, 1, "flags", "%d", &flags);
    if (res != ISP_ESUCCESS)
        goto done;
    if ((res = xml_el_attr_val(f, "path", &path)) != ISP_ESUCCESS)
        goto done;
    res = xml_el_attr_setval(f, "sink", "%d", isp_filterid_get());
    if (res != ISP_ESUCCESS)
        goto done;
    if (!(flags & ISP_RDONLY))
        (void)unlink(path);  /* best effort only due to isp_unit_copy() */
done:
    return res;
}

/* helper for _file_fini() - finalize just one file */
static int
_file_fini_one(xml_el_t f)
{
    int res;
    int src, sink;
    char *path;

    /* if file has been consumed, we don't care about it */
    if ((res = xml_el_attr_scanval(f, 1, "sink", "%d", &sink)) != ISP_ESUCCESS) 
        return res;
    if (sink != NO_FID)
        return ISP_ESUCCESS;


    /* unit created before isp_init?  Ours then. */
    if ((res = xml_el_attr_scanval(f, 1, "src", "%d", &src)) != ISP_ESUCCESS) 
        return res;
    if (src == NO_FID) {
        res = xml_el_attr_setval(f, "src", "%d", isp_filterid_get());
        if (res != ISP_ESUCCESS)
            return res;
        src = isp_filterid_get();
    }

    /* if not our unit, we do nothing further */
    if (src != isp_filterid_get())
        return ISP_ESUCCESS;

    /* now we look at the file */
    if ((res = xml_el_attr_val(f, "path", &path)) != ISP_ESUCCESS)
        return res;

    /* update the size - initially zero */
    {
        struct stat sb;
        char *key = "<unknown>";

        if (stat(path, &sb) < 0) {
            xml_el_attr_val(f, "key", &key);
            isp_dbgfail("stat key=%s path=%s: %m", key, path);
            free(key);
            return ISP_ENOENT;
        }
        res = xml_el_attr_setval(f, "size", "%lu", sb.st_size);
        if (res != ISP_ESUCCESS)
            return res;
    }

    /* update the md5 digest - initially empty string */
#if HAVE_OPENSSL
    if (isp_md5check_get()) {
        char *digest;

        if ((res = util_md5_digest(path, &digest)) != ISP_ESUCCESS)
            return res;
        res = xml_el_attr_setval(f, "md5", "%s", digest);
        free(digest); /* xml made a copy */
        if (res != ISP_ESUCCESS)
            return res;;
    }
#endif
    return ISP_ESUCCESS;
}

static int
_file_fini(isp_unit_t u)
{
    int res = ISP_ESUCCESS;
    xml_el_iterator_t itr = NULL;
    xml_el_t el;

    if (!u || !_unit_check(u))
        return ISP_EINVAL;

    res = xml_el_iterator_create(u, &itr);
    while (res == ISP_ESUCCESS && (el = xml_el_next(itr)) != NULL)
        if (_file_check(el))
            res = _file_fini_one(el);
    if (itr)
        xml_el_iterator_destroy(itr);
    return res;
}

/**
 ** Result functions
 **/

static int
_result_check(xml_el_t e)
{
    return (!e || strcmp(xml_el_name(e), "result")) ? 0 : 1;
}

static int 
_result_create(xml_el_t *ep, int fid, int code,
               unsigned long ut, unsigned long st, unsigned long rt)
{
    int res;
    xml_el_t e = NULL; 

    if (!ep)
        return ISP_EINVAL;

    if ((res = xml_el_create("result", &e)) != ISP_ESUCCESS)
        goto error;
    if ((res = xml_attr_int_append(e, "fid", fid)) != ISP_ESUCCESS)
        goto error;
    if ((res = xml_attr_ulong_append(e, "utime", ut)) != ISP_ESUCCESS)
        goto error;
    if ((res = xml_attr_ulong_append(e, "stime", st)) != ISP_ESUCCESS)
        goto error;
    if ((res = xml_attr_ulong_append(e, "rtime", rt)) != ISP_ESUCCESS)
        goto error;
    if ((res = xml_attr_int_append(e, "code", code)) != ISP_ESUCCESS)
        goto error;
    
    *ep = e;
    return ISP_ESUCCESS;
error:
    if (e)
        xml_el_destroy(e);
    return res;
}

/* Create 'result' with initial time values.  
 * The numbers is invalid until we run _result_fini(u).
 */
static int 
_result_init(isp_unit_t u)
{
    struct tms p;
    struct timeval t;
    int res = ISP_ESUCCESS;
    xml_el_t e;
    long tps; 

    if (!u || !_unit_check(u))
        return ISP_EINVAL;
    if (isp_filterid_get() == NO_FID)
        return ISP_ENOINIT;

    tps = sysconf(_SC_CLK_TCK);
    if (tps < 0 || times(&p) < 0 || gettimeofday(&t, NULL) < 0) {
        res = ISP_ETIME;
        goto done;
    }
    if ((res = _result_create(&e, isp_filterid_get(), ISP_ENOTRUN,
            1000L * p.tms_cutime / tps,
            1000L * p.tms_cstime / tps, 
            t.tv_usec/1000 + t.tv_sec*1000)) != ISP_ESUCCESS)
        goto done;

    if ((res = xml_el_push(u, e)) != ISP_ESUCCESS)
        goto done;

done:
    return res;
}

/* xml_el_match_t function (Ref: xml_el_find_first()).
 * Match result element of specified fid.
 */
static int 
_result_match(xml_el_t e, int *fidp)
{
    int fid;

    if (!_result_check(e))
        return 0;
    if (xml_el_attr_scanval(e, 1, "fid", "%d", &fid) != ISP_ESUCCESS)
        return 0;
    assert(fidp != NULL);
    if (fid == *fidp)
        return 1; /* match */
    return 0; /* no match */
}

/* Update the initial 'result' with the final thing.
 */
static int
_result_fini(isp_unit_t u, int result)
{
    struct tms p;
    struct timeval t;
    long tps = sysconf(_SC_CLK_TCK);
    unsigned long st, ut, rt;
    unsigned long nst, nut, nrt;
    int res;
    xml_el_t e;
    int fid = isp_filterid_get();

    if (!(e = xml_el_find_first(u, (xml_el_match_t)_result_match, &fid)))
        return ISP_EELEMENT;
    if (tps < 0 || times(&p) < 0 || gettimeofday(&t, NULL) < 0)
        return ISP_ETIME;
    if ((res = xml_el_attr_scanval(e, 1, "utime", "%lu", &ut)) != ISP_ESUCCESS)
        return res;
    if ((res = xml_el_attr_scanval(e, 1, "stime", "%lu", &st)) != ISP_ESUCCESS)
        return res;
    if ((res = xml_el_attr_scanval(e, 1, "rtime", "%lu", &rt)) != ISP_ESUCCESS)
        return res;

    nut = 1000L * p.tms_cutime / tps - ut;
    nst = 1000L * p.tms_cstime / tps - st;
    nrt = t.tv_usec/1000 + t.tv_sec*1000 - rt;

    if ((res = xml_el_attr_setval(e, "utime", "%lu", nut)) != ISP_ESUCCESS)
        return res;
    if ((res = xml_el_attr_setval(e, "stime", "%lu", nst)) != ISP_ESUCCESS)
        return res;
    if ((res = xml_el_attr_setval(e, "rtime", "%lu", nrt)) != ISP_ESUCCESS)
        return res;
    if ((res = xml_el_attr_setval(e, "code", "%d", result)) != ISP_ESUCCESS)
        return res;

    return ISP_ESUCCESS;
}

/* Get the "upstream" computation result for this unit so we can decide
 * whether to process it.  
 */
PUBLIC int 
isp_result_upstream_get(isp_unit_t u, int *codep)
{
    xml_el_iterator_t itr; 
    xml_el_t e;
    int code = ISP_ESUCCESS;
    int res = ISP_ESUCCESS;
    int fid = isp_filterid_get();

    if (!codep)
        return ISP_EINVAL;

    if ((res = xml_el_iterator_create(u, &itr)) == ISP_ESUCCESS) {
        while ((e = xml_el_next(itr)) != NULL) {
            if (_result_check(e)) {
                int r, id;

                res = xml_el_attr_scanval(e, 1, "fid", "%d", &id);
                if (res != ISP_ESUCCESS)
                    break;
                res = xml_el_attr_scanval(e, 1, "code", "%d", &r);
                if (res != ISP_ESUCCESS)
                    break;
                if (id < fid && r != ISP_ESUCCESS) { /* find the "earliest" */
                    code = r;
                    fid = id;
                }
            }
        }
        xml_el_iterator_destroy(itr);
    }

    if (res == ISP_ESUCCESS)
        *codep = code;
    return res;
}

static int
_result_find(isp_unit_t u, xml_el_t *ep, int fid)
{
    xml_el_t e;

    if (!(e = xml_el_find_first(u, (xml_el_match_t)_result_match, &fid)))
        return ISP_ENOKEY;
    *ep = e;

    return ISP_ESUCCESS;
}

PRIVATE int
isp_result_get(isp_unit_t u, int fid, unsigned long *utimep, 
               unsigned long *stimep, unsigned long *rtimep, 
               int *codep)
{
    xml_el_t e; 
    int res;

    if (!_unit_check(u) || fid > isp_filterid_get() || fid < 0)
        return ISP_EINVAL;
    if ((res = _result_find(u, &e, fid)) != ISP_ESUCCESS)
        return res;

    if (utimep) {
        res = xml_el_attr_scanval(e, 1, "utime", "%lu", utimep);
        if (res != ISP_ESUCCESS)
            return res;
    }
    if (stimep) {
        res = xml_el_attr_scanval(e, 1, "stime", "%lu", stimep);
        if (res != ISP_ESUCCESS)
            return res;
    }
    if (rtimep) {
        res = xml_el_attr_scanval(e, 1, "rtime", "%lu", rtimep);
        if (res != ISP_ESUCCESS)
            return res;
    }
    if (codep != NULL) {
        res = xml_el_attr_scanval(e, 1, "code", "%d", codep);
        if (res != ISP_ESUCCESS)
            return res;
    }
    return ISP_ESUCCESS;
}

/**
 ** Unit functions
 **/

static int
_unit_check(isp_unit_t u)
{
    return (u && strcmp(xml_el_name(u), "unit")) ? 0 : 1;
}

PUBLIC int
isp_unit_copy(isp_unit_t *up, isp_unit_t u)
{
    if (!u || !up || !_unit_check(u))
        return ISP_EINVAL;
    return xml_el_copy(up, u);
}

PUBLIC int
isp_unit_destroy(isp_unit_t u)
{
    if (!u || !_unit_check(u))
        return ISP_EINVAL;
    xml_el_destroy(u);
    return ISP_ESUCCESS;
}

PUBLIC int
isp_unit_create(isp_unit_t *up)
{
    if (up == NULL)
        return ISP_EINVAL;
    return xml_el_create("unit", up);
}

PUBLIC int
isp_unit_init(isp_unit_t u)
{
    if (!u)
        return ISP_EINVAL;
    return _result_init(u);
}

PUBLIC int
isp_unit_fini(isp_unit_t u, int result)
{
    int res;

    if (!u || !_unit_check(u) || result < 0)
        return ISP_EINVAL;
    if ((res = _file_fini(u)) != ISP_ESUCCESS)
        return res;
    if ((res = _meta_fini(u)) != ISP_ESUCCESS)
        return res;
    if ((res = _result_fini(u, result)) != ISP_ESUCCESS)
        return res;
    return ISP_ESUCCESS;
}

PUBLIC int
isp_unit_read(isp_handle_t h, isp_unit_t *up)
{
    isp_unit_t u;
    int res;

    if (!up)
        return ISP_EINVAL;
    if ((res = isp_handle_read(h, &u)) != ISP_ESUCCESS)
        return res;
    if (!_unit_check(u))
        return ISP_EDOCUMENT; /* XXX leaked an element */

    *up = u;
    return ISP_ESUCCESS;
}

PUBLIC int
isp_unit_write(isp_handle_t h, isp_unit_t u)
{
    /* allow NULL */
    if (!_unit_check(u))
        return ISP_EINVAL;
    return isp_handle_write(h, u);
}

/* Loop: read a work unit, call map function, write modified work unit.
 * Runs until input is exhausted and computation is complete.
 */
PUBLIC int
isp_unit_map(isp_handle_t h, isp_mapfun_t mapfun, void *arg)
{
    isp_unit_t u;
    int res = ISP_ESUCCESS;
    int flags;

    if ((res = isp_handle_flags_get(h, &flags)) != ISP_ESUCCESS)
        return res;

    while ((res = isp_unit_read(h, &u)) == ISP_ESUCCESS) {
        int mapres = ISP_ESUCCESS;
        int oldres;
       
        if ((res = isp_result_upstream_get(u, &oldres)) != ISP_ESUCCESS)
            break;

        if ((res = isp_unit_init(u)) != ISP_ESUCCESS)
            break;
        if ((flags & ISP_IGNERR) || oldres == ISP_ESUCCESS) {
            if (mapfun != NULL)
                mapres = mapfun(u, arg);
        } else
            mapres = ISP_ENOTRUN;
        if ((res = isp_unit_fini(u, mapres)) != ISP_ESUCCESS)
            break;
        if ((res = isp_unit_write(h, u)) != ISP_ESUCCESS)
            break;
        if ((res = isp_unit_destroy(u)) != ISP_ESUCCESS)
            break;
    }

    if (res == ISP_EEOF)   /* this is expected at the end */
        res = ISP_ESUCCESS;
    if (res == ISP_ESUCCESS)
        res = isp_unit_write(h, NULL);

    return res;
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
