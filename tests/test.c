/* Host test of src/uci.c and src/linestream.c against the simulated Ultimate. */
#include <stdio.h>
#include <string.h>

#include "linestream.h"
#include "uci.h"
#include "uci_sim.h"

static int failures;

#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); ++failures; } } while (0)

static char got_lines[8][LS_LINE_MAX];
static int n_lines;

static void on_line(const char *line, void *user)
{
    (void)user;
    if (n_lines < 8) {
        strcpy(got_lines[n_lines], line);
    }
    ++n_lines;
}

int main(void)
{
    unsigned char sock = 0, buf[300], resp[300];
    char stat[40], longline[200];
    unsigned got, rlen, i, sent;
    unsigned char cmd[8];
    struct linestream ls;

    sim_reset();
    CHECK(uci_present());
    sim_present = 0;
    CHECK(!uci_present());
    sim_present = 1;

    /* identify: a response and status of the right shape */
    cmd[0] = 3;
    cmd[1] = 1;
    CHECK(uci_exec(cmd, 2, resp, sizeof(resp), &rlen, stat, sizeof(stat), 50) == 0);
    CHECK(rlen == 34 && memcmp(resp, "ULTIMATE", 8) == 0);   /* 34 bytes: two blocks of response */
    CHECK(strcmp(stat, "00,OK") == 0);

    /* connecting */
    CHECK(net_connect("bad", 6464, &sock, stat, sizeof(stat)) == 2);
    CHECK(strcmp(stat, "84,UNRESOLVED HOST") == 0);
    CHECK(net_connect("192.168.0.7", 6464, &sock, stat, sizeof(stat)) == 0 && sock == 1);

    /* an unknown command is reported, not hung on */
    cmd[1] = 0x77;
    CHECK(uci_exec(cmd, 2, resp, sizeof(resp), &rlen, stat, sizeof(stat), 50) == 0);
    CHECK(strncmp(stat, "81", 2) == 0);

    /* reading: nothing yet, then some bytes */
    CHECK(net_read(sock, buf, 96, &got) == 0 && got == 0);
    sim_net_push("hello");
    CHECK(net_read(sock, buf, 96, &got) == 0 && got == 5 && memcmp(buf, "hello", 5) == 0);

    /* a big read crosses several response blocks */
    for (i = 0; i < 190; ++i) {
        buf[i] = (unsigned char)('a' + i % 26);
    }
    buf[190] = 0;
    sim_net_push((char *)buf);
    memset(buf, 0, sizeof(buf));
    CHECK(net_read(sock, buf, 190, &got) == 0 && got == 190);
    for (i = 0; i < 190; ++i) {
        CHECK(buf[i] == (unsigned char)('a' + i % 26));
    }

    /* writing */
    CHECK(net_write(sock, (const unsigned char *)"ping", 4, &sent) == 0 && sent == 4);
    CHECK(sim_net_outlen == 4 && memcmp(sim_net_out, "ping", 4) == 0);
    memset(buf, 'x', sizeof(buf));
    CHECK(net_write(sock, buf, 300, &sent) == 0 && sent == NET_WRITE_MAX);   /* cut to the largest write */

    /* a slow interface: many polls before data */
    sim_busy_polls = 500;
    sim_net_push("xyz");
    CHECK(net_read(sock, buf, 96, &got) == 0 && got == 3);
    sim_busy_polls = 3;

    /* a hung command times out and is aborted */
    sim_hang = 1;
    CHECK(net_read(sock, buf, 96, &got) == 2);
    CHECK(sim_aborts >= 1);
    sim_hang = 0;
    CHECK(net_read(sock, buf, 96, &got) == 0);        /* and the interface still works */
    net_close(sock);
    CHECK(!sim_socket_open);

    /* lines: split across reads, CR LF, several in one read */
    sim_reset();
    n_lines = 0;
    CHECK(ls_open(&ls, "192.168.0.7", 6464) == LS_OK);
    sim_net_push("one two\nthree\r\nfo");
    CHECK(ls_poll(&ls, on_line, 0) == 0);
    CHECK(n_lines == 2 && strcmp(got_lines[0], "one two") == 0 && strcmp(got_lines[1], "three") == 0);
    sim_net_push("ur\n");
    CHECK(ls_poll(&ls, on_line, 0) == 0);
    CHECK(n_lines == 3 && strcmp(got_lines[2], "four") == 0);
    CHECK(ls_poll(&ls, on_line, 0) == 0 && n_lines == 3);        /* nothing new: no lines */

    /* a line is kept until its end arrives */
    sim_net_push("par");
    CHECK(ls_poll(&ls, on_line, 0) == 0 && n_lines == 3);
    sim_net_push("tial\n");
    CHECK(ls_poll(&ls, on_line, 0) == 0 && n_lines == 4 && strcmp(got_lines[3], "partial") == 0);

    /* a line that is too long is dropped, and the next one is fine */
    memset(longline, 'z', sizeof(longline));
    longline[sizeof(longline) - 1] = 0;
    sim_net_push(longline);
    sim_net_push("\nok\n");
    for (i = 0; i < 5; ++i) {
        CHECK(ls_poll(&ls, on_line, 0) == 0);
    }
    CHECK(n_lines == 5 && strcmp(got_lines[4], "ok") == 0);        /* only "ok" came out */

    /* sending text */
    CHECK(ls_send(&ls, "hello\n") == 0);
    CHECK(sim_net_outlen == 6 && memcmp(sim_net_out, "hello\n", 6) == 0);

    /* the other end closes: the poll says so, the socket is freed, and later polls keep saying so */
    sim_net_closed = 1;
    CHECK(ls_poll(&ls, on_line, 0) == 1);
    CHECK(!sim_socket_open);
    CHECK(ls_poll(&ls, on_line, 0) == 1);
    CHECK(ls_send(&ls, "x") == 2);

    /* failures on opening */
    sim_reset();
    sim_present = 0;
    CHECK(ls_open(&ls, "192.168.0.7", 6464) == LS_NO_UCI);
    sim_present = 1;
    CHECK(ls_open(&ls, "bad", 6464) == LS_FAILED && strcmp(ls.stat, "84,UNRESOLVED HOST") == 0);
    sim_hang = 1;
    CHECK(ls_open(&ls, "192.168.0.7", 6464) == LS_TIMEOUT);
    sim_hang = 0;
    CHECK(ls_open(&ls, "192.168.0.7", 6464) == LS_OK);
    ls_close(&ls);
    CHECK(!sim_socket_open);

    if (failures) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("uci and linestream: all tests passed (%d commands)\n", sim_commands);
    return 0;
}
