# ultinet

TCP sockets for the Commodore 64 Ultimate, from C (cc65). It talks to the
Ultimate's **Command Interface** (the registers at `$DF1C`-`$DF1F`) and uses its
**network target** to open a TCP connection, read, write and close, and it adds
a small reader that turns a connection into text lines.

It is useful when a C64 program should talk to something on your network: a
phone, a Raspberry Pi, a small server. The Ultimate makes outgoing connections
only, so the other side is the server and the C64 is the client.

## Status

- Written from the Ultimate documentation (see below).
- Opening a connection, reading and closing have been used with a real C64
  Ultimate, reading lines from a program on a phone over Wi-Fi.
- **Writing (`net_write`, `ls_send`) has only been run against the simulation in
  `tests/`, not on a real Ultimate.** Treat it as untested until you try it.
- Whether two sockets can be open at once is not known.

## Using it

Enable **Command Interface** in the Ultimate's menu first. Without it
`uci_present()` returns 0.

```c
#include "linestream.h"

static void on_line(const char *line, void *user)
{
    /* one complete line, without its line ending */
}

struct linestream ls;

if (ls_open(&ls, "192.168.1.20", 6464) == LS_OK) {
    while (ls_poll(&ls, on_line, 0) == 0) {
        /* ... your main loop ... */
    }
    ls_close(&ls);
}
```

`examples/read_lines.c` is a complete program; build it with `make c64`.

### The two layers

- `uci.h`: the command interface itself. `uci_present()`, `uci_exec()` (run any
  command and get its response and status text), and the network target:
  `net_connect()`, `net_read()`, `net_write()`, `net_close()`.
- `linestream.h`: a connection plus the line being built. `ls_open()`,
  `ls_poll()` (call it often; it returns at once if nothing arrived), `ls_send()`,
  `ls_close()`. A line longer than `LS_LINE_MAX` (default 96) is dropped whole.

Each call waits for the Ultimate to answer, so a read takes as long as one
command round trip. Time it on your machine and poll at a rate that suits your
program.

### Things to know

- Hosts are dotted addresses or names, as ASCII. Port numbers are given as
  numbers and sent in the order the documentation requires.
- Timeouts are in `clock()` ticks.
- Characters from the network are bytes as sent. A C64 compiler may translate
  character constants to PETSCII, so the code uses numbers for line endings
  (10 and 13); do the same when comparing network data.
- `net_read` returns at most 200 bytes per call, and `net_write` sends at most
  `NET_WRITE_MAX` (128). These are the sizes of this library's buffers, not limits
  the documentation gives; a BASIC example online asks for 255 bytes at once.
- If the other end closes the connection, the read that reports it
  (`net_read` returns 1) does not return any bytes that came with that reply. The
  documentation does not say whether the Ultimate sends any, so a protocol that
  must not lose its last bytes should say "bye" itself before closing.

## Tests

```
make test       # builds the driver for the host against a simulated Ultimate and runs it
make c64        # checks that it builds for the C64 (needs cc65)
```

The simulation in `tests/uci_sim.c` follows the documented state machine: push a
command, wait, read the response in blocks, accept. It also exercises timeouts
and aborts, a slow interface and an interface that never answers. It does not
prove the real firmware behaves the same.

## Related

- The Ultimate documentation:
  [core UCI architecture](https://1541u-documentation.readthedocs.io/en/latest/uci/core_uci_architecture.html)
  and the [network target](https://1541u-documentation.readthedocs.io/en/latest/uci/network_target.html).
- [Taking the C64 Ultimate Online](https://tierceron.com/commodore/online/index.php):
  a BASIC walk-through of the same registers.
- IP65 is a different approach, a software TCP/IP stack for Ethernet
  adapters.

## License

MIT; see `LICENSE`.
