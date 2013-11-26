/* ctsw.c : context switcher
 */

#include <xeroskernel.h>
#include <i386.h>

/* Your code goes here - You will need to write some assembly code. You must
   use the gnu conventions for specifying the instructions. (i.e this is the
   format used in class and on the slides.) You are not allowed to change the
   compiler/assembler options or issue directives to permit usage of Intel's
   assembly language conventions.
*/
static void __attribute__ ((used)) *kStack;
static unsigned int ESP;
static int rc, interrupt;

extern void set_evec(unsigned int xnum, unsigned long handler);
extern void initPIT( int divisor );
extern const char* syscall_str[];
void debugContextFrame(contextFrame*, unsigned int);

/* Declaration of _SyscallEntryPoint, which enter contextswitch midway */
void _SyscallEntryPoint(void);
void _SystimerISREntryPoint(void);

/* Setup the IDT to map interrupt SYSCALL to _SyscallEntryPoint */
void initSyscall(void) {
  set_evec(SYSCALL, (unsigned long) _SyscallEntryPoint);
}

void enableTimerInterrupt(void) {
  set_evec(IRQBASE, (unsigned long) _SystimerISREntryPoint);
  initPIT(TIME_SLICE_DIV);
}

/* 
* switch context between kernel and a user process
* @param p - a PCB representing the process which context will switch to
* @return - type of syscall when p returns context back to kernel by requesting service
*/
extern int contextswitch(pcb* p) {
  contextFrame *context;
  ESP = p->esp;

  // Set return value in process context %eax
  context = (contextFrame*) ESP;
  context->eax = p->irc;

  // Save kernel context to kernal stack,
  // swap CPU state and return to user process
  asm volatile(
    "pushf;\n"
    "pusha;\n"
    "movl %%esp, kStack;\n"
    "movl ESP, %%esp;\n"
    "popa;\n"
    "iret;\n"

  "_SystimerISREntryPoint:\n"
    "cli;\n"
    "pusha;\n"
    "movl $1, %%ecx;\n"
    "jmp _CommonJump;\n"
  "_SyscallEntryPoint:\n"
    "cli;\n"
    "pusha;\n"
    "movl $0, %%ecx;\n"
  "_CommonJump:\n"
    "movl %%esp, ESP;\n"
    "movl %%eax, rc;\n"
    "movl %%ecx, interrupt;\n"
    "movl kStack, %%esp;\n"
    "popa;\n"
    "popf;\n"
    :::"%eax","%ecx");

  p->esp = ESP;
  context = (contextFrame*) ESP;

  if (interrupt) {
    p->irc = rc;
    return SYS_TIMER;
  } else {
    // Put argument pointer in PCB for dispatcher
    p->iargs= context->args[1];
    return context->args[0];
  }
}
  
void debugContextFrame(contextFrame *context, unsigned int argc) {
  unsigned int i = 0;

  dprintf("eax 0x%x\n", context->eax);
  dprintf("ecx 0x%x\n", context->ecx);
  dprintf("edx 0x%x\n", context->edx);
  dprintf("ebx 0x%x\n", context->ebx);
  dprintf("esp 0x%x\n", context->esp);
  dprintf("ebp 0x%x\n", context->ebp);
  dprintf("esi 0x%x\n", context->esi);
  dprintf("edi 0x%x\n", context->edi);
  dprintf("eip 0x%x\n", context->iret_eip);
  dprintf("cs 0x%x\n", context->iret_cs);
  dprintf("eflags 0x%x\n", context->eflags);
  for (i = 0; i < argc; i++) {
    dprintf("arg[%u] 0x%x\n", i, context->args[i]);
  }
}
