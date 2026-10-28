#include "uci.h"

#ifdef __CC65__
#include <time.h>

#define UCI_CONTROL(v) (*(volatile unsigned char *)0xDF1C = (v))
#define UCI_STATUS()   (*(volatile unsigned char *)0xDF1C)
#define UCI_CMDBYTE(v) (*(volatile unsigned char *)0xDF1D = (v))
#define UCI_ID()       (*(volatile unsigned char *)0xDF1D)
#define UCI_RESP()     (*(volatile unsigned char *)0xDF1E)
#define UCI_STATB()    (*(volatile unsigned char *)0xDF1F)
#else
/* host simulation: tests/uci_sim.c */
extern void uci_sim_control(unsigned char v);
extern unsigned char uci_sim_status(void);
extern void uci_sim_cmdbyte(unsigned char v);
extern unsigned char uci_sim_id(void);
extern unsigned char uci_sim_resp(void);
extern unsigned char uci_sim_statb(void);
#define UCI_CONTROL(v) uci_sim_control(v)
#define UCI_STATUS()   uci_sim_status()
#define UCI_CMDBYTE(v) uci_sim_cmdbyte(v)
#define UCI_ID()       uci_sim_id()
#define UCI_RESP()     uci_sim_resp()
#define UCI_STATB()    uci_sim_statb()
#endif

/* control register bits */
#define C_PUSH 0x01
#define C_ACC  0x02
#define C_ABORT 0x04
#define C_CLRERR 0x08
/* status register bits */
#define S_STATE 0x30                  /* 00 idle, 01 command busy, 10 data last, 11 data more */
#define S_LAST  0x20
#define S_MORE  0x30
#define S_ERROR 0x08
#define S_STAT_AV 0x40
#define S_DATA_AV 0x80

/* A timeout: counted in clock ticks on the C64, in polls on the host. */
#ifdef __CC65__
typedef clock_t stamp_t;
#define STAMP_NOW() clock()
#define TIMED_OUT(t0, ticks) (clock() - (t0) > (clock_t)(ticks))
#else
typedef unsigned long stamp_t;
static unsigned long polls;
#define STAMP_NOW() (polls = 0)
#define TIMED_OUT(t0, ticks) ((void)(t0), (void)(ticks), ++polls > 200000UL)
#endif

int uci_present(void)
{
    unsigned char id = UCI_ID();

    return id == 0xC9 || id == 0x49;
}

static void abort_and_clear(void)
{
    stamp_t t0 = STAMP_NOW();

    UCI_CONTROL(C_ABORT);
    UCI_CONTROL(C_ACC);
    while ((UCI_STATUS() & S_STATE) != 0 && !TIMED_OUT(t0, 25)) {
    }
    if (UCI_STATUS() & S_ERROR) {
        UCI_CONTROL(C_CLRERR);
    }
}

int uci_exec(const unsigned char *cmd, unsigned n, unsigned char *resp, unsigned rmax,
             unsigned *rlen, char *stat, unsigned smax, unsigned timeout)
{
    stamp_t t0 = STAMP_NOW();
    unsigned i, si = 0;
    unsigned char st, b;
    int more;

    *rlen = 0;
    if (smax) {
        stat[0] = 0;
    }
    /* wait until the interface is idle */
    while ((UCI_STATUS() & S_STATE) != 0) {
        if (UCI_STATUS() & S_ERROR) {
            UCI_CONTROL(C_CLRERR);
        }
        if (TIMED_OUT(t0, timeout)) {
            abort_and_clear();
            return 1;
        }
    }
    for (i = 0; i < n; ++i) {
        UCI_CMDBYTE(cmd[i]);
    }
    UCI_CONTROL(C_PUSH);

    do {
        /* wait for a block of response: state "data last" or "data more" */
        while (((st = UCI_STATUS()) & S_STATE) < S_LAST) {
            if (TIMED_OUT(t0, timeout)) {
                abort_and_clear();
                return 1;
            }
        }
        while (UCI_STATUS() & S_DATA_AV) {
            b = UCI_RESP();
            if (*rlen < rmax) {
                resp[(*rlen)++] = b;
            }
        }
        while (UCI_STATUS() & S_STAT_AV) {
            b = UCI_STATB();
            if (si + 1 < smax) {
                stat[si++] = (char)b;
            }
        }
        more = (st & S_STATE) == S_MORE;
        UCI_CONTROL(C_ACC);
    } while (more);
    if (smax) {
        stat[si] = 0;
    }
    /* the command is finished when the interface is idle again */
    while ((UCI_STATUS() & S_STATE) != 0) {
        if (TIMED_OUT(t0, timeout)) {
            abort_and_clear();
            return 1;
        }
    }
    return 0;
}

/* status texts begin with two digits: "00,OK" is success. The digits are
 * ASCII numbers, not character constants (the C64 compiler would translate). */
#define STATUS_IS(s, d1, d2) ((unsigned char)(s)[0] == (d1) && (unsigned char)(s)[1] == (d2))

int net_connect(const char *host, unsigned port, unsigned char *sock, char *stat, unsigned smax)
{
    unsigned char cmd[64], resp[4];
    unsigned n = 0, rlen;

    cmd[n++] = 3;                      /* network target */
    cmd[n++] = 7;                      /* open TCP socket */
    cmd[n++] = (unsigned char)(port & 0xFF);
    cmd[n++] = (unsigned char)(port >> 8);
    while (*host && n < sizeof(cmd) - 1) {
        cmd[n++] = (unsigned char)*host++;
    }
    cmd[n++] = 0;
    if (uci_exec(cmd, n, resp, sizeof(resp), &rlen, stat, smax, 500)) {      /* up to 10 s */
        return 1;
    }
    if (!STATUS_IS(stat, 0x30, 0x30) || rlen < 1) {
        return 2;
    }
    *sock = resp[0];
    return 0;
}


int net_read(unsigned char sock, unsigned char *buf, unsigned max, unsigned *got)
{
    unsigned char cmd[5], resp[204];
    char stat[32];
    unsigned rlen, count, i;

    *got = 0;
    cmd[0] = 3;
    cmd[1] = 0x10;                     /* read socket */
    cmd[2] = sock;
    cmd[3] = (unsigned char)(max & 0xFF);
    cmd[4] = (unsigned char)(max >> 8);
    if (uci_exec(cmd, 5, resp, sizeof(resp), &rlen, stat, sizeof(stat), 10)) {   /* 0.2 s */
        return 2;
    }
    if (STATUS_IS(stat, 0x30, 0x31)) {
        return 1;                      /* "01,CONNECTION CLOSED BY HOST" */
    }
    if (!STATUS_IS(stat, 0x30, 0x30) || rlen < 2) {
        return 2;
    }
    count = (unsigned)resp[0] | ((unsigned)resp[1] << 8);
    if (count > rlen - 2) {
        count = rlen - 2;
    }
    if (count > max) {
        count = max;
    }
    for (i = 0; i < count; ++i) {
        buf[i] = resp[2 + i];
    }
    *got = count;
    return 0;
}

int net_write(unsigned char sock, const unsigned char *data, unsigned n, unsigned *sent)
{
    unsigned char cmd[3 + NET_WRITE_MAX], resp[4];
    char stat[16];
    unsigned rlen, i;

    *sent = 0;
    if (n > NET_WRITE_MAX) {
        n = NET_WRITE_MAX;
    }
    cmd[0] = 3;                        /* network target */
    cmd[1] = 0x11;                     /* write socket */
    cmd[2] = sock;
    for (i = 0; i < n; ++i) {
        cmd[3 + i] = data[i];
    }
    if (uci_exec(cmd, 3 + n, resp, sizeof(resp), &rlen, stat, sizeof(stat), 50)) {
        return 1;
    }
    if (!STATUS_IS(stat, 0x30, 0x30) || rlen < 2) {
        return 2;
    }
    *sent = (unsigned)resp[0] | ((unsigned)resp[1] << 8);
    return 0;
}

void net_close(unsigned char sock)
{
    unsigned char cmd[3], resp[4];
    char stat[16];
    unsigned rlen;

    cmd[0] = 3;
    cmd[1] = 9;
    cmd[2] = sock;
    uci_exec(cmd, 3, resp, sizeof(resp), &rlen, stat, sizeof(stat), 50);
}

