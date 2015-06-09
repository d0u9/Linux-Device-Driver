# proc file system implemented by seq file

## Description

This module will create a proc file in `/proc` named `proc-fs-iterator`.
Initialially, there is no data in this devices' buff. you can write any thing
into this proc file, and in each writing a new buff node will be crated and be
used as storage buff for the inputs. All the buff nodes are linked together by
kernel's doubly link list library.

When you read from this file, the seq's iterator will travels each node in this
list, and `seq_show` function will be invoked to print the content in each node.

## Usage

1. Compile by executing `make` and then load this module by `insome` command.
2. Open and write this file servel times to fill the internal buffers.
3. Use `cat` command to verifiy the effect.

## Warning

There is no locks in this device to protect data against the concurrent
access. So, it is your duty to ensure there are NO concurrent access!

