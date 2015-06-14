#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(int argc, char *argv[])
{
	int fd = open("/dev/ioctl-test", O_RDONLY);
	int err = ioctl(fd, 25600);
	printf("retval = %d\n", err);

	return 0;	
}
