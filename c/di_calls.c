#include <xeroskernel.h>
#include <stdarg.h>

devsw devtab[NUM_DEVICE];

void di_open(pcb* p, int major_no) {
}

void di_close(pcb* p, int fd) {
}

void di_write(pcb* p, void* buf, int buflen) {
}

void di_read(pcb* p, void* buf, int buflen) {
}

void di_ioctl(pcb* p, int fd, unsigned long cmd, va_list ap) {
}
