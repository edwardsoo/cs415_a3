/* disp.c : dispatcher
 */

#include <xeroskernel.h>
#include <stdarg.h>

/* Your code goes here */

extern int contextswitch(pcb* );
extern int create (void (*func)(void), int stack, unsigned int parent);
extern void end_of_intr(void);
extern long freemem;
extern void deliver_signal(pcb* p);
extern int signal(unsigned int pid, int sig_no);
extern void register_sig_handler(pcb* p, int signal, handler, handler*);
const char* syscall_str[] = {
  "TIME_INT", "CREATE", "YIELD", "STOP", "GET_PID", "GET_P_PID", "PUTS",
  "SEND", "RECV", "SYS_TIMER", "SLEEP", "SIGHANDLER", "SIGRETURN", "KILL",
  "SIGWAIT", "OPEN"
};

void cleanup(pcb* p);
static open_device(pcb* p, unsigned int major_no);

// Some process management variables
pcb pcbTable[MAX_NUM_PROCESS];
pcb *idle = NULL;
pcb *ready_queue;
treeNode *pidMap = NULL;
unsigned int nextPid;

void idleproc(void) {
  while (TRUE) {
    asm("hlt;\n":::);
  }
}


void dispatch(void) {
  int rc, pid, sig_no;
  va_list ap;
  unsigned int dest_pid, *from_pid;
  request_type request = SYS_TIMER;
  pcb *p, *to_ready;
  signal_frame *sig_frame;
  funcptr fp;
  handler new_h, *old_h;

  for (p = next(); p != NULL;) {
    to_ready = NULL;
    deliver_signal(p);
    p->state = RUNNING;
    request = contextswitch(p);
    handle_request:
    ap = (va_list) p->iargs;
    switch (request) {
      case (CREATE):
        fp = (funcptr) va_arg(ap, int);
        pid = create(fp, va_arg(ap, int), p->pid);
        to_ready = p;
        p->irc = pid; 
        break;
      case YIELD:
        to_ready = p;
        break;
      case STOP:
        cleanup(p);
        break;
      case GET_PID:
        p->irc = p->pid;
        to_ready = p;
        break;
      case GET_P_PID:
        p->irc = p->parentPid;
        to_ready = p;
        break;
      case PUTS:
        kprintf((char*) va_arg(ap, int));
        to_ready = p;
        break;
      case SEND:
        dest_pid = (unsigned int) va_arg(ap, unsigned int);
        send(p, dest_pid);
        break;
      case RECV:
        from_pid = (unsigned int*) va_arg(ap, unsigned int);
        receive(p, from_pid);
        break;
      case SYS_TIMER:
        to_ready = p;
        tick();
        end_of_intr();
        break;
      case SLEEP:
        sleep(p, (unsigned int) va_arg(ap, int));
        break;
      case TIME_INT:
        request = contextswitch(p);
        goto  handle_request;
        break;
      case SIGHANDLER:
        sig_no = va_arg(ap, int);
        new_h = (handler) va_arg(ap, int);
        old_h = (handler*) va_arg(ap, int);
        register_sig_handler(p, sig_no, new_h, old_h);
        to_ready = p;
        break;
      case SIGRETURN:
        p->esp = (unsigned int) va_arg(ap, int);
        sig_frame = (signal_frame*) (p->esp - sizeof(signal_frame));
        p->hi_sig = sig_frame->old_hi_sig;
        p->irc = sig_frame->old_irc; 
        contextFrame *cntx = (contextFrame*) p->esp;
        // kprintf("PID %d sigreturn old_sp:0x%x, old_eip 0x%x, old_hi_sig:0x%x, old_irc:0x%x\n",
        //     p->pid, p->esp, cntx->iret_eip, p->hi_sig, p->irc);
        to_ready = p;
        break;
      case KILL:
        dest_pid = (unsigned int) va_arg(ap, int);
        sig_no = va_arg(ap, int);
        // kprintf("pid %u syskill pid %u\n", p->pid, dest_pid);
        rc = signal(dest_pid, sig_no);
        if (rc == -1) {
          p->irc = -33;
        } else if (rc == -2) {
          p->irc = -12;
        } else {
          p->irc = 0;
        }
        to_ready = p;
        break;
      case SIGWAIT:
        p->state = WAITING;
        break;
      case OPEN:
        open_device(p, va_arg(ap, int));
        to_ready = p;
        break;
      default:
        break;
    }

    if (to_ready) {
      ready(to_ready);
    }

    p = next();

    // Try to not run the idle process if there are others in queue
    if (idle != NULL && p == idle) {
      p = next();
      if (p == NULL) {
        p = idle;
      } else {
        ready(idle);
      }
    } else if (p == NULL) {
      kprintf("Ready queue empty, dispatch() returning\n");
    }
  }
}

/* PID to PCB indx lookup map functions */
static int balanceFactor(treeNode*);
static void updateHeight(treeNode*);
static treeNode* rotateLeft(treeNode*);
static treeNode* rotateRight(treeNode*node);
static treeNode* insert(treeNode*, unsigned int, unsigned int);
static int lookup(treeNode*, unsigned int, unsigned int *);
static treeNode* delete(treeNode*, unsigned int);

void pidMapInsert(unsigned int pid, unsigned int pcbIndex) {
  pidMap = insert(pidMap, pid, pcbIndex);
}

void pidMapDelete(unsigned int pid) {
  pidMap = delete(pidMap, pid);
}

int pidMapLookup(unsigned int pid, unsigned int *pcbIndex) {
  return lookup(pidMap, pid, pcbIndex);
}


/* PCB queues, init, and cleanup functions */
void init_pcb_table(void) {
  unsigned int i;

  for (i = 0; i < MAX_NUM_PROCESS; i++) {
    pcbTable[i].pid = 0;
    pcbTable[i].state = STOPPED;
    pcbTable[i].next = pcbTable[i].senders = pcbTable[i].receivers = NULL;
  }
  nextPid = 1;
  ready_queue = NULL;
  idle = NULL;
  pidMap = NULL;
}

/* Returns a PCB pointer  */
pcb *next(void) {
  pcb *next = ready_queue;
  if (next) {
    ready_queue = next->next;
    next->next = NULL;
  }
  return next;
}

void ready(pcb* p) {
  pcb **end;

  end = &ready_queue;
  while(*end) {
    end = &((*end)->next);
  }
  *end = p;
  p->next = NULL;
  p->state = READY;
}

void cleanup(pcb* p) {
  pcb *sender, *receiver;

  // unblock all blocked trying to send to/ receive from this process
  while (p->senders) {
    sender = p->senders;
    p->senders = sender->next;
    sender->irc = SYS_SR_NO_PID;
    ready(sender);
  }
  while (p->receivers) {
    receiver = p->receivers;
    p->receivers = receiver->next;
    receiver->irc = SYS_SR_NO_PID;
    ready(receiver);
  }
  kfree(p->stack);
  p->next = NULL;
  p->stack = NULL;
  p->state = STOPPED;
  pidMapDelete(p->pid);
}

// Device functions
static open_device(pcb* p, unsigned int major_no) {

}

/******************************************************************************
  AVL tree implementations
 *****************************************************************************/
static int balanceFactor(treeNode *node) {
  unsigned int lH, rH;
  lH = node->left ? node->left->height : 0;
  rH = node->right ? node->right->height : 0;
  return lH - rH;
}

/* 
 * Update height of AVL tree node, leaf has height of 1
 * Assumes children heights are correct
 */
static void updateHeight(treeNode *node) {
  node->height = max(node->left ? node->left->height : 0,
      node->right ? node->right->height : 0) + 1;
}

static treeNode* rotateLeft(treeNode* node) {
  treeNode* ret = node->right;
  node->right = ret->left;
  ret->left = node;
  updateHeight(node);
  updateHeight(ret);
  return ret;
}

static treeNode* rotateRight(treeNode* node) {
  treeNode* ret = node->left;
  node->left = ret->right;
  updateHeight(node);
  updateHeight(ret);
  return ret;
}

/*
 * Inserts a node and balances the tree
 */
static treeNode* insert(treeNode* node, unsigned int pid, unsigned int pcbIndex) {
  treeNode *newNode;
  if (!node) {
    newNode = (treeNode*) kmalloc(sizeof(treeNode));
    if (!newNode) {
      kprintf("PID map insert failed\n");
      abort();
    }
    newNode->pid = pid;
    newNode->pcbIndex = pcbIndex;
    newNode->left = newNode->right = NULL;
    newNode->height = 1;
    node = newNode;
    return node;
  } else if (pid > node->pid) {
    node->right = insert(node->right, pid, pcbIndex);
    updateHeight(node);
  } else if (pid < node->pid) {
    node->left = insert(node->left, pid, pcbIndex);
    updateHeight(node);
  } else {
    node->pcbIndex = pcbIndex;
    return node;
  }

  /* Retore Balance */
  if (balanceFactor(node) > 1) {
    // New inserted in left subtree caused imbalance
    if (balanceFactor(node->left) < 0) {
      node->left = rotateLeft(node->left);
    }
    node = rotateRight(node);
  } else if (balanceFactor(node) < -1) {
    // New inserted in right subtree caused imbalance
    if (balanceFactor(node->right) > 0) {
      node->right = rotateRight(node->right);
    }
    node = rotateLeft(node);
  }
  return node; 
}

/*
 * Recursive binary tree lookup
 */
static int lookup(treeNode* node, unsigned int pid, unsigned int *pcbIndex) {
  if (!node) {
    return 0;
  } else if (node->pid == pid) {
    *pcbIndex = node->pcbIndex;
    return OK;
  } else if (pid > node->pid) {
    return lookup(node->right, pid, pcbIndex);
  } else {
    return lookup(node->left, pid, pcbIndex);
  }
}

/*
 * Returns the element with the largest key that is less then node's key in node's subtree
 */
treeNode* predecessor(treeNode* node) {
  treeNode *pred = node->left;
  while (pred && pred->right)
    pred = pred->right;
  return pred;
}

/*
 * Removes a node from tree 
 */
static treeNode* delete(treeNode* node, unsigned int pid) {
  treeNode *pred, *child;
  if (!node) {
    // PID not in tree, nothing to do
    return NULL;
  } else if (node->pid == pid) {
    if (node->left && node->right) {
      // Node has 2 children
      pred = predecessor(node);
      node->pid = pred->pid;
      node->pcbIndex = pred->pcbIndex;
      node->left = delete(node->left, pred->pid);
    } else {
      // Node has 1 child or no child, child has no children since tree is balanced
      child = node->left ? node->left : node->right;
      kfree(node);
      return child;
    }
  } else if (pid > node->pid) { 
    node->right = delete(node->right, pid);
  } else {
    node->left = delete(node->left, pid);
  }
  return node;
}
