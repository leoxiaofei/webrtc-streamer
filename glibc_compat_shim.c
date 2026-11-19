/* Compatibility shims for linking against an older glibc sysroot (2.31):
** some webrtc-bundled static libraries (libc++, boringssl) were compiled with
** newer glibc headers that redirect fcntl->fcntl64 and emit references that
** do not bind to the versioned symbols of the sysroot libc.
*/
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/syscall.h>

int fcntl64(int fd, int cmd, ...)
{
	va_list ap;
	va_start(ap, cmd);
	long arg = va_arg(ap, long);
	va_end(ap);
	return fcntl(fd, cmd, arg);
}

ssize_t copy_file_range(int fd_in, off64_t *off_in, int fd_out, off64_t *off_out, size_t len, unsigned int flags)
{
	return syscall(SYS_copy_file_range, fd_in, off_in, fd_out, off_out, len, flags);
}
