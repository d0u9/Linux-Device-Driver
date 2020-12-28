#include <stdio.h>
#include <stdlib.h>

#include <netdb.h>
#include <netinet/in.h>
#include <net/if.h>

#include <string.h>

int main(int argc, char *argv[])
{
	int rv, sk, confd;
	char *buf[1024];
	struct ifreq ifr;
	struct sockaddr_in serv_addr;

	sk = socket(AF_INET, SOCK_STREAM, 0);
	if (sk < 0) {
		perror("ERROR opening socket");
		exit(1);
	}

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, "sn0", 5);
	if (setsockopt(sk, SOL_SOCKET, SO_BINDTODEVICE, (void*)(&ifr), sizeof(ifr)) < 0) {
		perror("setsocket filed\n");
		exit(1);
	}

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(5001);
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	rv = bind(sk, (struct sockaddr*)(&serv_addr), sizeof(serv_addr));
	if (rv) {
		perror("bind failed\n");
		exit(1);
	}

	rv = listen(sk, 10);
	if (rv) {
		perror("listen failed\n");
		exit(1);
	}

	while (1) {
		confd = accept(sk, NULL, NULL);	
		printf("accept ==== \n");
		memset(buf, 0, 1024);
		recv(confd, buf, 1024, 0);
		printf("msg: %s\n", buf);
	}

	close(sk);

	return 0;
}
