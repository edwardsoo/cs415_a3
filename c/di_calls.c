#include <xeroskernel.h>
#include <stdarg.h>

devsw devtab[NUM_DEVICE];

void di_open(pcb* p, int device_no) {
  // Invalid device number
  if (device_no < 0 || device_no >= NUM_DEVICE) {
    p->irc = -1;

  } else {
    // Driver accepted open request, update PCB file descriptor table
    if (devtab[device_no].dvopen(p) == DRV_DONE) {
      p->opened_dv[device_no] = devtab + device_no;
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
    if (devtab[fd].dvclose(p) == DRV_DONE) {
      p->opened_dv[fd] = NULL;
      p->irc = 0;

    } else {
      p->irc = -1;
    }
  }
  ready(p);
}

void di_write(pcb* p, int fd, void* buf, int buf_len) {
  int rc;

  // Invalid device number or device not opened by process
  if (fd < 0 || fd >= NUM_DEVICE || p->opened_dv[fd] == NULL) {
    p->irc = -1;
    ready(p);

  } else {
    rc = devtab[fd].dvwrite(p, buf, buf_len);
    // Driver accepted write request and blocked process
    if (rc == DRV_BLOCK) {
      p->state = WAITING;

    // Driver completed write
    } else if (rc == DRV_DONE) {
      ready(p);

    } else {
      p->irc = -1;
      ready(p);
    }
  }
}

void di_read(pcb* p, int fd, void* buf, int buf_len) {
  int rc;

  // Invalid device number or device not opened by process
  if (fd < 0 || fd >= NUM_DEVICE || p->opened_dv[fd] == NULL) {
    p->irc = -1;
    ready(p);

  } else {
    rc = devtab[fd].dvread(p, buf, buf_len);
    // Driver accepted read request and blocked process
    if (rc == DRV_BLOCK) {
      p->state = READING;

    // Driver completed read
    } else if (rc == DRV_DONE) {
      ready(p);

    } else {
      p->irc = -1;
      ready(p);
    }
  }
}

void di_ioctl(pcb* p, int fd, unsigned long cmd, va_list ap) {
  int rc;

  // Invalid device number or device not opened by process
  if (fd < 0 || fd >= NUM_DEVICE || p->opened_dv[fd] == NULL) {
    p->irc = -1;
    ready(p);

  } else {
    rc = devtab[fd].dvioctl(cmd, ap);
    if (rc == DRV_DONE) {
      p->irc = 0;
      ready(p);

    } else {
      p->irc = -1;
      ready(p);
    }
  }
}
