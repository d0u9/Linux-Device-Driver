BIN1 := remap.out
BIN2 := fault.out


.PHONY: all
all:
	$(CC) -static -o $(BIN1) remap.c
	$(CC) -static -o $(BIN2) fault.c

.PHONY: clean
clean:
	rm -fr $(BIN1) $(BIN2)
