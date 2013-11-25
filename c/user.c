/* user.c : User processes
 */

#include <xeroskernel.h>

/* Your code goes here */
extern int sprintf(char *str, char *fmt, int args);

void consumer(void) {
  unsigned int self, from_pid, milliseconds;
  char str[0x100], *printout;

  // Set printout prefix
  self = sysgetpid();
  sprintf(str, "Process %03u: ", self);
  printout = str + 13;

  sprintf(printout, "is alive\n", 0);
  sysputs(str);

  syssleep(5000);

  // Receive from parent process
  from_pid = sysgetppid();
  sysrecv(&from_pid, &milliseconds, sizeof(int));
  sprintf(printout, "received message, sleep for %u milliseconds\n", milliseconds);
  sysputs(str);

  syssleep(milliseconds);
  sprintf(printout, "awoken, going to exit\n", 0);
  sysputs(str);
}

#define NUM_CHILDREN 4
void root() {
  sysputs("root process, does nothing lol\n");
  for(;;);
}

void old_root(void) {
  unsigned int child[NUM_CHILDREN], self, milliseconds;
  int i, rc;
  char str[0x100], *printout;

  self = sysgetpid();
  sprintf(str, "Process %03u: ", self);
  printout = str + 13;

  // Set printout prefix
  sprintf(printout, "is alive\n", 0);
  sysputs(str);

  for (i = 0; i < NUM_CHILDREN; i++) {
    child[i] = syscreate(consumer, 0x2000);
  }

  syssleep(4000);

  milliseconds = 10000;
  syssend(child[2], &milliseconds, sizeof(int));

  milliseconds = 7000;
  syssend(child[1], &milliseconds, sizeof(int));

  milliseconds = 20000;
  syssend(child[0], &milliseconds, sizeof(int));

  milliseconds = 27000;
  syssend(child[3], &milliseconds, sizeof(int));

  rc = sysrecv(child+3, &milliseconds, sizeof(int));
  sprintf(printout, "sysrecv returned %d\n", rc);
  sysputs(str);

  rc = syssend(child[2], &milliseconds, sizeof(int));
  sprintf(printout, "syssend returned %d\n", rc);
  sysputs(str);
}

void semaphore(void) {
  unsigned int lock_holder;
  //char str[0x100];

  while (TRUE) {
    lock_holder = 0;
    sysrecv(&lock_holder, NULL, 0);
    //sprintf(str, "process %u got semaphore\n", lock_holder);
    // sysputs(str);
    syssend(lock_holder, NULL, 0);
    //sprintf(str, "process %u released semaphore\n", lock_holder);
    //sysputs(str);
  }
}

void P(unsigned int sem_pid) {
  syssend(sem_pid, NULL, 0);
}

void V(unsigned int sem_pid) {
  sysrecv(&sem_pid, NULL, 0);
}

#define INNER_LOOP 1000
#define OUTER_LOOK 100
unsigned int count;
void sem_a(void) {
  unsigned int ppid, sem_pid, i, j;
  // char str[0x100];

  ppid = 0;
  sysgetpid();
  sysrecv(&ppid, &sem_pid, sizeof(int));
  for (i = 0; i < INNER_LOOP; i ++) {
    //sprintf(str, "process %u wants semaphore\n", pid);
    //sysputs(str);
    P(sem_pid);
    for (j = 0; j < INNER_LOOP; j++)
      count++;
    //sprintf(str, "process %u releases semaphore\n", pid);
    //sysputs(str);
    V(sem_pid);
  }
  syssend(ppid, NULL, 0);
}

void sem_b(void) {
  unsigned int ppid, sem_pid, i, j;
  //char str[0x100];

  ppid = 0;
  sysgetpid();
  sysrecv(&ppid, &sem_pid, sizeof(int));
  for (i = 0; i < INNER_LOOP; i ++) {
    //sprintf(str, "process %u wants semaphore\n", pid);
    //sysputs(str);
    P(sem_pid);
    for (j = 0; j < INNER_LOOP; j++)
      count++;
    //sprintf(str, "process %u releases semaphore\n", pid);
    //sysputs(str);
    V(sem_pid);
  }
  syssend(ppid, NULL, 0);
}

void a(void) {
  unsigned int i, j;

  for (i = 0; i < INNER_LOOP; i ++) {
    for (j = 0; j < INNER_LOOP; j++)
      count++;
  }
  syssend(sysgetppid(), NULL, 0);
}

void b(void) {
  unsigned int i, j;

  for (i = 0; i < INNER_LOOP; i ++) {
    for (j = 0; j < INNER_LOOP; j++)
      count++;
  }
  syssend(sysgetppid(), NULL, 0);
}

void semaphore_root(void) {
  unsigned int sem_pid, a_pid, b_pid;

  // No critical section protection
  count = 0;
  a_pid = syscreate(a, 0x2000);  
  b_pid = syscreate(b, 0x2000);
  sysrecv(&a_pid, NULL, 0);
  sysrecv(&b_pid, NULL, 0);
  kprintf("unprotected count = %u\n", count);
  
  // Create semaphore and crit. section protected process
  count = 0;
  sem_pid = syscreate(semaphore, 0x2000);
  a_pid = syscreate(sem_a, 0x2000);  
  b_pid = syscreate(sem_b, 0x2000);
  syssend(a_pid, &sem_pid, sizeof(int));
  syssend(b_pid, &sem_pid, sizeof(int));
  sysrecv(&a_pid, NULL, 0);
  sysrecv(&b_pid, NULL, 0);
  kprintf("semaphore protected count = %u\n", count);

  for(;;);
}
