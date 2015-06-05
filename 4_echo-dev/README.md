# Echo device

## Desciption
In this example, we implement a simple echo device, which receives an input char
sequence and then can be read out.

## Usage
1. Execute `make` command.
2. Execute `sudo ./load_module.sh` to load device driver and create dev files
in `/dev`.
3. Write something to arbitrary deveices which are named `echo0` to `echo3` in
`/dev` directory. Simply, `echo "xxxx" > /dev/echoN`.
4. Read from the device which just be writen in. `cat /dev/echoN` is enough.
5. Here, you should have seen the string which you wrote in step 3.

## Warning
The length of your input **SHOULD NOT** exceed the `ECHO_BUFF_SIZE`, which is
defined in `global.h`.
