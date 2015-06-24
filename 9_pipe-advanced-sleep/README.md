# pipe-advanced-sleep

## Description

This example shows a kernel module which act similar to a pipe. We used advanced sleep technology to suspend a process
when its requested data is invaliable. The advanced sleep is different from simple sleep technology for that advanced 
sleep can give us more free to control all the details during the sleeping procedure.

## Usage

1. Comlile source code and load the module via `sudo ./load_module` command.
2. Write to or read from `/dev/pipeX` in two distict processes. You will observe
that one process would be blocked when there is no data avaible and could run
again when another process read from or write to the same device.
