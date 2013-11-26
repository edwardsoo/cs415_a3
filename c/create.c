/* create.c : create a process
 */

#include <xeroskernel.h>

/* Your code goes here. */
extern unsigned short getCS(void);

// process management variables
extern pcb pcbTable[MAX_NUM_PROCESS];
extern treeNode *pidMap;
extern unsigned int nextPid;

/* Set all general registers in a context frame struct to zero */
void zeroRegisters(contextFrame *context) {
  context->eax = 0;
  context->ecx = 0;
  context->edx = 0;
  context->ebx = 0;
  context->esp = 0;
  context->ebp = 0;
  context->esi = 0;
  context->edi = 0;
}

// returns next ununsed non-zero PID
unsigned int getNextPid(void) {
  unsigned int pcbIndex;
  while (!nextPid || pidMapLookup(nextPid, &pcbIndex) == OK) nextPid++;
  return nextPid++;
}

/* Create a process 
* @param func - The function that the new process will run
* @param stack - size of process stack, to be dynamically allocated
* @param parent - pid of parent process
* @return pid of new process or SYSERR 
*/
int create (void (*func)(void), int stack, unsigned int parent) {
  pcb* pcb;
  unsigned int ptr;
  unsigned int mallocSize, i;
  contextFrame *context;

  // In additional to stack size, allocate some space for a safety margin and context frame
  mallocSize = stack + SAFETY_MARGIN + sizeof(contextFrame);
  
  ptr = (unsigned int) kmalloc(mallocSize);
  if (ptr != NULL) {
    pcb = NULL;

    // Look for usable PCB
    for (i = 0; i < MAX_NUM_PROCESS; i++) {
      if (pcbTable[i].state == STOPPED) {
        pcb = &pcbTable[i];
        break;
      }
    }

    if (pcb) {
      // Find the next currently unused PID
      pcb->pid = getNextPid();
      pcb->parentPid = parent;
      pcb->next = NULL;
      pcb->stack = (void*) ptr;
      
      // Update PID in the PCB map
      pidMapInsert(pcb->pid, i);

      pcb->esp = ptr + ((stack/16) + (stack%16?1:0))*16;
      context = (contextFrame*) (pcb->esp) ;
      zeroRegisters(context);
      
      // Initialize EIP and CS so when kernel does iret, the process starts running func
      context->iret_eip = (unsigned int) func;
      context->iret_cs = (unsigned int) getCS();
      // Enable interrupts
      context->eflags = 0x00003200;

      // Setup stack so process calls sysstop when run out of code
      context->args[0] = (unsigned int) sysstop;

      // Disable all signals
      for (i = 0; i < NUM_SIGNAL; i++) {
        pcb->sig_handler[i] = NULL;
      }
      pcb->pending_sig = 0;
      pcb->allowed_sig = 0;
      pcb->hi_sig = 0xFFFFFFFF;

      // Set file descriptor table to NULL
      for (i = 0; i < NUM_FD; i++) {
        pcb->opened_dv[i] = NULL;
      }

      // Add to ready queue
      ready(pcb);
      return pcb->pid;

    } else {
      // PCB table full, free space and return error
      kfree((void*) ptr);
    }
  }
  return SYSERR;
}

