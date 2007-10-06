#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include <isp/util.h>
#include <isp/xml.h>
#include <isp/xin.h>
#include <isp/isp.h>

static void 
_errx(char *str, int val)
{
    fprintf(stderr, "sinkxml: %s: %s\n", str, isp_errstr(val));
    exit(1);
}

static int
_wait_for_io(xin_handle_t h)
{
    pfd_t pfd;
    int res;

    if ((res = util_pfd_create(&pfd)) == ISP_ESUCCESS) {
        util_pfd_zero(pfd);
        xin_prepoll(h, pfd);
        if ((res = util_poll(pfd, NULL)) == ISP_ESUCCESS)
            xin_postpoll(h, pfd);
        util_pfd_destroy(pfd);
    }
    return res;
}

static int 
_read_el_blocking(xin_handle_t xin, xml_el_t *el)
{
    int res;
    int count = 0;

    while ((res = xin_read_el(xin, el)) == ISP_EWOULDBLOCK) {
        if ((res = _wait_for_io(xin) != ISP_ESUCCESS))
            break;
        count++;
    }
    /* detect excessive spinning here.  We could have taken many trips through
     * _wait_for_io to read just one element, but we shouldn't have taken more
     * trips than the element has characters...
     */
    if (res == ISP_ESUCCESS) {
        char *buf;
        int size;

        if ((res = xml_el_to_str(*el, &buf, &size)) != ISP_ESUCCESS)
            _errx("xml_el_to_str", res);
        free(buf);
        if (count > size) {
            fprintf(stderr, "sinkxml: polled %d X to read %d char element\n",
                    count, size);
            exit(1);
        }
    }

    return res;
}

static void
_sink_element(xin_handle_t xin, unsigned long i, int nattrs)
{
    int res;
    unsigned long j, k;
    xml_el_t el;

    if ((res = _read_el_blocking(xin, &el)) != ISP_ESUCCESS)
        _errx("xin_read_el", res);

    if (strcmp(xml_el_name(el), "test") != 0) {
        fprintf(stderr, "sinkxml: el name wrong (want \"test\" got \"%s\")",
        xml_el_name(el));
        exit(1);
    }
    if ((res = xml_el_attr_scanval(el, 1, "attr", "%lu", &k)) != ISP_ESUCCESS)
        _errx("xml_el_attr_scanval", res);
    if (k != i) {
        fprintf(stderr, "sinkxml: attr value wrong (want %lu got %lu)\n", 
                i, k);
        exit(1);
    }
    for (j = 0; j < nattrs; j++) {
        char attrname[64];
        
        sprintf(attrname, "test%lu", j);
        res = xml_el_attr_scanval(el, 1, attrname, "%lu", &k);
        if (res != ISP_ESUCCESS)
            _errx("xml_el_attr_scanval", res);
        if (k != j) {
            fprintf(stderr, "sinkxml: attr value wrong (want %lu got %lu)\n", 
                    j, k);
            exit(1);
        }
    }
    xml_el_destroy(el);
}

int
main(int argc, char *argv[])
{
    xin_handle_t xin;
    int res;
    xml_el_t el;
    unsigned long i = 0;
    unsigned long backlog, nelements, nattrs, sleepsec = 0;

    if (argc != 4 && argc != 5) {
        fprintf(stderr, "Usage: sinkxml backlog nelements nattrs [sleepsec]\n");
        exit(1);
    }
    backlog = strtoul(argv[1], NULL, 10);
    nelements = strtoul(argv[2], NULL, 10);
    nattrs = strtoul(argv[3], NULL, 10);
    if (argc == 5)
        sleepsec = strtoul(argv[4], NULL, 10);

    if ((res = xin_handle_create(0, backlog, &xin)) != ISP_ESUCCESS)
        _errx("xin_handle_create", res);

    /* A delay here should cause write buffer upstream to fill up.
     */
    if (sleepsec > 0)
        sleep(sleepsec);

    for (i = 0; i < nelements; i++)
        _sink_element(xin, i, nattrs);

    if ((res = _read_el_blocking(xin, &el)) != ISP_EEOF) {
        if (res == ISP_ESUCCESS) {
            fprintf(stderr, "did not get EOF at the right place!\n");
            exit(1);
        } else
            _errx("xin_read_el", res);
    }

    if ((res = xin_handle_destroy(xin)) != ISP_ESUCCESS)
        _errx("xin_handle_destroy", res);

    exit(0);
}

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */
