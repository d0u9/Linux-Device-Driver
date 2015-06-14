#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>

#define NEW_STR		"Hello, Linux!!\n"
#define NEW_STR_LEN	sizeof(NEW_STR)

struct ioctl_str {
	int str_len;
	char *str;
};

int main(int argc, char *argv[])
{
	char *str = (char *)malloc(NEW_STR_LEN);
	memset(str, 0, NEW_STR_LEN);
	memcpy(str, NEW_STR, NEW_STR_LEN);

	struct ioctl_str str_io = {
		.str_len = NEW_STR_LEN,
		.str = str,
	};

	int fd = open("/dev/ioctl-test", O_RDONLY);
	/*int err = ioctl(fd, 25600);	//reset*/
	/*int err = ioctl(fd, 0xc0046401, );*/
	int err = ioctl(fd, 0x40046402, &str_io);
	printf("retval = %d\n", err);

	return 0;	
}
