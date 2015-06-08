# proc file system

## Description

This module will crate a directory and a file in `/proc`. When read from this 
file, a string of *Hello World* will be returned.

## Usage

1. Compile by executing `make` and then load this module by `insome` command.
2. Read `/proc/my_sub_dir/proc-fs` file, you will get *Hello World*.

