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

// Kernel global defines
#define SYSCALL 80
#define xstr(exp) str(exp)
#define str(exp) #exp
#define MAX_NUM_PROCESS 256
#define FREEMEM_END 0x400000
#define SAFETY_MARGIN 0x40
#define NUM_SIGNAL 32
#define NUM_FD 4
#define KEYBOARD_0 0
#define KEYBOARD_1 KEYBOARD_0 + 1
#define NUM_DEVICE KEYBOARD_1 + 1

// IPC return codes
#define SYS_SR_NO_PID -1
#define SYS_SR_SELF -2
#define SYS_SR_ERR -3

// Driver to DII return codes
#define DRV_DONE   0
#define DRV_BLOCK  1
#define DRV_ERROR -1

// System timer init params
#define TIME_SLICE_MS 10
#define TIME_SLICE_DIV (1000/TIME_SLICE_MS)

// debug print toggle
#define DEBUG 0
#if DEBUG
#define dprintf(...) kprintf(__VA_ARGS__)
#else 
#define dprintf(...)
#endif

// test toggle
#define RUNTEST 0
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

// Signal number to integer with bit at position signo set
#define SIG_INT(sig) (1 << sig)

/* Test functions and macro */
#if TEST_VERBOSE
#define test_print(...) kprintf(__VA_ARGS__)
#define test_puts(S, F, ...) sprintf(S, F, ##__VA_ARGS__); \
  sysputs(S)
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
#define where(); \
  kprintf("%s(%u)\n", __func__, __LINE__);




// Memory block header
typedef struct _memHeader {
 // Size of the usable memory of this node
  unsigned long size;
 // Free node with the highest address that is lower than this
  struct _memHeader *prev;
 // Free node with the lowest address that is higher than this
  struct _memHeader *next;
 // Should be the same as dataStart when this node is allocated
  char *sanityCheck;
  unsigned char dataStart[0];
} memHeader;


/* Deivce independent interface struct*/
typedef struct _pcb pcb;
typedef struct _devsw {
  int (*dvopen)(pcb*);
  int (*dvclose)(pcb*);
  int (*dvread)(pcb*, void*, int);
  int (*dvwrite)(pcb*, void*, int);
  int (*dvioctl)(pcb*, unsigned long, ...);
} devsw;

/* Process Control Block */
struct _pcb {
  unsigned int pid; // Process ID
  unsigned int parentPid; // Parent process ID
  enum {
    STOPPED = 0, RUNNING, READY,
    /* all normal blocked state */
    SENDING, RECEIVING, SLEEPING, READING, WRITING,
    /* waiting is special */
    WAITING
  } state;
  // Next process in ready/send queue
  struct _pcb *next;
  // Head of queue of senders & receivers
  struct _pcb *senders ,*receivers;
  unsigned int esp;
  // start of process stack returned by malloc
  void *stack;
  // Interrupt return value holder
  int irc;
  // Interrupt arguments pointer
  unsigned int iargs;
  // Time slice left to be waken
  unsigned int delta;
  // signal handlers
  void (*sig_handler[32])(void*);
  unsigned int pending_sig;
  unsigned int allowed_sig;
  unsigned int hi_sig;
  // array of pointers to opened devices
  devsw* opened_dv[NUM_FD];
};

typedef void (*funcptr)(void);
typedef void (*handler)(void*);

/* Context switch and signal trampoline structs */
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

typedef struct _signal_frame {
  contextFrame new_cntx;
  unsigned int ret_addr;
  unsigned int handler;
  unsigned int cntx;
  unsigned int old_sp;
  unsigned int old_hi_sig;
  unsigned int old_irc;
} signal_frame;

/* PCB queues struct and functions */
extern pcb* next(void);
extern void ready(pcb*);

/* Memory functions */
extern void* kmalloc(int);
extern void kfree(void*);

/* System calls */
typedef enum {
  TIME_INT, CREATE, YIELD, STOP, GET_PID, GET_P_PID, PUTS, SEND, RECV,
  SYS_TIMER, SLEEP, SIGHANDLER, SIGRETURN, KILL, SIGWAIT, OPEN, CLOSE,
  WRITE, READ, IO_CTL
} request_type;
extern int syscreate(void (*func)(void), int stack);
extern void sysyield(void);
extern void sysstop(void);
extern unsigned int sysgetpid(void);
extern unsigned int sysgetppid(void);
extern void sysputs(char*);
extern int syssend( unsigned int dest_pid, void *buffer, int buffer_len);
extern int sysrecv( unsigned int *from_pid, void *buffer, int buffer_len );
extern unsigned int syssleep(unsigned int milliseconds);
extern void syssigreturn(void *old_sp);
extern int syssighandler(int signal, handler new_handler, handler* old_handler);
extern int syskill(unsigned int pid, int signal);
extern int syssigwait(void);
extern int sysopen(int device_no);
extern int sysclose(int fd);
extern int syswrite(int fd, void *buf, int buflen);
extern int sysread(int fd, void *buf, int buflen);
extern int sysioctl(int fd, unsigned long cmd, ...);

/* Inter-process communications */
extern void send(pcb* p, unsigned int dest_pid);
extern void receive(pcb* p, unsigned int *from_pid);

/* Sleep device */
extern void tick(void);
extern void sleep(pcb*, unsigned int);

/* Misc functions */
extern unsigned long time_int(void);
extern void root(void);
extern void semaphore_root(void);
void inline abort(void);
extern void print_ready_q(void);

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

/* Functions defined by startup code */
void bzero(void *base, int cnt);
void _bcopy(const void *src, void *dest, unsigned int n);
int kprintf(char * fmt, ...);
void lidt(void);
void init8259(void);
void disable(void);
void outb(unsigned int, unsigned char);
unsigned char inb(unsigned int);
