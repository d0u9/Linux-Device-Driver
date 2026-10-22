BIN_CLIENT = client.out
BIN_SERVER = server.out

.PHONY: all
all:
	$(CC) -I../ -static -o $(BIN_CLIENT) client.c
	$(CC) -I../ -static -o $(BIN_SERVER) server.c

.PHONY: clean
clean:
	rm -fr $(BIN_CLIENT) $(BIN_SERVER)
