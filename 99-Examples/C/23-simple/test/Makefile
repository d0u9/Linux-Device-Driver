BIN1 := remap.out
BIN2 := fault.out


all:
	gcc -static -o $(BIN1) remap.c
	gcc -static -o $(BIN2) fault.c

.PHONY: clean
clean:
	rm -fr $(BIN1) $(BIN2)
