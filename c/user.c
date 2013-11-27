/* user.c : User processes
 */

#include <xeroskernel.h>
#include <xeroslib.h>

/* Your code goes here */

#define NUM_CHILDREN 4
#define STR_SIZE 0x100
#define STACK_SIZE 0x2000
#define puts(F, ...) \
  c = kmalloc(STR_SIZE); \
sprintf(c, F, ##__VA_ARGS__); \
sysputs(c); \
kfree(c);

void print_called_1(void* cntx) {
  sysputs("Signal handler was called\n");
}

void print_called_2(void* cntx) {
  sysputs("Second signal handler was called\n");
}

void child_1(void) {
  unsigned int ppid, me;
  char *c;

  me = sysgetpid();
  ppid = sysgetppid();

  puts("12. Process %03d: Hello, I am going to sleep for 1 second\n", me);
  syssleep(1000);
  puts("Process %03d: sends signal 20 to process %03d\n", me, ppid);
  syskill(ppid, 20);
  puts("Process %03d: sends signal 18 to process %03d\n", me, ppid);
  syskill(ppid, 18);
}

void child_2(void) {
  unsigned int ppid, me;
  char *c;

  me = sysgetpid();
  ppid = sysgetppid();

  puts("15. Process %03d: Hello, I am going to sleep for 5 seconds\n", me);
  syssleep(5000);
  puts("Process %03d: sends signal 18 to process %03d\n", me, ppid);
  syskill(ppid, 18);
}

void child_3(void) {
  unsigned int ppid, me;
  char *c;

  me = sysgetpid();
  ppid = sysgetppid();

  puts("20. Process %03d: Hello, I am going to sleep for 5 seconds\n", me);
  syssleep(5000);
  puts("Process %03d: sends signal 20 to process %03d\n", me, ppid);
  syskill(ppid, 20);
}

void root() {
  unsigned int me;
  int fd, i, rc;
  char str[STR_SIZE + 1];
  handler old;
  char *c;

  me = sysgetpid();

  puts("1. Process %03d: Hello\n", me);

  puts("2. Process %03d: opens the echo keyboard\n", me);
  fd = sysopen(KEYBOARD_1);

  puts("3. Procsss %03d: reads characters, 1 at a time until 10 are read\n", me);
  for (i = 0; i < 10; i++) {
    sysread(fd, str, 1);
  }
  sysputs("\n");

  puts("4. Process %03d: attempts to open the no echo keyboard\n", me);
  sysopen(KEYBOARD_0);

  puts("5. Process %03d: attempts to open the echo keyboard\n", me);
  sysopen(KEYBOARD_1);

  puts("6. Process %03d: closes the echo keyboard\n", me);
  sysclose(fd);

  puts("7. Process %03d: opens the no echo keyboard\n", me);
  sysopen(KEYBOARD_0);

  puts("8. Process %03d: does 3 reads of 10 characters\n", me);
  for (i = 0; i < 3; i++) {
    rc = sysread(fd, str, 10);
    str[rc] = 0;
    sysputs(str);
    sysputs("\n");
  }

  puts("9. Process %03d: continues reading characters until an EOF indicator arrives\n", me);
  do {
    rc = sysread(fd, str, 1);
  } while (rc != 0);

  puts("10. Process %03d: closes the no echo keyboard\n", me);
  sysclose(fd);

  puts("10. Process %03d: opens the echo keyboard\n", me);
  fd = sysopen(KEYBOARD_1);

  puts("11. Process %03d: installs a signal handler for signal 18\n", me);
  syssighandler(18, print_called_1, NULL);

  syscreate(child_1, STACK_SIZE);

  puts("13. Process %03d: does another read\n", me);
  rc = sysread(fd, str, 1);
  puts("14. Process %03d: sysread returns %d\n", me, rc);

  syscreate(child_2, STACK_SIZE);

  puts("16. Process %03d: installs the second signal handler for signal 18\n", me);
  syssighandler(18, print_called_2, &old);

  puts("17. Process %03d: does another read\n", me);
  rc = sysread(fd, str, 1);
  puts("18. Process %03d: sysread returns %d\n", me, rc);

  puts("19. Process %03d: installs signal handler returned by last syssighandler for signal 20\n", me);
  syssighandler(20, old, NULL);

  syscreate(child_3, STACK_SIZE);

  puts("21. Process %03d: does another read\n", me);
  rc = sysread(fd, str, 1);
  puts("22. Process %03d: sysread returns %d\n", me, rc);

  puts("23. Process %03d: continues reading characters until an EOF indicator arrives\n", me);
  do {
    rc = sysread(fd, str, 1);
  } while (rc != 0);
  sysputs("\n");

  puts("24. Process %03d attempts to read again\n", me);
  rc = sysread(fd, str, 1);
  puts("24. Process %03d: sysread returns %d\n", me, rc);

  puts("25. Process %03d: Bye\n", me);

}

