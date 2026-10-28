/* Connect to a server that sends text lines and print them. Edit HOST and PORT.
 * Build with: make c64 */
#include <stdio.h>
#include <conio.h>

#include "linestream.h"

#define HOST "192.168.1.20"
#define PORT 6464

static void on_line(const char *line, void *user)
{
    (void)user;
    printf("%s\n", line);
}

int main(void)
{
    struct linestream ls;
    int r = ls_open(&ls, HOST, PORT);

    if (r == LS_NO_UCI) {
        printf("TURN ON THE COMMAND INTERFACE IN THE ULTIMATE MENU\n");
    } else if (r == LS_TIMEOUT) {
        printf("NO ANSWER\n");
    } else if (r) {
        printf("NOT CONNECTED: %s\n", ls.stat);
    } else {
        while (!kbhit() && ls_poll(&ls, on_line, 0) == 0) {
        }
        ls_close(&ls);
    }
    return 0;
}
