#ifndef UCI_SIM_H
#define UCI_SIM_H

void sim_reset(void);
void sim_net_push(const char *s);

extern int sim_present, sim_busy_polls, sim_hang, sim_aborts, sim_commands;
extern unsigned char sim_net_in[2000];
extern unsigned sim_net_len, sim_net_pos;
extern int sim_net_closed, sim_socket_open;
extern unsigned char sim_net_out[2000];
extern unsigned sim_net_outlen;

#endif
