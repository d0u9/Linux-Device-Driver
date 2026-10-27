# lddtty

A tty driver without hardware, after LDD3's tiny_tty: four lines,
`/dev/lddtty0` to `/dev/lddtty3`. While a line is open, a timer stands for
the receive interrupt and delivers a `z` every second; what is written to
the line is collected into lines and printed with `printk`. No hardware is
needed.

## where the driver sits

```
read()/write()
   |
tty core          /dev/lddtty*, sessions, job control
   |
line discipline   N_TTY: echo, line editing, Ctrl-C, \n -> \r\n
   |
tty driver        this module: write() and the flip buffer
   |
"the wire"        the timer and printk
```

The driver only moves characters. Echo, `stty`, canonical lines and
signals all come from the layers above it.

## what each part shows

- **Registering.** `tty_alloc_driver()` and `tty_register_driver()` create
  the driver with a dynamic major and, without `TTY_DRIVER_DYNAMIC_DEV`,
  the four device nodes. `init_termios` gives each line its settings on
  first open; `TTY_DRIVER_RESET_TERMIOS` restores them after the last
  close.
- **Ports.** Each line has a `struct tty_port`, linked to it with
  `tty_port_link_device()`. The `.open`, `.close` and `.hangup` operations
  only call `tty_port_open()`, `tty_port_close()` and `tty_port_hangup()`:
  the port counts the opens, calls `port_ops.activate` on the first, which
  starts the timer, and `port_ops.shutdown` after the last, which stops it
  with `timer_delete_sync()` and prints what is left of the line.
- **Receiving.** `tty_insert_flip_char()` puts a character into the port's
  flip buffer and `tty_flip_buffer_push()` hands it to the line discipline.
  Nothing is read by the driver; `read()` is served by N_TTY.
- **Writing.** `.write` must not sleep: the line discipline also calls it
  to echo, from where it receives characters. `.write_room` says how much
  it may be given.
- **Line settings.** `.set_termios` is called by `stty` and `tcsetattr()`;
  it decodes the data bits, parity, stop bits, flow control and baud rate,
  as a UART driver would program them.
- **Modem lines.** `.tiocmget` and `.tiocmset` report and set DTR, RTS and
  the input lines, for `TIOCMGET` and `TIOCMSET`.
- **/proc.** `.proc_show` makes `/proc/tty/driver/lddtty`, listing the open
  lines.

## shorter ways of writing the same

- `tty_port_install()` called from `.install` links the port at open time,
  instead of `tty_port_link_device()` at load time.

## older ways of doing the same

- Drivers counted opens themselves and kept the `tty_struct`; the port
  helpers do both now.
- `tty_driver.major` was often fixed, with `mknod` in the load script.
- `.write` took `const unsigned char *` and an `int`, and `.write_room`
  returned `int`.
- `del_timer_sync()` is `timer_delete_sync()`, and `from_timer()` is
  `timer_container_of()`.

## build the module

To build this module, execute:

```bash
make KDIR=/path/to/kernel/source/dir
```

If you have already set and exported `KDIR` environment variable, simply
execute `make` is enough.

If neither `KDIR` environment variable nor `KDIR` option of make
are set, the current running kernel will be built against.

## Usage

Copy **load_module.sh**, **lddtty.ko** and **test/tty_test.sh** to the
target machine, then run:

```bash
sh load_module.sh
```

## test the module

```bash
(stty raw -echo; dd bs=1 count=3) < /dev/lddtty0    # zzz, one a second
echo hello > /dev/lddtty1; dmesg | tail -n 1         # [lddtty1] hello
sh tty_test.sh
```

```
ok   /dev/lddtty0..3 created
ok   two characters read from lddtty0
ok   lddtty1 listed as open in /proc
ok   lddtty1 no longer listed after close
ok   line written to lddtty2 printed
ok   a 100-char line split after 80
ok   baud rate set on lddtty3
PASSED
```

In the default, canonical mode, `cat /dev/lddtty0` shows nothing: the
`z`s are echoed and wait for a newline that never comes. When the line is
closed, the echoes are printed, as `[lddtty0] zz...`.

---

### ¶ The end
