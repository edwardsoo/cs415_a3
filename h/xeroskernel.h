/* xeroskernel.h - disable, enable, halt, restore, isodd, min, max */

/* Symbolic constants used throughout Xinu */

typedef	char		Bool;		/* Boolean type			*/
#define	FALSE		0		/* Boolean constants		*/
#define	TRUE		1
#define	EMPTY		(-1)		/* an illegal gpq		*/
#define	NULL		0		/* Null pointer for linked lists*/
#define	NULLCH		'\0'		/* The null character		*/


/* Universal return constants */

#define	OK		 1		/* system call ok		*/
#define	SYSERR		-1		/* system call failed		*/
#define	EOF		-2		/* End-of-file (usu. from read)	*/
#define	TIMEOUT		-3		/* time out  (usu. recvtim)	*/
#define	INTRMSG		-4		/* keyboard "intr" key pressed	*/
					/*  (usu. defined as ^B)	*/
#define	BLOCKERR	-5		/* non-blocking op would block	*/

// Context switch syscall request types
#define TIME_INT 0
#define CREATE TIME_INT + 1
#define YIELD  CREATE + 1
#define STOP   YIELD + 1
#define GET_PID STOP + 1
#define GET_P_PID GET_PID + 1
#define PUTS GET_P_PID + 1
#define SEND PUTS + 1
#define RECV SEND + 1
#define TIMER_INT RECV + 1
#define SLEEP TIMER_INT + 1
#define SIGHANDLER SLEEP + 1

// Kernel global defines
#define SYSCALL 80
#define xstr(exp) str(exp)
#define str(exp) #exp
#define MAX_NUM_PROCESS 256
#define FREEMEM_END 0x400000
#define SAFETY_MARGIN 0x40
#define NUM_SIGNAL 32

// debug print toggle
#define DEBUG 0
#if DEBUG
#define dprintf(...) kprintf(__VA_ARGS__)
#else 
#define dprintf(...)
#endif

// test toggle
#define RUNTEST 1
#define TEST_VERBOSE 1

// max/min
#define max(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })
#define min(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })

// Memory block header
typedef struct _memHeader {
  unsigned long size; // Size of the usable memory of this node
  struct _memHeader *prev; // Free node with the highest address that is lower than this
  struct _memHeader *next; // Free node with the lowest address that is higher than this
  char *sanityCheck; // Should be the same as dataStart when this node is allocated
  unsigned char dataStart[0];
} memHeader;

// Process Control Block
typedef struct _pcb {
  unsigned int pid; // Process ID
  unsigned int parentPid; // Parent process ID
  enum {STOPPED = 0, RUNNING, READY, SENDING, READING, SLEEPING} state;
  struct _pcb *next; // Next process in ready/send queue
  struct _pcb *senders ,*receivers; // Head of queue of senders & receivers
  unsigned int esp;
  void *stack; // start of process stack returned by malloc
  int irc; // Interrupt return value holder
  unsigned int iargs; // Interrupt arguments pointer
  unsigned int delta; // Time slice left to be waken
  void (*sig_handler[32])(void*); // signal handlers
} pcb;

typedef void (*funcptr)(void);
typedef void (*handler)(void*);

// Context Frame struct
typedef struct _contextFrame {
  unsigned int edi;
  unsigned int esi;
  unsigned int ebp;
  unsigned int esp;
  unsigned int ebx;
  unsigned int edx;
  unsigned int ecx;
  unsigned int eax;
  unsigned int iret_eip;
  unsigned int iret_cs;
  unsigned int eflags;
  unsigned int args[0];
} contextFrame;


/* PCB queues struct and functions */
typedef struct _pcbQueue {
  pcb *front, *back;
} pcbQueue;
extern pcb* next(void);
extern void ready(pcb*);

/* ASL tree struct and functions */
typedef struct _treeNode {
  unsigned int pid;
  unsigned int pcbIndex;
  struct _treeNode *left, *right;
  unsigned int height;
} treeNode;

void pidMapInsert(unsigned int pid, unsigned int pcbIndex);
int pidMapLookup(unsigned int pid, unsigned int *pcbIndex);
void pidMapDelete(unsigned int pid);

/* Memory functions */
extern void* kmalloc(int);
extern void kfree(void*);

/* System calls */
extern int syscreate(void (*func)(void), int stack);
extern void sysyield(void);
extern void sysstop(void);
extern unsigned int sysgetpid(void);
extern unsigned int sysgetppid(void);
extern void sysputs(char*);
extern int syssend( unsigned int dest_pid, void *buffer, int buffer_len);
extern int sysrecv( unsigned int *from_pid, void *buffer, int buffer_len );
extern unsigned int syssleep(unsigned int milliseconds);
int syssighandler(int signal, handler new_handler, handler* old_handler);

/* Messaging */
#define SYS_SR_NO_PID -1
#define SYS_SR_SELF -2
#define SYS_SR_ERR -3
extern void send(pcb* p, unsigned int dest_pid);
extern void receive(pcb* p, unsigned int *from_pid);

/* Sleep device */
#define TIME_SLICE_MS 10
#define TIME_SLICE_DIV (1000/TIME_SLICE_MS)
extern void tick(void);
extern void sleep(pcb*, unsigned int);

/* Signaling */
void sighandler(pcb* p, int signal, handler new_handler, handler* old_handler);

/* Misc functions */
extern unsigned long time_int(void);
extern void root(void);
extern void semaphore_root(void);
void inline abort(void);

/* Test functions and macro */
#if TEST_VERBOSE
#define test_print(...) kprintf(__VA_ARGS__)
#define test_puts(...) sysputs(__VA_ARGS__)
#else
#define test_print(...)
#define test_puts(...)
#endif

#define assertEquals(A, E); \
  if (E != A) {\
    kprintf("%s(%u): Assertion failed: actual 0x%x mismatch expected 0x%x\n", __func__, __LINE__, A, E);\
    for(;;);\
  }
#define assert(C); \
  if (!(C)) {\
    kprintf("%s(%u): Assertion failed: (%s) not true\n", __func__, __LINE__, xstr(C));\
    for(;;);\
  }
#define WHERE(); \
  kprintf("%s(%u)\n", __func__, __LINE__);


extern void testFreeList(void);
extern void testKmalloc(void);
extern void testContextSwitch(void);
extern void testProcessManagement(void);
extern void testPidMap(void);
extern void testSendReceive(void);
extern void testTimeSharing(void);
extern void testSleepList(void);
extern void test_signal(void);

/* Functions defined by startup code */
void bzero(void *base, int cnt);
void _bcopy(const void *src, void *dest, unsigned int n);
int kprintf(char * fmt, ...);
void lidt(void);
void init8259(void);
void disable(void);
void outb(unsigned int, unsigned char);
unsigned char inb(unsigned int);
