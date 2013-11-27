/* sleep.c : sleep device (assignment 2)
 */

#include <xeroskernel.h>

/* Your code goes here */
pcb *sleep_list;

/*
 * Decrements key of head of the delta list,
 * puts all process with key == 0 on ready queue
 */
void tick() {
  pcb *p;

  if (sleep_list) {
    sleep_list->delta--;
    while (sleep_list->delta == 0) {
      p = sleep_list;
      p->irc = 0;
      sleep_list = p->next;
      ready(p);
    }
  }
}

/*
 * Insert a process into the delta list 
 */
void sleep(pcb *p, unsigned int milliseconds) {
  unsigned int delta;
  pcb** next;

  delta = (milliseconds/TIME_SLICE_MS) + (milliseconds%TIME_SLICE_MS?1:0);
  if (delta) {
    next = &sleep_list;
    // Traverse the delta list
    while (*next && (*next)->delta <= delta) {
      // Adjust the delta value of the sleep process
      delta -= (*next)->delta;
      next = &(*next)->next;
    }
    p->next = *next;
    p->delta = delta;
    if (p->next) {
      p->next->delta -= delta;
    }
    *next = p;
    p->state = SLEEPING;
  } else {
    ready(p);
  }
}

void traverseSleepList(void) {
  pcb *pcb = sleep_list;
  kprintf("Sleep list: ");
  while (pcb) {
    kprintf("P%u:%u -> ", pcb->pid, pcb->delta);
    pcb = pcb->next;
  }
  kprintf("\n");
}
