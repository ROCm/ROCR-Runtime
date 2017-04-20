#include <errno.h>
#include <sys/ioctl.h>

#include "libhsakmt.h"

/* Call ioctl, restarting if it is interrupted */
int kmtIoctl(int fd, unsigned long request, void *arg)
{
	int ret;

	do {
		ret = ioctl(fd, request, arg);
	} while (ret == -1 && (errno == EINTR || errno == EAGAIN));

	return ret;
}
