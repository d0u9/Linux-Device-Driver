# Static, so it runs in the guest without its libraries.
BIN := zero.out

.PHONY: all
all:
	$(CC) -Wall -static -I.. -o $(BIN) main.c

.PHONY: clean
clean:
	rm -f $(BIN)
