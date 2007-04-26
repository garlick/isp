#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <isp/util.h>
#include <isp/xml.h>
#include <isp/xout.h>
#include <isp/isp.h>

static void 
_errx(char *str, int val)
{
    fprintf(stderr, "srcxml: %s: %s\n", str, isp_errstr(val));
    exit(1);
}

static int 
_wait_for_io(xout_handle_t h)
{
    pfd_t pfd;
    int res;

    if ((res = util_pfd_create(&pfd)) == ISP_ESUCCESS) {
        util_pfd_zero(pfd);
        xout_prepoll(h, pfd);
        if ((res = util_poll(pfd, NULL)) == ISP_ESUCCESS)
            xout_postpoll(h, pfd);
        util_pfd_destroy(pfd);
    }

    return res;
}   

static int
_write_el_blocking(xout_handle_t xout, xml_el_t el)
{
    int res;
    int count = 0;

    while ((res = xout_write_el(xout, el)) == ISP_EWOULDBLOCK) {
        if ((res = _wait_for_io(xout)) != ISP_ESUCCESS)
            break;
        count++;
    }
    /* detect excessive spinning here.  We could have taken many trips through
     * _wait_for_io to read just one element, but we shouldn't have taken 
     * more trips than the element has characters...
     */
    if (res == ISP_ESUCCESS && el != NULL) {
        char *buf = NULL;
        int size = 0;

        if ((res = xml_el_to_str(el, &buf, &size)) != ISP_ESUCCESS)
            _errx("xml_el_to_str", res);
        free(buf);
        if (count > size) {
            fprintf(stderr, "srcxml: polled %d X to read %d char element\n",
                count, size);
            exit(1);
        }
    }
    return res;
}

/* Write an element with one attribute called "attr", value i
 * plus nattrs additional attributes named "test%d", %d:1...nattrs
 * Flush between writes so we should never see EWOULDBLOCK.
 */
static void
_src_element(xout_handle_t xout, unsigned long i, int nattrs)
{
    xml_el_t el;
    xml_attr_t at;
    int res;
    unsigned long j;
    char attrname[64];

    if ((res = xml_el_create("test", &el)) != ISP_ESUCCESS)
        _errx("xml_el_create", res);
    if ((res = xml_attr_create("attr", &at, "%lu", i)) != ISP_ESUCCESS)
        _errx("xml_attr_create", res);
    if ((res = xml_attr_append(el, at)) != ISP_ESUCCESS)
        _errx("xml_attr_append", res);
    for (j = 0; j < nattrs; j++) {
        sprintf(attrname, "test%lu", j);
        if ((res = xml_attr_create(attrname, &at, "%lu", j)) != ISP_ESUCCESS)
            _errx("xml_attr_create", res);
        if ((res = xml_attr_append(el, at)) != ISP_ESUCCESS)
            _errx("xml_attr_append", res);
    }

    if ((res = _write_el_blocking(xout, el)) != ISP_ESUCCESS)
        _errx("xout_write_el", res);

    xml_el_destroy(el);
}

int
main(int argc, char *argv[])
{
    xout_handle_t xout;
    int res;
    unsigned long i;
    unsigned long nelements, nattrs, backlog, sleepsec = 0;

    if (argc != 4 && argc != 5) {
        fprintf(stderr, "Usage: srcxml backlog nelements nattrs [sleepsec]\n");
        exit(1);
    }
    backlog = strtoul(argv[1], NULL, 10);
    nelements = strtoul(argv[2], NULL, 10);
    nattrs = strtoul(argv[3], NULL, 10);
    if (argc == 5)
        sleepsec = strtoul(argv[4], NULL, 10);

    if ((res = xout_handle_create(1, backlog, &xout)) != ISP_ESUCCESS)
        _errx("xout_handle_create", res);

    for (i = 0; i < nelements; i++)
        _src_element(xout, i, nattrs);

    if ((res = _write_el_blocking(xout, NULL)) != ISP_ESUCCESS)
        _errx("xout_write_el", res);

    /* After writing <document>, downstream will quit.
     * Give that time to happen then destroy the handle.  If anything
     * further gets written by xout_handle_destroy, we will see a SIGPIPE.
     */
    if (sleepsec > 0)
        sleep(sleepsec);

    if ((res = xout_handle_destroy(xout)) != ISP_ESUCCESS)
        _errx("xout_handle_destroy", res);
    exit(0);
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
