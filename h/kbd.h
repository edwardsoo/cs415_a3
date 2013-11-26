#define KEYBOARD_BUF_LEN 4
typedef struct _proc_state {
  pcb* pcb;
  unsigned char *buf;
  int buf_len;
  int ch_read;
  int eof;
  int echo;
} proc_state;

int keyboard_open(pcb* p);
int keyboard_close(pcb* p);
int keyboard_ioclt(pcb* p, unsigned long cmd, va_list ap);
int keyboard_write(pcb* p, void* buf, int buf_len);
int keyboard_read(pcb* p, void* buf, int buf_len);
void keyboard_lower(void);


void _KeyboardISREntryPoint(void);
