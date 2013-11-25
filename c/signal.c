#include <xeroskernel.h>

extern long freemem;
extern pcb pcbTable[MAX_NUM_PROCESS];
extern unsigned short getCS(void);
extern void zeroRegisters(contextFrame *context);

void register_sig_handler(pcb* p, int signal,
    handler new_handler, handler* old_handler) {
  // kprintf("registering signal %u with handler 0x%x\n", signal, new_handler);

  if (signal < 0 || signal >= NUM_SIGNAL) {
    p->irc = -1;
    return;
  }

  if (old_handler) {
    *old_handler = p->sig_handler[signal];
  }

  if ((long) new_handler >= freemem) {
    p->irc = -2;
    return;
  }

  p->sig_handler[signal] = new_handler;
  kprintf("process %d registered 0x%x as signal %d handler\n",
      p->pid, new_handler, signal);
  if (new_handler == NULL) {
    p->allowed_sig &= ~(SIG_INT(signal));
  } else {
    p->allowed_sig |= SIG_INT(signal);
  }
  p->irc = 0;
}

void sigtramp(handler handler, void *cntx, void *old_sp) {
  handler(cntx);
  syssigreturn(old_sp);
}

int signal(unsigned int pid, int sig_no) {
  unsigned pcb_index;
  pcb* p;

  if (sig_no < 0 || sig_no >= NUM_SIGNAL) {
    return -2;
  }

  if (pidMapLookup(pid, &pcb_index) != OK) {
    return -1;
  }

  p = pcbTable + pcb_index;
  kprintf("target pid %d, allowed_sig 0x%x, pending_sig 0x%x, hi_sig 0x%x\n",
      pid, p->allowed_sig, p->pending_sig, p->hi_sig);  

  // Check if signal should be recorded for delivery
  if (p->allowed_sig & SIG_INT(sig_no)) {
    kprintf("target pid %u record signal %d for delivery\n", p->pid, sig_no);
    p->pending_sig |= SIG_INT(sig_no);
  }
  return 0;
}

// returns the position of the most significant bit that is set
// Algorithm from Hacker's delight
int msb_1_pos(unsigned int x) {
   int n;
   if (x == 0) return (32);
   n = 0;
   if (x <= 0x0000FFFF) {n = n +16; x = x <<16;}
   if (x <= 0x00FFFFFF) {n = n + 8; x = x << 8;}
   if (x <= 0x0FFFFFFF) {n = n + 4; x = x << 4;}
   if (x <= 0x3FFFFFFF) {n = n + 2; x = x << 2;}
   if (x <= 0x7FFFFFFF) {n = n + 1;}
   return n;
}

void deliver_signal(pcb* p) {
  contextFrame *new_cntx;
  signal_frame *sig_frame;
  int sig_no, sig_int;

  // where();
  // check if there is a signal to deliver
  if (p->pending_sig & p->hi_sig) {
    // Get signal number to deliver
    sig_no = NUM_SIGNAL - msb_1_pos(p->pending_sig) - 1; 
    kprintf("needs to deliver sig %u to pid %u\n", sig_no, p->pid);

    // Put signal frame on process stack
    new_cntx = (contextFrame*) (p->esp - sizeof(signal_frame));
    zeroRegisters(new_cntx);

    // point EIP to signal handler
    new_cntx->iret_eip = (unsigned int) sigtramp;
    new_cntx->iret_cs = (unsigned int) getCS();
    // Enable interrupts
    new_cntx->eflags = 0x3200;

    // Set up sigtramp arguments and values to be stored
    sig_frame = (signal_frame*) new_cntx;
    sig_frame->ret_addr = (unsigned int) sysstop;
    sig_frame->handler= (unsigned int) p->sig_handler[sig_no];
    sig_frame->cntx = (unsigned int) p->esp;
    sig_frame->old_sp = (unsigned int) p->esp;
    sig_frame->old_hi_sig = (unsigned int) p->hi_sig;
    sig_frame->old_irc = (unsigned int) p->irc;
    p->esp = new_cntx;

    // Update signal masks
    sig_int = SIG_INT(sig_no);
    p->pending_sig &= ~sig_int;
    p->hi_sig = (~sig_int) - (sig_int - 1);
  }
}
