/* header files */
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

/* functions */
void sig_catch(int sig);

/* main */
int main(void) {
  /* シグナルハンドラの設定 */
  if (SIG_ERR == signal(SIGABRT, sig_catch)) {
    fprintf(stderr, "error.\n");
    return EXIT_FAILURE;
  }

  /* シグナルを送信する */
  raise(SIGABRT);

  return EXIT_SUCCESS;
}

/** シグナルハンドラ */
void sig_catch(int sig) {
  printf("signal: %d\n", sig);
}

