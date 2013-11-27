#include <xeroskernel.h>
#include <i386.h>
#include <icu.h>
#include <kbd.h>
#include <stdarg.h>

#define KEYBOARD_BUF_LEN 4
#define DEFAULT_EOF 4

extern void set_evec(unsigned int xnum, unsigned long handler);
extern void enable_irq(unsigned int, int);
extern void	kputc(int, unsigned char);
static int buf_copy(void);
static int insert(unsigned char c);
static int remove(unsigned char *c);
static unsigned int kbtoa( unsigned char code );

// State variables
static char kb_buf[KEYBOARD_BUF_LEN];
static int head = 0;
static int size = 0;
static __attribute__ ((used)) unsigned int ESP;
static proc_state ps;

void set_keyboard_ISR(void) {
  set_evec(IRQBASE + 1, (unsigned long) _KeyboardISREntryPoint);
}

int _keyboard_open(pcb *p) {
  // Device has already been opened
  if (ps.pcb != NULL) {
    return DRV_ERROR;
  }

  // set driver state
  ps.pcb = p;
  ps.buf = NULL;
  ps.buf_len = 0;
  ps.ch_read = 0;
  ps.eof = DEFAULT_EOF;
  ps.status = 0;
  head = 0;
  size = 0;

  // enable keyboard interrupt
  enable_irq(1,0);

  return DRV_DONE;
}

int keyboard_open(pcb* p) {
  if (_keyboard_open(p) == DRV_DONE) {
    ps.echo = 0;
    return DRV_DONE;
  }
  return DRV_ERROR;
}

int keyboard_open_echo(pcb* p) {
  if (_keyboard_open(p) == DRV_DONE) {
    ps.echo = 1;
    return DRV_DONE;
  }
  return DRV_ERROR;
}

int keyboard_close(pcb* p) {
  // reset driver state
  ps.pcb = NULL;
  ps.buf = NULL;
  ps.buf_len = 0;
  ps.ch_read = 0;
  ps.eof = DEFAULT_EOF;
  ps.echo = 0;
  ps.status = 0;
  head = 0;
  size = 0;

  // disable keyboard interrupt
  enable_irq(1,1);

  return DRV_DONE;
}

int keyboard_ioclt(pcb* p, unsigned long cmd, ...) {
  va_list k_ap, p_ap;

  if (cmd == 53) {
    va_start(k_ap, cmd);
    p_ap = va_arg(k_ap, va_list);
    ps.eof = va_arg(p_ap, int);
    va_end(k_ap);
    return DRV_DONE;
  } else {
    return DRV_ERROR;
  }
}

int keyboard_write(pcb* p, void* buf, int buf_len) {
  return DRV_ERROR;
}

int keyboard_read(pcb* p, void* buf, int buf_len) {
  if (ps.pcb != p) {
    kprintf("Should not happen\n");
    abort();
  }
  
  // Process wants to read, EOF not reached 
  // but kernel buffer size is less than request length
  if (!(ps.status & EOF_REACHED) && size < buf_len) {
    ps.buf = buf;
    ps.buf_len = buf_len;
    ps.ch_read = 0;
    p->state = READING;
    return DRV_BLOCK;

  // Buffer has more characters than the request length
  } else if (size >= buf_len) {
    if (buf_copy() == DRV_DONE) {
      return DRV_DONE;
    } else {
      where();
      abort();
      return DRV_BLOCK;
    }

  } else {
    p->irc = 0;
    return DRV_DONE;
  }
}

int buf_copy() {
  unsigned char a;
  int rc;
  
  if (!ps.pcb) {
    where();
    abort();  
  }

  // Copy bytes from character buffer to process buffer
  // Loop until buffer empty or read request met unblock condition
  do {
    rc = remove(&a);

    // buffer empty
    if (rc != 0) {
      return DRV_BLOCK;
    }

    // Reach EOF
    if (a == ps.eof) {
      ps.status |= EOF_REACHED;
      goto read_done;
    }

    ps.buf[ps.ch_read] = a;
    ps.ch_read++;
    if (a == '\n') {
      goto read_done;
    }

  } while (ps.ch_read < ps.buf_len);

  read_done:
  ps.pcb->irc = ps.ch_read;
  return DRV_DONE;
}

void keyboard_lower() {
  unsigned char byte, a;
  int rc;

  rc = 0;
  byte = inb(0x64);
  // Read from keyboard controller while its buffer is not empty and 
  // the kernel buffer is not full
  while (byte & 1 && rc == 0) {
    byte = inb(0x60);
    // Scan code to ASCII
    a = kbtoa(byte);
    if (a && a != NOCHAR) {
      rc = insert(a);
      // Echo
      if (ps.echo) {
        kputc(0, a);
      }
    }
    byte = inb(0x64);
  }

  // Let upper half feed buffer to process
  if (buf_copy() == DRV_DONE) {
    ready(ps.pcb);
  }

  // signal APIC end of interrupt
  outb(ICU1, EOI); 

  asm volatile(
    "movl ESP, %%esp;\n"
    "popa;\n"
    "iret;\n"
    :::"%eax"
  );
}

void KeyboardISREntryPoint(void) {
  asm volatile(
  "_KeyboardISREntryPoint:\n"
    "cli;\n"
    "pusha;\n"
    "movl %%esp, ESP;\n"
  :::"%eax");
  keyboard_lower();
}


// Circular queue insert
static int insert(unsigned char c) {
  if (size < KEYBOARD_BUF_LEN) {
    kb_buf[(head + size) % KEYBOARD_BUF_LEN] = c;
    size++;
    return 0;
  }
  return -1;
}
// Circular queue remove
static int remove(unsigned char *c) {
  if (size > 0) {
    *c = kb_buf[head++];
    head %= KEYBOARD_BUF_LEN;
    size--;
    return 0;
  }
  return -1;
}


/***********************************
 * Copied from scancodesToAscii.txt 
 ***********************************/

static  int     state; /* the state of the keyboard */

/*  Normal table to translate scan code  */
unsigned char   kbcode[] = { 0,
  27,  '1',  '2',  '3',  '4',  '5',  '6',  '7',  '8',  '9',
  '0',  '-',  '=', '\b', '\t',  'q',  'w',  'e',  'r',  't',
  'y',  'u',  'i',  'o',  'p',  '[',  ']', '\n',    0,  'a',
  's',  'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';', '\'',
  '`',    0, '\\',  'z',  'x',  'c',  'v',  'b',  'n',  'm',
  ',',  '.',  '/',    0,    0,    0,  ' ' };

/* captialized ascii code table to tranlate scan code */
unsigned char   kbshift[] = { 0,
  0,  '!',  '@',  '#',  '$',  '%',  '^',  '&',  '*',  '(',
  ')',  '_',  '+', '\b', '\t',  'Q',  'W',  'E',  'R',  'T',
  'Y',  'U',  'I',  'O',  'P',  '{',  '}', '\n',    0,  'A',
  'S',  'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',  '"',
  '~',    0,  '|',  'Z',  'X',  'C',  'V',  'B',  'N',  'M',
  '<',  '>',  '?',    0,    0,    0,  ' ' };
/* extended ascii code table to translate scan code */
unsigned char   kbctl[] = { 0,
  0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
  0,   31,    0, '\b', '\t',   17,   23,    5,   18,   20,
  25,   21,    9,   15,   16,   27,   29, '\n',    0,    1,
  19,    4,    6,    7,    8,   10,   11,   12,    0,    0,
  0,    0,   28,   26,   24,    3,   22,    2,   14,   13 };


static unsigned int extchar( unsigned char code )
{
  state &= ~EXTENDED;
  return NOCHAR;
}

unsigned int kbtoa( unsigned char code )
{
  unsigned int    ch;

  if (state & EXTENDED) {
    kprintf("Don't know what is an extended key\n");
    return extchar(code);
  }
  if (code & KEY_UP) {
    switch (code & 0x7f) {
      case LSHIFT:
      case RSHIFT:
        state &= ~INSHIFT;
        break;
      case CAPSL:
        // state &= ~CAPSLOCK;
        state = (state & CAPSLOCK) ? (state & ~CAPSLOCK) : (state | CAPSLOCK);
        break;
      case LCTL:
        state &= ~INCTL;
        break;
      case LMETA:
        state &= ~INMETA;
        break;
    }

    return NOCHAR;
  }


  /* check for special keys */
  switch (code) {
    case LSHIFT:
    case RSHIFT:
      state |= INSHIFT;
      return NOCHAR;
    case CAPSL:
      // state |= CAPSLOCK;
      return NOCHAR;
    case LCTL:
      state |= INCTL;
      return NOCHAR;
    case LMETA:
      state |= INMETA;
      return NOCHAR;
    case EXTESC:
      state |= EXTENDED;
      return NOCHAR;
  }

  ch = NOCHAR;

  if (code < sizeof(kbcode)){
    if ( state & CAPSLOCK )
      ch = kbshift[code];
    else
      ch = kbcode[code];
  }
  if (state & INSHIFT) {
    if (code >= sizeof(kbshift))
      return NOCHAR;
    if ( state & CAPSLOCK )
      ch = kbcode[code];
    else
      ch = kbshift[code];
  }
  if (state & INCTL) {
    if (code >= sizeof(kbctl))
      return NOCHAR;
    ch = kbctl[code];
  }
  if (state & INMETA)
    ch += 0x80;
  return ch;
}
