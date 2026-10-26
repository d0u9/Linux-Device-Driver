# Static, so it runs in the guest without its libraries.
BIN := mmap_test.out

.PHONY: all
all:
	$(CC) -Wall -static -o $(BIN) mmap_test.c

.PHONY: clean
clean:
	rm -f $(BIN)
