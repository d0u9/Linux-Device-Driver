#include <stdio.h>
#include <stdlib.h>

#include <netdb.h>
#include <netinet/in.h>
#include <net/if.h>

#include <string.h>


int main(int argc, char *argv[])
{
	int sk;
	struct ifreq ifr;
	struct sockaddr_in serv_addr;

	sk = socket(AF_INET, SOCK_STREAM, 0);
	if (sk < 0) {
		perror("ERROR opening socket");
		exit(1);
	}

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, "sn1", 5);
	if (setsockopt(sk, SOL_SOCKET, SO_BINDTODEVICE, (void*)(&ifr), sizeof(ifr)) < 0) {
		perror("setsocket filed\n");
		exit(1);
	}

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(5001);
	if (inet_pton(AF_INET, "172.16.100.1", &serv_addr.sin_addr) <= 0) {
		perror("inet_pton() failed\n");
		exit(1);
	}

	if (connect(sk, (struct sockaddr*)(&serv_addr), sizeof(serv_addr)) < 0) {
		perror("Connect failed\n");
		exit(1);
	}

	send(sk, "hello world!", 13, 0);

	close(sk);

	return 0;
}
