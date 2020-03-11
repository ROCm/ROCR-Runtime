#include <stdio.h>
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

	if (errno == EBADF) {
		/* In case pthread_atfork didn't catch it, this will
		 * make any subsequent hsaKmt calls fail in CHECK_KFD_OPEN.
		 */
		pr_err("KFD file descriptor not valid in this process\n");
		is_forked_child();
	}

	return ret;
}
