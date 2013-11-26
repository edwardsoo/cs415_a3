#include <xeroskernel.h>
#include <stdarg.h>

devsw devtab[NUM_DEVICE];

void di_open(pcb* p, int device_no) {
  // Invalid device number
  if (device_no < 0 || device_no >= NUM_DEVICE) {
    p->irc = -1;

  } else {
    // Driver accepted open request, update PCB file descriptor table
    if (devtab.dvopen(p) == 0) {
      p->opened_dv[device_no] = dev_tab + device_no;
      p->irc = device_no;

    } else {
      p->irc = -1;
    }
  }
  ready(p); 
}

void di_close(pcb* p, int fd) {
  // Invalid device number or device not opened by process
  if (fd < 0 || fd >= NUM_DEVICE || p->opened_dv[fd] == NULL) {
    p->irc = -1;

  } else {
    // Drive closed device for process
    if (devtab[fd].dvclose(p) == 0) {
      p->opened_dv[fd] = NULL;
      p->irc = 0;

    } else {
      p->irc = -1;
    }
  }
  ready(p);
}

void di_write(pcb* p, void* buf, int buf_len) {
  int rc;

  // Invalid device number or device not opened by process
  if (fd < 0 || fd >= NUM_DEVICE || p->opened_dv[fd] == NULL) {
    p->irc = -1;
    ready(p);

  } else {
    rc = devtab[fd].dvwrite(p, buf, buf_len);
    // Driver accepts write request, will unblock process when request completes
    if (rc >= 0) {
      p->irc = rc;
      p->state == WRITING;

    } else {
      p->irc = -1;
      ready(p);
    }
  }
}

void di_read(pcb* p, void* buf, int buf_len) {
  int rc;

  // Invalid device number or device not opened by process
  if (fd < 0 || fd >= NUM_DEVICE || p->opened_dv[fd] == NULL) {
    p->irc = -1;
    ready(p);

  } else {
    rc = devtab[fd].dvread(p, buf, buf_len);
    // Driver accepts read request, will unblock process when request completes
    if (rc >= 0) {
      p->irc = rc;
      p->state = READING;

    } else {
      p->irc = -1;
      ready(p);
    }
  }
}

void di_ioctl(pcb* p, int fd, unsigned long cmd, va_list ap) {
  int rc;
}
