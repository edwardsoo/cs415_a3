#include <xeroskernel.h>
#include <i386.h>
#include <icu.h>
#include <kbd.h>
#include <stdarg.h>

#define KEYBOARD_BUF_LEN 4
#define DEFAULT_EOF 4

extern void set_evec(unsigned int xnum, unsigned long handler);
extern void	kputc(int, unsigned char);
static char kb_buf[KEYBOARD_BUF_LEN];
static int head = 0;
static int size = 0;
static __attribute__ ((used)) unsigned int ESP;
static proc_state ps;
static void copy(void);
static int insert(unsigned char c);
static int remove(unsigned char *c);
static unsigned int kbtoa( unsigned char code );

void enable_keyboard(void) {
  set_evec(IRQBASE + 1, (unsigned long) _KeyboardISREntryPoint);
}

int keyboard_open(pcb* p) {
  if (ps.pcb != NULL) {
    where();
    return -1;
  }
  ps.pcb = p;
  ps.buf = NULL;
  ps.buf_len = 0;
  ps.ch_read = 0;
  ps.eof = DEFAULT_EOF;
  ps.echo = 0;
  return 0;
}

int keyboard_close(pcb* p) {
  ps.pcb = NULL;
  return 0;
}

int keyboard_ioclt(pcb* p, unsigned long cmd, ...) {
  va_list ap;

  va_start(ap, cmd);
  ps.eof = va_arg(ap, int);
  va_end(ap);

  return 0;
}

int keyboard_write(pcb* p, void* buf, int buf_len) {
  return -1;
}

int keyboard_read(pcb* p, void* buf, int buf_len) {
  if (ps.pcb != p) {
    kprintf("Should not happen\n");
    abort();
  }
  
  // Process wants to read 1 or more, block process
  if (buf_len) {
    ps.buf = buf;
    ps.buf_len = buf_len;
    ps.ch_read = 0;
    p->state = READING;
  } else {
    p->irc = 0;
    ready(p);
    return 0;
  }
}

void copy() {
  unsigned char c, a;
  int rc;
  
  if (!ps.pcb) {
    // Print chars for testing
    while(remove(&c) == 0) {
      kputc(0, kbtoa(c));
      // kprintf("0x%x\n", c);
    }
    return;
  }

  do {
    rc = remove(&c);

    // buffer empty
    if (rc != 0) {
      return;
    }

    // Scan code to ASCII
    a = kbtoa(c);

    // Reach EOF
    if (a == ps.eof) {
      ps.pcb->irc = ps.ch_read;
      ready(ps.pcb);
      return;
    }

    ps.buf[ps.ch_read] = a;
    ps.ch_read++;
    if (a == '\n') {
      ps.pcb->irc = ps.ch_read;
      ready(ps.pcb);
      return;
    }
  } while (ps.ch_read < ps.buf_len);

  ps.pcb->irc = ps.ch_read;
  ready(ps.pcb);
}

void keyboard_lower() {
  unsigned char byte;
  int rc;

  rc = 0;
  byte = inb(0x64);
  // Read from keyboard controller while its buffer is not empty and 
  // the kernel buffer is not full
  while (byte & 1 && rc == 0) {
    byte = inb(0x60);
    rc = insert(byte);
    byte = inb(0x64);
  }

  // Let upper half feed buffer to process
  copy();

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

static int insert(unsigned char c) {
  if (size < KEYBOARD_BUF_LEN) {
    kb_buf[(head + size) % KEYBOARD_BUF_LEN] = c;
    size++;
    return 0;
  }
  return -1;
}

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

#define KEY_UP   0x80            /* If this bit is on then it is a key   */
/* up event instead of a key down event */

/* Control code */
#define LSHIFT  0x2a
#define RSHIFT  0x36
#define LMETA   0x38

#define LCTL    0x1d
#define CAPSL   0x3a


/* scan state flags */
#define INCTL           0x01    /* control key is down          */
#define INSHIFT         0x02    /* shift key is down            */
#define CAPSLOCK        0x04    /* caps lock mode               */
#define INMETA          0x08    /* meta (alt) key is down       */
#define EXTENDED        0x10    /* in extended character mode   */

#define EXTESC          0xe0    /* extended character escape    */
#define NOCHAR  256


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
