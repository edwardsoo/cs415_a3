/* syscall.c : syscalls
 */

#include <xeroskernel.h>
#include <stdarg.h>

/* Your code goes here */
static unsigned int x;
static inline void debugEsp(void);


int syscall(int call, ...) {
  int rc;
  va_list ap;

  va_start(ap, call);

  dprintf("%s(%u) syscall(0x%x)\n", __func__, __LINE__, syscall);
  dprintf("%s(%u) call(0x%x) = 0x%x\n", __func__, __LINE__, &call, call);
  dprintf("%s(%u) ptr(0x%x) = 0x%x\n", __func__, __LINE__, &ptr, ptr);

  // debugEsp();
  // Push vargs pointer and REQ_ID, then interrupt
  // When interrupt returns, move EAX value to memory and return it
  __asm __volatile(
      "push %2;\n"
      "push %1;\n"
      "int $" xstr(SYSCALL) ";\n"
      "movl %%eax, %0;\n"
      :"=m"(rc)
      :"m"(call), "m"(ap)
      :"%eax"
      );
  va_end(ap);
  return rc;
}

int syscreate(void (*func)(void), int stack) {
  return syscall(CREATE, func, stack);
}

void sysyield(void) {
  syscall(YIELD);
}

void sysstop(void) {
  syscall(STOP);
}

unsigned int sysgetpid(void) {
  return syscall(GET_PID);
}

unsigned int sysgetppid(void) {
  return syscall(GET_P_PID);
}

void sysputs(char *str) {
  syscall(PUTS, str);
}

int syssend( unsigned int dest_pid, void *buffer, int buffer_len) {
  return syscall(SEND, dest_pid, buffer, buffer_len);
}

int sysrecv( unsigned int *from_pid, void *buffer, int buffer_len ) {
  return syscall(RECV, from_pid, buffer, buffer_len);
}

unsigned int syssleep(unsigned int milliseconds) {
  return syscall(SLEEP, milliseconds);
}

int syssighandler(int signal, void (*newhandler)(void *), void (** oldHandler)(void *)) {
  return syscall(SIGHANDLER, newhandler, oldHandler);
}

// Experimental function to time a context switch by calling 
// a system call that does not do any work
unsigned long time_int(void) {
  unsigned long long ret, edx, ebx;
  unsigned  eax, ecx;

  // Call rdtsc and save values in other registers,
  // execute an interrupt with req = TIME_INT,
  // dispatcher will return control to this process immediately
  // call rdtsc again and do some math to get cycles elapsed
  __asm __volatile(
    "push $0;\n"
    "push $" xstr(TIME_INT) ";\n"
    "rdtsc;\n"
    "movl %%eax, %%ecx;\n"
    "movl %%edx, %%ebx;\n"
    "int $" xstr(SYSCALL) ";\n"
    "rdtsc;\n"
    "movl %%eax, %0;\n"
    "movl %%ecx, %1;\n"
    "movl %%edx, %2;\n"
    "movl %%ebx, %3;\n"
    "pop %%eax;\n"
    "pop %%eax;\n"
    :"=m"(eax), "=m"(ecx), "=m"(edx), "=m"(ebx)
    ::"%eax"
  );
  kprintf("eax(0x%x), ecx(0x%x), edx(0x%x), ebx(0x%x)\n", eax, ecx, edx, ebx);
  ret = (edx << 32 | eax) - (ebx << 32 | ecx);
  return ret;
}

static void inline debugEsp() {
  asm volatile (
    "movl %%esp, x;\n"
    :::"%eax"
  );
  kprintf("Current process ESP = 0x%x\n", x);
}
