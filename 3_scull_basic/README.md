# scull basic

## Description

This example illustrates a simple cache device. Anything you write into this
device will be kept until be rewrote next time. The maxium bytes you can write
into is determined by the size of your machine's memory.

## Usage

1. Compile by `make` command and load module by exectue `sudo ./load_module.sh`.
2. Wrte any thing you want into anyone of `/dev/scull0` to `/dev/scull1`.
3. Read from the devicd you just wrote.
4. You will find the result on your screen.

## Warning

This module may occupied all of your memory space if you write enough bytes into
it. When your memory space runs out, your system will be corrupted. 

Anyway, this module is one good tool to test the performance under a high memory
usage.

