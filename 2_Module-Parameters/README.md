# Module Parameters

## Description
This module has two parameters inside, each parameter can be initialized at
loading moment.

## Usage
1. Use `insmod module-parameters.ko name="test" howmany=3` command to load
module.
2. Use `dmesg | tail -10` to check the result.
3. Also, you can check `/sys/module/module_parameters/parameters/` to
identify the values.
