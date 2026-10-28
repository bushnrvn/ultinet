#include <string.h>

#include "linestream.h"

#define LF 10                          /* numbers, not '\n': a C64 compiler may map characters to PETSCII */
#define CR 13

int ls_open(struct linestream *s, const char *host, unsigned port)
{
    int r;

    s->open = 0;
    s->len = 0;
    s->skip = 0;
    s->stat[0] = 0;
    if (!uci_present()) {
        return LS_NO_UCI;
    }
    r = net_connect(host, port, &s->sock, s->stat, sizeof(s->stat));
    if (r) {
        return r == 1 ? LS_TIMEOUT : LS_FAILED;
    }
    s->open = 1;
    return LS_OK;
}

int ls_poll(struct linestream *s, ls_line_fn fn, void *user)
{
    unsigned char buf[96];
    unsigned got, i;
    int r;

    if (!s->open) {
        return 1;
    }
    r = net_read(s->sock, buf, sizeof(buf), &got);
    if (r == 1) {
        net_close(s->sock);            /* the other end closed it: free the Ultimate's socket too */
        s->open = 0;
        return 1;
    }
    if (r) {
        return 0;                      /* no answer this time: try again */
    }
    for (i = 0; i < got; ++i) {
        if (buf[i] == LF) {
            if (!s->skip) {
                s->line[s->len] = 0;
                fn(s->line, user);
            }
            s->len = 0;
            s->skip = 0;
        } else if (buf[i] != CR && !s->skip) {
            if (s->len < LS_LINE_MAX - 1) {
                s->line[s->len++] = (char)buf[i];
            } else {
                s->skip = 1;           /* too long to be wanted: drop the whole line */
            }
        }
    }
    return 0;
}

int ls_send(struct linestream *s, const char *text)
{
    unsigned n = (unsigned)strlen(text), sent, off = 0;

    if (!s->open) {
        return 2;
    }
    while (off < n) {
        if (net_write(s->sock, (const unsigned char *)text + off, n - off, &sent) || sent == 0) {
            return 2;
        }
        off += sent;
    }
    return 0;
}

void ls_close(struct linestream *s)
{
    if (s->open) {
        net_close(s->sock);
        s->open = 0;
    }
}
