#include <xeroskernel.h>
#include <stdarg.h>

devsw devtab[NUM_DEVICE];

int di_open(pcb* p, int major_no) {
	return 0;
}

int di_close(pcb* p, int fd) {
	return 0;
}

int di_write(pcb* p, void* buf, int buflen) {
	return 0;
}

int di_read(pcb* p, void* buf, int buflen) {
	return 0;
}

int di_ioctl(pcb* p, int fd, unsigned long cmd, va_list ap) {
	return 0;
}
