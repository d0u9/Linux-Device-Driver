# Hello World

## Usage
1. Execute `make` command.
2. Execute `insmod hello-world.ko`.
3. Use `dmesg | tail -10` command to verify the result of printing
`Hello, world!`.
4. Execute `rmmod hello-world` to unload the driver.
5. Again, use `dmesg | tail -10` command to see what happened.
