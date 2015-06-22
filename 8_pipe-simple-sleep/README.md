# pipe-simple-sleep

## Description

This example illustrates the usage of basic sleep interface in linux kernel.

## Usage

1. Compile and load module.
2. Read and write `/dev/pipeX`. If the internal buffer is full, Read/Write will
be blocked until there has some free spaces.
