#define KEYBOARD_BUF_LEN 4
#define EOF_REACHED 1
#define EOF_IN_BUF  2
typedef struct _proc_state {
  pcb* pcb;
  unsigned char *buf;
  int buf_len;
  int ch_read;
  int eof;
  int echo;
  int status;
} proc_state;

int keyboard_open(pcb* p);
int keyboard_open_echo(pcb* p);
int keyboard_close(pcb* p);
int keyboard_ioclt(pcb* p, unsigned long cmd, ...);
int keyboard_write(pcb* p, void* buf, int buf_len);
int keyboard_read(pcb* p, void* buf, int buf_len);
void keyboard_lower(void);
void _KeyboardISREntryPoint(void);


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
