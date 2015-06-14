# Illustration of usage of ioctl interface

## Description

In this example, we illustrate a simple example to show the basic usage of 
`ioctl` interface. `ioctl` interface is widely used in the configuratong
of hardware, or some normal file operations couldn't expressed sitiations.

This example will create a device file in `/dev` named `ioctl-test`. There are
3 sub-commands in ioctl:

1. Reset the device to it's initial state.
2. Change the number of ouput iterations.
3. Change the contents of ouput.

## Usage

1. Compile by executing `make` and then load this module by `sudo ./load_modue.sh`
command.
2. Modify the userspace example in `test` directory to follow your will.
3. Compile and run the userspace example.

## Warning

There is no locks in this device to protect data against the concurrent
access. So, it is your duty to ensure there are NO concurrent access!

