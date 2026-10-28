# Static, so it runs in the guest without its libraries.
BIN := dma_sg.out

.PHONY: all
all:
	$(CC) -Wall -static -o $(BIN) main.c

.PHONY: clean
clean:
	rm -f $(BIN)
