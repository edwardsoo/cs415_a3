/* initialize.c - initproc */

#include <i386.h>
#include <stdarg.h>
#include <xeroskernel.h>
#include <xeroslib.h>
#include <kbd.h>

extern int	entry( void );  /* start of kernel image, use &start    */
extern int	end( void );    /* end of kernel image, use &end        */
extern long	freemem; 	/* start of free memory (set in i386.c) */
extern char	*maxaddr;	/* max memory address (set in i386.c)	*/
extern pcb pcbTable[MAX_NUM_PROCESS];
extern pcb *idle;
extern devsw devtab[NUM_DEVICE];

extern void set_evec(unsigned int xnum, unsigned long handler);
extern void enable_irq(unsigned int, int);
extern void initSyscall(void);
extern void enableTimerInterrupt(void);
extern void set_keyboard_ISR(void);
extern void init_pcb_table(void);
extern void dispatch(void);
extern void kmeminit(void);
extern int create (void (*func)(void), int stack, int parent);
extern void idleproc(void);

extern pcb *ready_queue;
extern pcb *sleep_list;

void initproc( void )
{
  int pid;
  unsigned int pcb_index;

	kprintf( "\n\nCPSC 415, 2013W1 \n32 Bit Xeros 1.1\nLocated at: %x to %x\n", &entry, &end ); 

  /* Your code goes here */

  #if RUNTEST
  // Test without pre-emption
  kmeminit();
  initSyscall();
  init_pcb_table();

  testKmalloc();
  kprintf("Passed memory test 1\n");
  testFreeList();
  kprintf("Passed memory test 2\n");
  testContextSwitch();
  kprintf("Passed context switch test\n");
  testProcessManagement();
  kprintf("Passed process management test\n");
  testPidMap();
  kprintf("Passed PID reuse test\n");
  testSendReceive();
  kprintf("Passed messaging test\n");
  testSleepList();
  kprintf("Passed sleep list test\n");
  
  // Test with pre-emption
  enableTimerInterrupt();

  testTimeSharing();
  kprintf("Passed time sharing test\n");
  test_signal();
  kprintf("Passed signal tests\n");
  kprintf("Passed all tests\n");
  #endif

  // Init memory management
  kmeminit();

  // Init some PCB stuff
  init_pcb_table();

  // Set ISR for syscall interrupt
  initSyscall();

  // Enable pre-emption
  enableTimerInterrupt();

  // Set keyboard ISR and init device struct
  set_keyboard_ISR();
  devtab[KEYBOARD_0].dvopen = keyboard_open;
  devtab[KEYBOARD_0].dvclose = keyboard_close;
  devtab[KEYBOARD_0].dvread = keyboard_read;
  devtab[KEYBOARD_0].dvwrite = keyboard_write;
  devtab[KEYBOARD_0].dvioctl = keyboard_ioclt;
  devtab[KEYBOARD_1].dvopen = keyboard_open_echo;
  devtab[KEYBOARD_1].dvclose = keyboard_close;
  devtab[KEYBOARD_1].dvread = keyboard_read;
  devtab[KEYBOARD_1].dvwrite = keyboard_write;
  devtab[KEYBOARD_1].dvioctl = keyboard_ioclt;
  // Disable keyboard interrupt
  enable_irq(1,1);

  // Create first user process
  pid = create(root, 0x2000, NULL);
  // pid = create(semaphore_root, 0x2000, NULL);
  if (pid == SYSERR) {
    kprintf("failed to create root process\n");
  }

  // Create the idle process
  pid = create(idleproc, 0x2000, NULL);
  if (pid == SYSERR) {
    kprintf("failed to create idle process\n");
  }
  pidMapLookup(pid, &pcb_index);
  idle = pcbTable + pcb_index;
  assertEquals(idle->state, READY);

  dispatch();

  /* This code should never be reached after you are done */
	for(;;) ; /* loop forever */
}

void inline abort() {
  __asm __volatile(
      "int $0;\n"
      :::"%eax"
      );
}


/* START OF TEST CODE */
#if RUNTEST

#define EAX_TEST_VALUE 0xaaaa
#define ECX_TEST_VALUE 0xcccc
#define EDX_TEST_VALUE 0xdddd
#define EBX_TEST_VALUE 0xbbbb
#define TEST_IRET_VALUE 0xdeadbeef
#define TEST_STACK_SIZE 0x8000

extern memHeader *freeList;
extern  long  freemem;
extern int contextswitch(pcb* );
extern pcb pcbTable[MAX_NUM_PROCESS];
extern treeNode *pidMap;
extern void cleanup(pcb*);
extern unsigned int nextPid;

void debugMemHeader(memHeader *hdr) {
  dprintf("header@0x%x: size(0x%x), next(0x%x), prev(0x%x), sanityCheck(0x%x)\n",
      (void*) hdr, hdr->size, hdr->next, hdr->prev, hdr->sanityCheck);
}

void traverseFreeList(void) {
  memHeader *hdr = freeList;
  kprintf("Free list:\n");
  while (hdr) {
    kprintf("@0x%x:size(0x%x)\n", (void*) hdr, hdr->size);
    hdr = hdr->next;
  }
}

int getFreeListSize(void) {
  int i = 0;
  memHeader *hdr = freeList;
  dprintf("Free list:\n");
  while (hdr != NULL) {
    dprintf("node%u@0x%x: size(0x%x), next(0x%x), prev(0x%x), sanityCheck(0x%x)\n",
        i, (void*) hdr, hdr->size, hdr->next, hdr->prev, hdr->sanityCheck);
    hdr = hdr->next;
    i++;
  }
  return i;
}

void printTree(treeNode* node, int indent) {
  int i;
  if (!node) return;
  printTree(node->right, indent+1);
  for (i=0; i<indent; i++)
    kprintf("  ");
  //kprintf("%02u->%02u\n", node->pid, node->pcbIndex);
  kprintf("%02u\n", node->pid);
  printTree(node->left, indent+1);
  if (!indent)
    kprintf("\n");
}

void *testKmallocHelper(int size) {
  memHeader *hdr ;
  void *addr;

  addr = kmalloc(size);
  dprintf("kmalloc(0x%x) returned address 0x%x\n", size, addr);
  if (addr) {
    hdr = (memHeader*) (addr - sizeof(memHeader));
    debugMemHeader(hdr);
  }
  return addr;
}

void testKfree(void *ptr) {
  dprintf("calling kfree(0x%x)\n", ptr);
  kfree(ptr);
}

void testFreeList(void) {
  void *ptr1, *ptr2, *ptr3;
  int listSize;

  // Expects 2 node, before and after hole
  listSize = getFreeListSize();
  assertEquals(listSize, 2);

  // Allocate all memory before hole
  ptr1 = testKmallocHelper(freeList->size);
  listSize = getFreeListSize();
  assertEquals(listSize, 1);

  // Allocate all memory after hole
  ptr2 = testKmallocHelper(freeList->size);
  listSize = getFreeListSize();
  assertEquals(listSize, 0);
  
  // Free all memory after hole
  testKfree(ptr2);
  listSize = getFreeListSize();
  assertEquals(listSize, 1);
    
  // Free all memory before hole
  testKfree(ptr1);
  listSize = getFreeListSize();
  assertEquals(listSize, 2);

  // Try to free some random address
  testKfree((void*) HOLEEND + 0x4000);
  listSize = getFreeListSize();
  assertEquals(listSize, 2);
  
  // Allocate blocks then free in reverse order
  ptr1 = testKmallocHelper(0x100);
  listSize = getFreeListSize();
  assertEquals(listSize, 2);
  
  ptr2 = testKmallocHelper(0x100);
  ptr3 = testKmallocHelper(0x100);
  listSize = getFreeListSize();
  assertEquals(listSize, 2);

  // no coalision
  testKfree(ptr2);
  listSize = getFreeListSize();
  assertEquals(listSize, 3);

  // first 2 coalesce
  testKfree(ptr1);
  listSize = getFreeListSize();
  assertEquals(listSize, 3);

  // expected coalision
  testKfree(ptr3);
  listSize = getFreeListSize();
  assertEquals(listSize, 2);
}

void testKmalloc(void) {
  memHeader *hdr;
  void *ptr, *expected, *alignedStart, *ptr_a[3];
  int size;

  // Setup
  alignedStart = (void*) ((freemem/16 + (freemem%16?1:0)) * 16);

  // Regular case
  size = 0x100;
  ptr = ptr_a[0] = testKmallocHelper(size);
  expected = alignedStart + sizeof(memHeader);
  assertEquals(ptr, expected);
  hdr = (memHeader*) (ptr - sizeof(memHeader));
  assertEquals(hdr->size, size);
  
  // Test memory alignment
  size = 0x100;
  ptr = ptr_a[1] = testKmallocHelper(size - 0xf);
  expected = alignedStart + 2*sizeof(memHeader) + 0x100;
  assertEquals(ptr, expected);
  hdr = (memHeader*) (ptr - sizeof(memHeader));
  assertEquals(hdr->size, size);

  // Allocate all memory before HOLESTART
  size = HOLESTART - ((int) alignedStart + 3*sizeof(memHeader) + 0x200);
  ptr = ptr_a[2] = testKmallocHelper(size);
  expected = alignedStart + 3*sizeof(memHeader) + 0x200;
  assertEquals(ptr, expected);
  hdr = (memHeader*) (ptr - sizeof(memHeader));
  assertEquals(hdr->size, size);

  // Allocate something after hole
  size = 0x100;
  ptr = ptr_a[3] = testKmallocHelper(size);
  expected = (void*) HOLEEND + sizeof(memHeader);
  
  // Assert returned address is after HOLE
  assertEquals(ptr, expected);

  kfree(ptr_a[0]);
  kfree(ptr_a[1]);
  kfree(ptr_a[2]);
  kfree(ptr_a[3]);

  // Try to allocate more memory than possible
  ptr = testKmallocHelper(0x400000);
  assertEquals(ptr, NULL);
}

void testContextSwitchChild(void) {
  int ret;
  __asm __volatile(
      // Push dummy parameters
      "push $0;\n"
      "push $0;\n"
      // Set some test values for assertion
      "movl $" xstr(EAX_TEST_VALUE) ", %%eax;\n"
      "movl $" xstr(ECX_TEST_VALUE) ", %%ecx;\n"
      "movl $" xstr(EDX_TEST_VALUE) ", %%edx;\n"
      "movl $" xstr(EBX_TEST_VALUE) ", %%ebx;\n"
      "int $" xstr(SYSCALL) ";\n"
      // Get interrupt return value
      "movl %%eax, %0;\n"
      :"=m"(ret)::"%eax"
      );

  // Assert interrupt return value
  assertEquals(ret, TEST_IRET_VALUE);

  // Return to testContextSwitch with sysstop
  sysstop();
}

void testContextSwitch(void) {
  int pid, request;
  pcb *p;
  contextFrame *context;

  // Assert no process in ready queue
  assertEquals(next(), NULL);
  
  // Create a process that will set some values to its general registers
  pid = create(testContextSwitchChild, 0x2000, NULL);
  assertEquals(pid, 1);

  // Get the new process PCB
  p = next();

  // Transfer control to process
  request = contextswitch(p);
  context = (contextFrame*) p->esp;
  
  // Assert process context are stored correctly
  assertEquals(context->eax, EAX_TEST_VALUE);
  assertEquals(context->ecx, ECX_TEST_VALUE);
  assertEquals(context->edx, EDX_TEST_VALUE);
  assertEquals(context->ebx, EBX_TEST_VALUE);

  // Pass a value back to process
  p->irc = TEST_IRET_VALUE;
  request = contextswitch(p);
  
  // Assert process called sysstop
  assertEquals(request, STOP);

  // Clean up
  cleanup(p);
  nextPid = 0;
}

static volatile int procMaxed = FALSE;
static int numProc = 0;

void spawner(void) {
  int pid;
 
  numProc++;
  sysyield();
  pid = syscreate(spawner, TEST_STACK_SIZE);
  if (pid == SYSERR) {
    // kprintf("last proc pid %u\n", sysgetpid());
    procMaxed = TRUE;
    sysstop();
  } else {
    // Wait for last process to be spawn
    while (!procMaxed) {
      sysyield();
    }
    sysstop();
  }
}

/* Spawn a tree of process until out of memory */
void testProcessManagement(void) {
  void *ptr;
  int i, pid, maxNumStack;

  // Allocate all memory before hole
  ptr = testKmallocHelper(freeList->size);
  
  // Free list should be everything after HOLE
  assertEquals((unsigned int) freeList, HOLEEND);
  maxNumStack = (FREEMEM_END - HOLEEND) / (TEST_STACK_SIZE + SAFETY_MARGIN + sizeof(contextFrame));

  // Init test variable
  numProc = 0;

  // Create a list of process that will take up all memory
  pid = create(spawner, TEST_STACK_SIZE, NULL);
  assertEquals(pid, 1);
  
  // Run dispatch, but this dispatch will return when root process calls sysstop
  dispatch();

  // Assert that maxNumStack processes were created
  assertEquals(numProc, maxNumStack);

  // Assert the next process will get PID equals to numProc, shows PID reuse interval is large
  printTree(pidMap, 0);

  // Assert all PCB have stopped state
  for (i = 0; i < MAX_NUM_PROCESS; i++) {
    assertEquals(pcbTable[i].state, STOPPED);
  }

  // Rerun
  pid = create(spawner, TEST_STACK_SIZE, NULL);
  assertEquals(pid, numProc + 1);
  numProc = 0;
  procMaxed = FALSE;
  dispatch();
  assertEquals(numProc, maxNumStack);
  for (i = 0; i < MAX_NUM_PROCESS; i++) {
    assertEquals(pcbTable[i].state, STOPPED);
  }

  // Clean up
  nextPid = 0;
  kfree(ptr);
}

int sprintTreePreorder(treeNode* node, char *buffer) {
  if (!node) return 0;
  int written = 0;
  written = sprintf(buffer, "%u ", node->pid);
  written += sprintTreePreorder(node->right, buffer+written);
  written += sprintTreePreorder(node->left, buffer+written);
  return written;
}

void testPidMap(void) {
  // char buffer[0x1000];
  // int written;
  unsigned int value;

  pidMapInsert(3, 3);
  pidMapInsert(0, 0);
  pidMapInsert(2, 2);
  pidMapInsert(4, 4);
  pidMapInsert(1, 1);
  pidMapInsert(5, 5);
  pidMapInsert(6, 6);
  pidMapInsert(7, 7);
  pidMapInsert(8, 8);
  pidMapInsert(9, 9);
  pidMapInsert(10, 10);
  pidMapInsert(11, 11);
  pidMapInsert(12, 12);
  pidMapInsert(13, 13);
  pidMapInsert(14, 14);
  pidMapInsert(15, 15);
  // printTree(pidMap, 0);
  // kprintf("height = %u\n", pidMap->height);
  assert(pidMap->height <= 5);

  pidMapDelete(0);
  pidMapDelete(1);
  pidMapDelete(2);
  pidMapDelete(3);
  pidMapDelete(4);
  pidMapDelete(5);
  pidMapDelete(6);
  pidMapDelete(7);
  assert(pidMap->height <= 4);
  assertEquals(pidMapLookup(8, &value), OK);
  assertEquals(value, 8);
  assertEquals(pidMapLookup(10, &value), OK);
  assertEquals(value, 10);
  assertEquals(pidMapLookup(12, &value), OK);
  assertEquals(value, 12);
  assertEquals(pidMapLookup(14, &value), OK);
  assertEquals(value, 14);
  assertEquals(pidMapLookup(7, &value), 0);
  assertEquals(pidMapLookup(5, &value), 0);
  assertEquals(pidMapLookup(3, &value), 0);
  assertEquals(pidMapLookup(1, &value), 0);


  pidMapDelete(7);
  pidMapDelete(7);
  pidMapDelete(7);

  pidMapDelete(8);
  pidMapDelete(9);
  pidMapDelete(10);
  pidMapDelete(11);
  pidMapDelete(12);
  pidMapDelete(13);
  pidMapDelete(14);
  pidMapDelete(15);
  assertEquals(pidMap, NULL);
}

#define SEND_BUFFER_SIZE 0x200
#define RECV_BUFFER_SIZE 0x100
#define TEST_MSG "Yo mama so fat, Obi-Wan Kenobi said \"Thats no moon.... thats yo mama!\""

void test_send_recv_fail(void) {
  char buffer[RECV_BUFFER_SIZE];
  unsigned int from_pid, bad_pid;

  bad_pid = 0xdeadbeef;
  // send to invalid PID
  assertEquals(syssend(bad_pid, buffer, RECV_BUFFER_SIZE), SYS_SR_NO_PID);
  test_print("Called syssend with invalid dest_pid, return code %d\n", SYS_SR_NO_PID);

  // self send
  assertEquals(syssend(sysgetpid(), buffer, RECV_BUFFER_SIZE), SYS_SR_SELF)
  test_print("Called syssend with dest_pid = sysgetpid(), return code %d\n", SYS_SR_SELF);

  // recv from invalid PID
  assertEquals(sysrecv(&bad_pid, buffer, RECV_BUFFER_SIZE), SYS_SR_NO_PID);
  test_print("Called sysrecv with invalid from_pid, return code %d\n", SYS_SR_NO_PID);

  // self send
  from_pid = sysgetpid();
  assertEquals(sysrecv(&from_pid, buffer, RECV_BUFFER_SIZE), SYS_SR_SELF);
  test_print("Called sysrecv with dest_pid = sysgetpid(), return code %d\n", SYS_SR_SELF);

}

void receiver_1(void) {
  char buffer[RECV_BUFFER_SIZE];
  unsigned int from_pid, ppid, pid;
  int num;

  pid = sysgetpid();
  from_pid = 0;
  assert(strcmp(buffer, TEST_MSG) != 0);
  test_print("Process %03u: Receiver buffer does not have test message \n", pid);

  test_print("Process %03u: Calling sysrecv\n", pid);
  num = sysrecv(&from_pid, buffer, RECV_BUFFER_SIZE);
  ppid = sysgetppid();
  assertEquals(from_pid, ppid);
  assertEquals(num, RECV_BUFFER_SIZE);
  test_print("Process %03u: sysrecv returns %d\n", pid, num);
  assert(strcmp(buffer, TEST_MSG) == 0);
  test_print("Process %03u: Receiver buffer has test message\n", pid);
}

void sender_1(void) {
  unsigned int pid, d_pid;
  int num;
  char buffer[SEND_BUFFER_SIZE];

  sprintf(buffer, TEST_MSG, 0);
  pid = sysgetpid();
  d_pid = syscreate(receiver_1, TEST_STACK_SIZE);

  // give time for receiving process to call sysrecv
  sysyield();
  sysyield();
  sysyield();

  test_print("Process %03u: Calling syssend\n", pid);
  num = syssend(d_pid, buffer, SEND_BUFFER_SIZE);
  assertEquals(num, RECV_BUFFER_SIZE);
  test_print("Process %03u: syssend returns %d\n", pid, num);
}

void receiver_2(void) {
  char buffer[RECV_BUFFER_SIZE];
  unsigned int from_pid, pid;
  int num;

  pid = sysgetpid();
  from_pid = sysgetppid();

  // give time for sending process to call syssend
  sysyield();
  sysyield();
  sysyield();

  test_print("Process %03u: Receiver buffer does not have test message \n", pid);

  test_print("Process %03u: Calling sysrecv\n", pid);
  num = sysrecv(&from_pid, buffer, RECV_BUFFER_SIZE);
  assertEquals(from_pid, sysgetppid());
  assertEquals(num, RECV_BUFFER_SIZE);
  test_print("Process %03u: sysrecv returns %d\n", pid, num);
  assert(strcmp(buffer, TEST_MSG) == 0);
  test_print("Process %03u: Receiver buffer has test message \n", pid);
}

void sender_2(void) {
  unsigned int pid;
  int num;
  char buffer[SEND_BUFFER_SIZE];

  pid = sysgetpid();
  sprintf(buffer, TEST_MSG, 0);
  test_print("Process %03u: Calling syssend\n", pid);
  pid = syscreate(receiver_2, TEST_STACK_SIZE);
  num = syssend(pid, buffer, SEND_BUFFER_SIZE);
  assertEquals(num, RECV_BUFFER_SIZE);
  test_print("Process %03u: syssend returns %d\n", pid, num);
}

#define NUM_SENDERS 3
void sender_3(void) {
  unsigned int pid, ppid;

  ppid = sysgetppid();
  pid = sysgetpid();

  test_print("Process %03u calling syssend with dest_pid = %03u\n", pid, ppid);
  syssend(ppid, &pid, sizeof(int));
}

void receiver_3(void) {
  int i, num;
  unsigned int pid, from_pid, word;
  unsigned int child[NUM_SENDERS];

  pid = sysgetpid();

  for (i = 0; i < NUM_SENDERS; i++) {
    child[i] = syscreate(sender_3, TEST_STACK_SIZE);
    // give time for sender to call syssend
    sysyield();
    sysyield();
    sysyield();
  }

  for (i = 0; i < NUM_SENDERS; i++) {
    from_pid = child[i];
    test_print("Process %03u calling sysrecv with from_pid = %03u\n", pid, from_pid);
    num = sysrecv(&from_pid, &word, sizeof(int));
    assertEquals(num, sizeof(int));
    assertEquals(word, from_pid);
    test_print("Process %03u received word %u from Process %03u\n", pid, word, from_pid);
  }

  for (i = 0; i < NUM_SENDERS; i++) {
    // Store pid in reversed order
    child[NUM_SENDERS - i - 1] = syscreate(sender_3, TEST_STACK_SIZE);

    // give time for sender to call syssend
    sysyield();
    sysyield();
    sysyield();
  }

  for (i = 0; i < NUM_SENDERS; i++) {
    from_pid = child[i];
    test_print("Process %03u calling sysrecv with from_pid = %03u\n", pid, from_pid);
    num = sysrecv(&from_pid, &word, sizeof(int));
    assertEquals(num, sizeof(int));
    assertEquals(word, from_pid);
    test_print("Process %03u received word %u from Process %03u\n", pid, word, from_pid);
  }
}

void receiver_4(void) {
  unsigned int pid, ppid;
  unsigned int word;

  ppid = sysgetppid();
  pid = sysgetpid();

  test_print("Process %03u calling sysrecv with from_pid = %03u\n", pid, ppid);
  sysrecv(&ppid, &word, sizeof(int));
  test_print("Process %03u received word %u from Process %03u\n", pid, word, ppid);
}

#define NUM_RECEIVERS 3
void sender_4(void) {
  int i, num;
  unsigned int pid, dest_pid, word;
  unsigned int child[NUM_RECEIVERS];

  pid = sysgetpid();

  for (i = 0; i < NUM_RECEIVERS; i++) {
    child[i] = syscreate(receiver_4, TEST_STACK_SIZE);
    // give time for receiver to call sysrecv
    sysyield();
    sysyield();
    sysyield();
  }

  for (i = 0; i < NUM_RECEIVERS; i++) {
    word =dest_pid = child[i];
    test_print("Process %03u calling syssend with dest_pid = message = %03u\n", pid, dest_pid);
    num = syssend(dest_pid, &word, sizeof(int));
    assertEquals(num, sizeof(int));
  }

  for (i = 0; i < NUM_RECEIVERS; i++) {
    // Store pid in reversed order
    child[NUM_RECEIVERS - i - 1] = syscreate(receiver_4, TEST_STACK_SIZE);

    // give time for receiver to call sysrecv
    sysyield();
    sysyield();
    sysyield();
  }

  for (i = 0; i < NUM_RECEIVERS; i++) {
    word = dest_pid = child[i];
    test_print("Process %03u calling syssend with dest_pid = message = %03u\n", pid, dest_pid);
    num = syssend(dest_pid, &word, sizeof(int));
    assertEquals(num, sizeof(int));
  }
}

void testSendReceive(void) {
  // Test bad syssend sysrecv
  test_print("Tests for send and receive failures:\n");
  create(test_send_recv_fail, TEST_STACK_SIZE, NULL);
  dispatch();

  // Test blocking sysrecv
  test_print("Test for single blocking sysrecv:\n");
  create(sender_1, TEST_STACK_SIZE, NULL);
  dispatch();

  // Test blocking syssend
  test_print("Test for single blocking syssend:\n");
  create(sender_2, TEST_STACK_SIZE, NULL);
  dispatch();

  // Test mulitple blocking syssend (sender queue)
  test_print("Test for receiving from multiple blocked senders:\n");
  create(receiver_3, TEST_STACK_SIZE, NULL);
  dispatch();

  // Test mulitple blocking sysrecv (receiver queue)
  test_print("Test for sending to multiple blocked receivers:\n");
  create(sender_4, TEST_STACK_SIZE, NULL);
  dispatch();

  // cleanup
  nextPid = 0;
}

#define NUM_SLEEP_P 6
void testSleepList(void) {
  pcb process[NUM_SLEEP_P], *p;
  int i;
  
  for (i = 0; i < NUM_SLEEP_P; i++) {
    process[i].pid = i+1;
  }

  sleep(process, 70);
  sleep(process+1, 20);
  sleep(process+5, 19);
  sleep(process+2, 40);
  sleep(process+3, 1);
  sleep(process+4, 119);
  // traverseSleepList();

  p = sleep_list;
  assertEquals(p, process+3);
  assertEquals(p->delta,1);
  p = p->next;
  assertEquals(p, process+1);
  assertEquals(p->delta,1);
  p = p->next;
  assertEquals(p, process+5);
  assertEquals(p->delta,0);
  p = p->next;
  assertEquals(p, process+2);
  assertEquals(p->delta,2);
  p = p->next;
  assertEquals(p, process);
  assertEquals(p->delta,3);
  p = p->next;
  assertEquals(p, process+4);
  assertEquals(p->delta,5);
  p = p->next;
  assertEquals(p, NULL);

  tick();

  p = sleep_list;
  assertEquals(p, process+1);
  assertEquals(p->delta,1);

  tick();

  p = sleep_list;
  assertEquals(p, process+2);
  assertEquals(p->delta,2);

  // clean up
  sleep_list = NULL;
  ready_queue = NULL;
  assert(next() == NULL);
}

static Bool preempted, awake;
static unsigned int ticks;

void preemptive(void) {
  unsigned int pid = sysgetpid();
  char str[0x100];

  // let other process enter while loop
  sysyield();
  sysyield();
  sysyield();

  sprintf(str, "Process %03u set global variable to True\n", pid);
  test_puts(str);
  preempted = TRUE;
}

void gets_preempted(void) {
  unsigned int pid = sysgetpid();
  char str[0x100];

  syscreate(preemptive, TEST_STACK_SIZE);

  sprintf(str, "Process %03u set global variable to False\n", pid);
  test_puts(str);

  preempted = FALSE;
  // this process does not yield; must be preempted
  sprintf(str, "Process %03u entering loop with condition (global variable == False)\n", pid);
  test_puts(str);

  while (!preempted);
  
  sprintf(str, "Process %03u was preempted, exiting\n", pid);
  test_puts(str);
}

void idling(void) {
  char str[0x100];

  sprintf(str, "Idling process gets to run\n", 0);
  test_puts(str);

  while (!awake) {
    ticks++;
    asm("hlt;\n":::);
  }
  sprintf(str, "Idling process looped %u times before exiting\n", ticks);
  test_puts(str);
}

void sleeping(void) {
  unsigned int pid, ms;
  char str[0x100];

  ms = 1000;
  pid = sysgetpid();
  sprintf(str, "Process %03u calling syssleep\n", pid);
  test_puts(str);

  assert(syssleep(ms) == 0);
  sprintf(str, "Process %03u is awoken\n", pid);
  test_puts(str);
  awake = TRUE;  
}

void testTimeSharing(void) {
  unsigned int pid, pcb_index;

  // Test process gets preempted
  test_print("Test for preempting a process:\n");
  preempted = FALSE;
  create(gets_preempted, TEST_STACK_SIZE, NULL);
  dispatch();
  assertEquals(preempted, TRUE);

  // Test process can sleep and get awaken
  test_print("Test for sleep device:\n");
  awake = FALSE;
  ticks = 0;
  create(sleeping, TEST_STACK_SIZE, NULL);

  // Create an idling process that exits on a global variable awake
  pid = create(idling, TEST_STACK_SIZE, NULL);
  pidMapLookup(pid, &pcb_index);
  idle = pcbTable + pcb_index;

  dispatch();
  assertEquals(awake, TRUE);
  assert(ticks);
}

void handler_exit(void *cntx) {
  char str[0x100];
  unsigned int me = sysgetpid();
  sprintf(str, "Process %03u received signal, calling sysstop now\n", me);
  sysputs(str);
  sysstop();
}

void handler_nothing(void *cntx) {
  char str[0x100];
  unsigned int me = sysgetpid();
  sprintf(str, "Process %03u received signal, does nothing\n", me);
  sysputs(str);
}

#define TEST_SIG 20
void test_syssighandler(void) {
  int rc;
  void* ptr;
  handler old;

  rc = syssighandler(-1, NULL, NULL);
  assertEquals(rc, -1);

  rc = syssighandler(32, NULL, NULL);
  assertEquals(rc, -1);

  rc = syssighandler(0, handler_exit, &old);
  assertEquals(rc, 0);
  assertEquals(old, NULL);

  rc = syssighandler(31, handler_exit, &old);
  assertEquals(rc, 0);
  assertEquals(old, NULL);

  ptr = kmalloc(sizeof(int) * 4);
  rc = syssighandler(TEST_SIG, (handler) ptr, &old);
  kfree(ptr);
  assertEquals(rc, -2);

  rc = syssighandler(TEST_SIG, handler_exit, &old);
  assertEquals(rc, 0);

  rc = syssighandler(TEST_SIG, NULL, &old);
  assertEquals(rc, 0);
  assertEquals(old, handler_exit);
}

void idle_wait_sig(void) {
  syssighandler(TEST_SIG, handler_exit, NULL);
  for(;;);
}

// This function registers a signal handler and loops waiting to be stopped
void loop_wait_sig(void) {
  unsigned int me, ppid;
  int rc;
  char str[0x100];

  me = sysgetpid();
  ppid = sysgetppid();

  // Set signal handler
  rc = syssighandler(TEST_SIG, handler_exit, NULL);
  assertEquals(rc, 0);
  sprintf(str, "Process %03u registered handler_exit for signal %d\n", me, TEST_SIG);
  sysputs(str);

  // Tell parent it's ready
  rc = syssend(ppid, NULL, 0);
  assertEquals(rc, 0);

  // Enter loop
  sprintf(str, "Process %03u entering infinite loop\n", me);
  sysputs(str);
  while(1);
  sprintf(str, "%s(%u): should not reach here\n", __func__, __LINE__);
  sysputs(str);
}

void interrupted_by_sig(void) {
  unsigned int ppid, me;
  int rc;
  char str[0x100];

  me = sysgetpid();
  ppid = sysgetppid();

  // Set signal handler
  rc = syssighandler(TEST_SIG, handler_nothing, NULL);
  assertEquals(rc, 0);
  sprintf(str, "Process %03u registered handler_nothing for signal %d\n", me, TEST_SIG);
  sysputs(str);

  // Tell parent it's ready
  rc = syssend(ppid, &me, sizeof(int));

  // Wait for parent to signal
  rc = sysrecv(&ppid, NULL, 0);
  assertEquals(rc, -129);
  sprintf(str, "Process %03u returns from sysrecv\n", me);
  sysputs(str);
}

void test_syssigkill(void) {
  int rc, msg;
  unsigned int pid, me, bg_pid;
  char str[0x100];

  // Create an idle process to prevent dispatch returning
  bg_pid = syscreate(idle_wait_sig, TEST_STACK_SIZE);

  // Try sending to an invalid PID
  me = sysgetpid();
  rc = syskill(0xdeadbeef, TEST_SIG);
  assertEquals(rc, -33);

  // Create a child process that only exits when receiving signal 20
  pid = syscreate(loop_wait_sig, TEST_STACK_SIZE);
  sprintf(str, "Process %03u created process %03d\n", me, pid);
  sysputs(str);

  // Wait for child process to set signal handler
  rc = sysrecv(&pid, NULL, 0);
  assertEquals(rc, 0);
  sysyield();
  sysyield();
  sysyield();
  sysyield();

  // Try sending an invalid signal
  rc = syskill(pid, -1);
  assertEquals(rc, -12);

  // Send signal 20 to child process
  sprintf(str, "Process %03u sending signal %d to process %03u\n",
      me, TEST_SIG, pid);
  sysputs(str);
  rc = syskill(pid, TEST_SIG);
  assertEquals(rc, 0);

  // Create a child process that blocks receiving, but it is not going to
  // receive any message because we will interrupt it with a signal
  pid = syscreate(interrupted_by_sig, TEST_STACK_SIZE);
  sprintf(str, "Process %03u created process %03d\n", me, pid);
  sysputs(str);

  // Give time for child process to set sighandler
  rc = sysrecv(&pid, &msg, sizeof(int));
  assertEquals(rc, sizeof(int));
  assertEquals(msg, pid);
  syssleep(1500);

  // Zap that blocked receiving retarded
  sprintf(str, "Process %03u sending signal %d to process %03u\n",
      me, TEST_SIG, pid);
  sysputs(str);
  rc = syskill(pid, TEST_SIG);
  assertEquals(rc, 0);


  rc = syskill(bg_pid, TEST_SIG);
  assertEquals(rc, 0);
}

void blocked_wait_sig(void) {
  unsigned int me, ppid;
  int rc;
  char str[0x100];

  me = sysgetpid();
  ppid = sysgetppid();

  // Set signal handler
  rc = syssighandler(TEST_SIG, handler_nothing, NULL);
  assertEquals(rc, 0);
  sprintf(str, "Process %03u registered handler_nothing for signal %d\n", me, TEST_SIG);
  sysputs(str);

  // Tell parent it's ready
  rc = syssend(ppid, &me, sizeof(int));
  assertEquals(rc, sizeof(int));

  // Wait for parent to signal
  sprintf(str, "Process %03u calling syssigwait\n", me);
  sysputs(str);
  rc = syssigwait();
  sprintf(str, "Process %03u returns from syssigwait\n", me);
  sysputs(str);
  assertEquals(rc, TEST_SIG);
}

void test_syssigwait(void) {
  int rc, msg;
  unsigned int pid, me, bg_pid;
  char str[0x100];

  // Create idle process to prevent dispatch returning
  bg_pid = syscreate(idle_wait_sig, TEST_STACK_SIZE); 
  me = sysgetpid();

  // Create a child process that does a sigwait before exiting
  pid = syscreate(blocked_wait_sig, TEST_STACK_SIZE);
  sprintf(str, "Process %03u created process %03u\n", me, pid);
  sysputs(str);

  // Wait for child process to set handler and call syssigwait
  rc = sysrecv(&pid, &msg, sizeof(int));
  assertEquals(rc, sizeof(int));
  syssleep(1000);

  // Send signal 20
  sprintf(str, "Process %03u sending signal %d to process %03u\n",
      me, TEST_SIG, pid);
  sysputs(str);
  rc = syskill(pid, TEST_SIG);
  assertEquals(rc, 0);

  rc = syskill(bg_pid, TEST_SIG);
  assertEquals(rc, 0);
  sysstop();
}

#define SIG_LO 0
#define SIG_MID 16
#define SIG_HI 31
void send_sig_lo(void* cntx) {
  unsigned int me, pid;
  int rc;

  me = sysgetpid();
  char str[0x100];
  sprintf(str, "Process %03d received signal %d, calling sysrecv\n", me, SIG_LO);
  sysputs(str);
  pid = sysgetppid();
  rc = sysrecv(&pid, NULL, 0);
  assertEquals(rc, -129);
  
  sprintf(str, "Process %03d sending kill signal to self\n", me);
  sysputs(str);
  syskill(me, TEST_SIG);
  sprintf(str, "Process %03d signal %d handler returning\n", me, SIG_LO);
  sysputs(str);
}
void send_sig_mid(void* cntx) {
  unsigned int me, pid;
  int rc;

  me = sysgetpid();
  char str[0x100];
  sprintf(str, "Process %03d received signal %d, calling sysrecv\n", me, SIG_MID);
  sysputs(str);
  pid = sysgetppid();
  rc = sysrecv(&pid, NULL, 0);
  assertEquals(rc, -129);
  sprintf(str, "Process %03d signal %d handler returning\n", me, SIG_MID);
  sysputs(str);
}
void send_sig_hi(void* cntx) {
  unsigned int ppid, me;
  int msg, rc;

  ppid = sysgetppid();
  me = sysgetpid();
  msg = SIG_HI;
  char str[0x100];
  sprintf(str, "Process %03d received signal %d, calling syssend to process %03d\n",
      me, SIG_HI, ppid);
  sysputs(str);
  rc = syssend(ppid, &msg, sizeof(int)); 
  assertEquals(rc, sizeof(int));
  sprintf(str, "Process %03d signal %d handler returning\n", me, SIG_HI);
  sysputs(str);
}

void stack_sigtramp(void) {
  int rc;
  unsigned int me, ppid;
  char str[0x100];

  me = sysgetpid();
  ppid = sysgetppid();

  // Setup signal handlers
  rc = syssighandler(TEST_SIG, handler_exit, NULL);
  assertEquals(rc, 0);
  rc = syssighandler(SIG_LO, send_sig_lo, NULL);
  assertEquals(rc, 0);
  rc = syssighandler(SIG_MID, send_sig_mid, NULL);
  assertEquals(rc, 0);
  rc = syssighandler(SIG_HI, send_sig_hi, NULL);
  assertEquals(rc, 0);
  sprintf(str, "Process %03u done setting signal handlers\n", me);
  sysputs(str);

  // Tell parent it's ready
  syssend(ppid, &me, sizeof(int));
  for(;;);
}

void test_stack_sigtramp(void) {
  unsigned int pid, me, bg_pid;
  int rc, msg;
  char str[0x100];

  me = sysgetpid();

  // Create idle process to prevent dispatch returning
  bg_pid = syscreate(idle_wait_sig, TEST_STACK_SIZE); 
  sprintf(str, "Process %03u created idle process %03d\n", me, bg_pid);
  sysputs(str);

  // Create a child process that accepts signals of different priorities
  pid = syscreate(stack_sigtramp, TEST_STACK_SIZE);
  sprintf(str, "Process %03u created process %03d\n", me, pid);
  sysputs(str);

  // Wait for child to set up handlers
  rc = sysrecv(&pid, &msg, sizeof(int));
  assertEquals(msg, pid);
  assertEquals(rc, sizeof(int));

  // Send signals in increasing priority so sigtramp would stack
  sprintf(str, "Process %03u sending signal %d to process %03d\n", me, SIG_LO, pid);
  sysputs(str);
  rc = syskill(pid, SIG_LO);
  assertEquals(rc, 0);
  syssleep(1000);
  sprintf(str, "Process %03u sending signal %d to process %03d\n", me, SIG_MID, pid);
  sysputs(str);
  rc = syskill(pid, SIG_MID);
  assertEquals(rc, 0);
  syssleep(1000);
  sprintf(str, "Process %03u sending signal %d to process %03d\n", me, SIG_HI, pid);
  sysputs(str);
  rc = syskill(pid, SIG_HI);
  assertEquals(rc, 0);

  // Receive message from child process handlers
  rc = sysrecv(&pid, &msg, sizeof(int));
  assertEquals(msg, SIG_HI);
  sprintf(str, "Process %03u received message from process %03d signal %d handler\n",
      me, pid, msg);
  sysputs(str);
  
  rc = syskill(bg_pid, TEST_SIG);
  sprintf(str, "Process %03u exiting\n", me);
  sysputs(str);
  

  // Give child processes some time to finish first
  syssleep(5000);
}

void test_signal(void) {
  // Test registering signal handlers
  test_print("Tests for syssighandler:\n");
  create(test_syssighandler, TEST_STACK_SIZE, NULL);
  dispatch();

  // Test syssigkill
  test_print("Tests for syssigkill:\n");
  create(test_syssigkill, TEST_STACK_SIZE, NULL);
  dispatch();

  // Test syssigwait
  test_print("Tests for syssigwait:\n");
  create(test_syssigwait, TEST_STACK_SIZE, NULL);
  dispatch();

  // Test stacking sigtramp
  test_print("Tests for stacking sigtramp:\n");
  create(test_stack_sigtramp, TEST_STACK_SIZE, NULL);
  dispatch();
}


/* END OF TEST CODE*/
#endif
