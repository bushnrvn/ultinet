/* Read text lines from a TCP connection through the Ultimate's network target.
 * A stream holds one connection and the line being built, so several can be
 * open at once if the Ultimate allows it. Built on uci.h. */
#ifndef LINESTREAM_H
#define LINESTREAM_H

#include "uci.h"

/* The longest line kept; a longer one is dropped whole. */
#ifndef LS_LINE_MAX
#define LS_LINE_MAX 96
#endif

struct linestream {
    unsigned char sock;
    unsigned char open;
    unsigned char len;
    unsigned char skip;              /* inside a line that was too long: ignore it up to its end */
    char line[LS_LINE_MAX];
    char stat[24];                   /* what the Ultimate said about the last open */
};

/* Called for each complete line, without its line ending (LF, or CR LF). */
typedef void (*ls_line_fn)(const char *line, void *user);

/* Results of ls_open. */
#define LS_OK       0
#define LS_NO_UCI   1                /* the command interface does not answer: is it enabled? */
#define LS_FAILED   2                /* the Ultimate refused; the reason is in s->stat */
#define LS_TIMEOUT  3                /* no answer in time */

/* Connect to host (a dotted address or a name) on a TCP port. */
int ls_open(struct linestream *s, const char *host, unsigned port);

/* Collect what has arrived and call fn for each complete line. Call it often;
 * it returns at once if nothing has arrived. Returns 0 while the connection
 * is up, 1 once the other end has closed it (the socket is then freed). */
int ls_poll(struct linestream *s, ls_line_fn fn, void *user);

/* Send text. Returns 0 if all of it was sent. */
int ls_send(struct linestream *s, const char *text);

/* Close the connection (does nothing if it is already closed). */
void ls_close(struct linestream *s);

#endif
