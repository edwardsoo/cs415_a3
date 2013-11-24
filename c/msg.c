/* msg.c : messaging system (assignment 2)
 */

#include <xeroskernel.h>
#include <stdarg.h>

/* Your code goes here */
extern pcb pcbTable[MAX_NUM_PROCESS];

static void send_receive_transfer(pcb*, pcb*);
static pcb* recv_queue_remove(pcb*, unsigned int);
static pcb* send_queue_remove(pcb*, unsigned int);
static void recv_queue_insert(pcb*, pcb*);
static void send_queue_insert(pcb*, pcb*);

/*
 * Sends a message to another process
 * Readies p and returns error code if target does not exist or is self
 * Readies p and returns bytes transferred if target is blocked receiving from p or any
 * Blocks p if target process is not blocked receiving
 */
void send(pcb* p, unsigned int dest_pid) {
  unsigned int dest_pcb_index;
  va_list ap;
  pcb *dest_p;
  unsigned int *src_pid;

  if (p->pid == dest_pid) {
    p->irc = SYS_SR_SELF;
    ready(p);
    return; 

  } else if (pidMapLookup(dest_pid, &dest_pcb_index) != OK) {
    p->irc = SYS_SR_NO_PID;
    ready(p);
    return; 
  }

  // Look for specified process in receiver queue
  dest_p = recv_queue_remove(p, dest_pid);
  if (dest_p) {
    send_receive_transfer(p, dest_p);
    ready(p);
    ready(dest_p);

  } else {
    // Destination process not in queue, check if it's blocked recv from any
    dest_p = pcbTable + dest_pcb_index;
    ap = (va_list) dest_p->iargs;
    src_pid = (unsigned int*) va_arg(ap, int);
    if (dest_p->state == READING && !(*src_pid)) {
      send_receive_transfer(p, dest_p);
      ready(p);
      ready(dest_p);
    } else {
      // Put process in destination process' sender queue
      send_queue_insert(pcbTable + dest_pcb_index, p);
    }
  }
}

/*
 * Receives a message from another process
 * Readies process and set returns error code if target does not exist or is self
 * Readies p and return bytes transferred if target is blocked sending to p
 * Blocks if target process is not blocked sending
 */
void receive(pcb* p, unsigned int *src_pid) {
  unsigned int src_pcb_index;
  pcb *src_p;

  if (p->pid == *src_pid) {
    p->irc = SYS_SR_SELF;
    ready(p);
    return;

  } else if (*src_pid && pidMapLookup(*src_pid, &src_pcb_index) != OK) {
    p->irc = SYS_SR_NO_PID;
    ready(p);
    return;
  }

  if (*src_pid) {
    // Look for specific process in sender queue
    src_p = send_queue_remove(p, *src_pid);
    if (src_p) {
      // Found specified sender in sender queue
      send_receive_transfer(src_p, p);
      ready(p);
      ready(src_p);
    } else {
      // specified process not found
      src_p = pcbTable + src_pcb_index;
      recv_queue_insert(src_p, p);
    }

  } else {
    // if src_pid not specified, accept any sender in sender queue
    if (p->senders) {
      // Accept first sender in queue
      src_p = p->senders;
      send_receive_transfer(src_p, p);
      p->senders = src_p->next;
      ready(p);
      ready(src_p);
    } else {
      // Block
      p->state = READING;
    }
  }
}

/*
 * Removes destination process from p's receiver queue
 */
pcb* recv_queue_remove(pcb *p, unsigned int dest_pid) {
  pcb **receiver, *dest_p;

  dest_p = NULL;
  receiver = &(p->receivers);
  // Traverse the receiver queue and try to match a process with specified PID
  while (*receiver) {
    if ((*receiver)->pid == dest_pid) {
      dest_p = *receiver;
      *receiver = dest_p->next;
      break;
    } else {
      receiver = &(*receiver)->next;
    }
  }
  return dest_p;
}

/*
 * Removes source process from p's sender queue
 */
pcb* send_queue_remove(pcb *p, unsigned int src_pid) {
  pcb **sender, *src_p;

  src_p = NULL;
  sender = &(p->senders);
  // Traverse the sender queue and try to match a process with specified PID
  while (*sender) {
    if ((*sender)->pid == src_pid) {
      src_p = *sender;
      *sender = src_p->next;
      break;
    } else {
      sender = &(*sender)->next;
    }
  }
  return src_p;
}

/*
 * Appends receiver to p's receiver queue
 */
void recv_queue_insert(pcb *p, pcb *receiver) {
  pcb **recv_q;

  recv_q = &(p->receivers);
  // Append process to end of receiver queue
  while (*recv_q) {
    recv_q = &(*recv_q)->next;
  }
  *recv_q = receiver;
  receiver->state = READING;
}

/*
 * Appends sender to p's sender queue
 */
void send_queue_insert(pcb *p, pcb *sender) {
  pcb **send_q;

  send_q = &(p->senders);
  // Append process to end of sender queue
  while (*send_q) {
    send_q = &(*send_q)->next;
  }
  *send_q = sender;
  sender->state = SENDING;
}

/*
 * Copy memory from sender's buffer to receiver's buffer,
 * sets the return values to the bytes transferred for both processes
 * and the sender PID for the receiving process
 */
void send_receive_transfer(pcb *src_p, pcb* dest_p) {
  va_list s_ap, r_ap;
  void *src_buf, *dest_buf;
  int len;
  unsigned int *src_pid;

  s_ap = (va_list) src_p->iargs;
  r_ap = (va_list) dest_p->iargs;
  va_arg(s_ap, int);
  src_pid = (unsigned int*) va_arg(r_ap, int);
  src_buf = (void*) va_arg(s_ap, int);
  dest_buf = (void*) va_arg(r_ap, int);

  // Transfer amount equals to the least of the two supplied lengths
  len = min(va_arg(s_ap, int), va_arg(r_ap, int));

  if (len) {
    _bcopy(src_buf, dest_buf, len);
  }

  // write the sender PID to the address supplied by receiving process
  *src_pid = src_p->pid;
  src_p->irc = dest_p->irc = len;
}
