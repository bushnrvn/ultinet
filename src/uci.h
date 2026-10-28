/* The Ultimate Command Interface (UCI): the registers at $DF1C..$DF1F through
 * which a C64 program asks the Ultimate to do things, here its network target
 * (TCP sockets). Enable "Command Interface" in the Ultimate's menu first.
 *
 * Written from the documentation at 1541u-documentation.readthedocs.io (UCI
 * core architecture and network target). On the host the registers are those
 * of a simulation (tests/uci_sim.c) so the logic can be tested. */
#ifndef UCI_H
#define UCI_H

/* Is the command interface answering? (identification byte $C9, or $49 with its
 * interrupt on). Returns 1 if so. */
int uci_present(void);

/* Run one command: cmd[0..n) is the target byte, the command byte and the
 * parameters. The response data goes to resp (at most rmax bytes; *rlen is
 * how many arrived) and the status text (like "00,OK") to stat as a string of
 * at most smax-1 characters. timeout is in clock() ticks (1/50 second on PAL, 1/60 on NTSC). Returns 0 if
 * the command completed, 1 if it did not (it is then aborted). */
int uci_exec(const unsigned char *cmd, unsigned n, unsigned char *resp, unsigned rmax,
             unsigned *rlen, char *stat, unsigned smax, unsigned timeout);

/* The network target. Hosts are dotted IP addresses or names, as ASCII. */

/* Open a TCP connection. Returns 0 and sets *sock on success; 1 if the
 * interface did not answer, 2 if the connection failed (status in stat). */
int net_connect(const char *host, unsigned port, unsigned char *sock, char *stat, unsigned smax);

/* Read what has arrived on a socket, at most max bytes (max <= 200). Returns
 * 0 and sets *got (possibly 0), 1 if the other end closed the connection, 2
 * if the interface failed. */
int net_read(unsigned char sock, unsigned char *buf, unsigned max, unsigned *got);

/* Send bytes on a socket, at most NET_WRITE_MAX at a time. Returns 0 and sets
 * *sent to how many the Ultimate took, 1 if the interface did not answer, 2 if
 * it refused (for example, the socket is closed). */
#define NET_WRITE_MAX 128
int net_write(unsigned char sock, const unsigned char *data, unsigned n, unsigned *sent);

void net_close(unsigned char sock);

#endif
