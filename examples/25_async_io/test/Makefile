# Static, so it runs in the guest without its libraries.
BIN := aio_test.out

.PHONY: all
all:
	$(CC) -Wall -static -o $(BIN) aio_test.c

.PHONY: clean
clean:
	rm -f $(BIN)
