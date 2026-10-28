/* A simulation of the Ultimate Command Interface and its network target, for
 * testing src/uci.c and src/linestream.c on the host. It follows the
 * documentation's state machine: push a command, wait for data (in blocks, so
 * "data more" is exercised), read the response and status bytes, accept.
 *
 * The network side is one TCP socket whose incoming bytes the test supplies. */
#include <string.h>

#include "uci_sim.h"

#define BLOCK 40                       /* response bytes per block */

static unsigned char cmd[896];
static unsigned cmdlen;
static unsigned char resp[896];
static unsigned resplen, respblock, respread;
static char status[40];
static unsigned statlen, statread;
static int state;                      /* 0 idle, 1 busy, 2 data last, 3 data more */
static int busy_left;
static int error_flag;

int sim_present = 1;
int sim_busy_polls = 3;                /* status reads before a command's data arrives */
int sim_hang;                          /* commands never complete */
int sim_aborts;
int sim_commands;

unsigned char sim_net_in[2000];
unsigned sim_net_len, sim_net_pos;
int sim_net_closed;
int sim_socket_open;
unsigned char sim_net_out[2000];       /* what the program sent */
unsigned sim_net_outlen;

void sim_reset(void)
{
    state = 0;
    error_flag = 0;
    resplen = statlen = cmdlen = 0;
    sim_present = 1;
    sim_busy_polls = 3;
    sim_hang = 0;
    sim_aborts = 0;
    sim_commands = 0;
    sim_net_len = sim_net_pos = 0;
    sim_net_closed = 0;
    sim_socket_open = 0;
    sim_net_outlen = 0;
}

void sim_net_push(const char *s)
{
    unsigned n = (unsigned)strlen(s);

    memcpy(sim_net_in + sim_net_len, s, n);
    sim_net_len += n;
}

static void set_status(const char *s)
{
    strcpy(status, s);
    statlen = (unsigned)strlen(s);
}

static void run_command(void)
{
    unsigned avail, take;

    ++sim_commands;
    resplen = 0;
    set_status("81,INVALID PARAMS");
    if (cmdlen >= 2 && cmd[0] == 3) {
        switch (cmd[1]) {
        case 1:
            strcpy((char *)resp, "ULTIMATE-II NETWORK INTERFACE V1.0");
            resplen = (unsigned)strlen((char *)resp);
            set_status("00,OK");
            break;
        case 7:                        /* open TCP: port lo, hi, host, 0 */
            if (cmdlen >= 6 && strcmp((char *)cmd + 4, "bad") != 0) {
                resp[0] = 1;
                resplen = 1;
                sim_socket_open = 1;
                set_status("00,OK");
            } else {
                set_status("84,UNRESOLVED HOST");
            }
            break;
        case 9:
            sim_socket_open = 0;
            set_status("00,OK");
            break;
        case 0x11:                     /* write: socket, data */
            if (cmdlen < 3 || cmd[2] != 1 || !sim_socket_open) {
                break;
            }
            take = cmdlen - 3;
            memcpy(sim_net_out + sim_net_outlen, cmd + 3, take);
            sim_net_outlen += take;
            resp[0] = (unsigned char)(take & 0xFF);
            resp[1] = (unsigned char)(take >> 8);
            resplen = 2;
            set_status("00,OK");
            break;
        case 0x10:                     /* read: socket, len lo, hi */
            if (cmdlen != 5 || cmd[2] != 1 || !sim_socket_open) {
                break;
            }
            avail = sim_net_len - sim_net_pos;
            if (avail == 0 && sim_net_closed) {
                resp[0] = resp[1] = 0;
                resplen = 2;
                set_status("01,CONNECTION CLOSED BY HOST");
                break;
            }
            take = (unsigned)cmd[3] | ((unsigned)cmd[4] << 8);
            if (take > avail) {
                take = avail;
            }
            resp[0] = (unsigned char)(take & 0xFF);
            resp[1] = (unsigned char)(take >> 8);
            memcpy(resp + 2, sim_net_in + sim_net_pos, take);
            sim_net_pos += take;
            resplen = take + 2;
            set_status("00,OK");
            break;
        default:
            break;
        }
    }
    respblock = 0;
    respread = statread = 0;
    state = 1;
    busy_left = sim_busy_polls;
}

/* the block of response the host is looking at */
static unsigned block_len(void)
{
    unsigned start = respblock * BLOCK;

    if (start >= resplen) {
        return 0;
    }
    return resplen - start < BLOCK ? resplen - start : BLOCK;
}

static int more_blocks(void)
{
    return (respblock + 1) * BLOCK < resplen;
}

void uci_sim_control(unsigned char v)
{
    if (v & 0x08) {
        error_flag = 0;
    }
    if (v & 0x04) {                    /* abort */
        ++sim_aborts;
        state = 0;
        resplen = statlen = 0;
        sim_hang = 0;
    }
    if (v & 0x01) {                    /* push */
        if (state != 0) {
            error_flag = 1;
        } else if (sim_hang) {
            state = 1;
            busy_left = 1 << 30;
        } else {
            run_command();
        }
        cmdlen = 0;
    }
    if (v & 0x02) {                    /* data accepted */
        if (state == 3) {
            ++respblock;
            respread = 0;
            state = 1;
            busy_left = 1;
        } else if (state == 2) {
            state = 0;
        }
    }
}

unsigned char uci_sim_status(void)
{
    unsigned char s = 0;

    if (state == 1 && busy_left > 0 && --busy_left == 0) {
        state = more_blocks() ? 3 : 2;
        respread = 0;
        if (respblock == 0) {
            statread = 0;
        }
    }
    s |= (unsigned char)(state << 4);
    if (state == 1) {
        s |= 0x01;
    }
    if (error_flag) {
        s |= 0x08;
    }
    if (state >= 2) {
        if (respread < block_len()) {
            s |= 0x80;
        }
        if (state == 2 && statread < statlen) {
            s |= 0x40;
        }
    }
    return s;
}

void uci_sim_cmdbyte(unsigned char v)
{
    if (cmdlen < sizeof(cmd)) {
        cmd[cmdlen++] = v;
    }
}

unsigned char uci_sim_id(void)
{
    return sim_present ? 0xC9 : 0xFF;
}

unsigned char uci_sim_resp(void)
{
    if (respread < block_len()) {
        return resp[respblock * BLOCK + respread++];
    }
    return 0;
}

unsigned char uci_sim_statb(void)
{
    return statread < statlen ? (unsigned char)status[statread++] : 0;
}
