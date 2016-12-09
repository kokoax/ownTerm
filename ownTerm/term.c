#define _XOPEN_SOURCE 600

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <locale.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>

#include <stdint.h>

#define LINE_MAX 1000
#define history_term 10000
#define BUFFSIZE 512

typedef struct {
  int row;
  int col;
  int fw, fh;
  char line[LINE_MAX];
  char hist[history_term][LINE_MAX];
  int histi;
  int hist_max;
} Term;

typedef struct {
  int x, y, w, h;
  Window win;
  Display *dpy;
  GC gc;
  int scr;
  Visual *vis;
  XIM xim;
  XIC xic;
} XWindow;

static Term term;
static XWindow xw;
static int pty_master, pty_slave;

void ttynew(){
  char *pts_name;
  int   nread;
  char  buf[BUFFSIZE];
  pid_t pid;

  pty_master = posix_openpt(O_RDWR);
  grantpt(pty_master);
  unlockpt(pty_master);

  pts_name = ptsname(pty_master);

  if (fork() == 0) {
    setsid();

    pty_slave = open(pts_name, O_RDWR);
    close(pty_master);

    dup2(pty_slave, STDIN_FILENO);
    dup2(pty_slave, STDOUT_FILENO);
    dup2(pty_slave, STDERR_FILENO);
    close(pty_slave);
    execvp("/usr/bin/bash", NULL);
  }

  kill(pid, SIGTERM);
}

void ttyread(char buf[BUFFSIZE]){
  int n;
  if ((n = read(pty_master, buf, BUFFSIZE)) <= 0) buf[0] = '\0'; return;
  // if (write(STDOUT_FILENO, buf, n) != n) return;
}

void ttywrite( char *str, size_t n ){
  if (n < 0 || n == 0) return;
  if (write(pty_master, str, n) != n) return;
}

void ttysend( char *str, size_t n ){
  ttywrite(str, n);
}

void hist_push();
void hist_pop();

void line_clear( void );
void buff_clear( char* line );
void xinit( void );
void die( char *str );

int main( int argc, char **argv )
{

  int i, j;
  char shell[6] = "bash> ";

  KeySym keySym;
  char keybuf[BUFFSIZE];
  char readbuf[BUFFSIZE];
  char tmpbuf[BUFFSIZE];
  int len;
  XEvent event;
  Status status;

  setlocale(LC_CTYPE, "");
  xinit();

  ttynew();
  /* イベントループ。XtAppMainLoop() もこんなことをしている*/
  while( 1 ) {
    XNextEvent( xw.dpy, &event );  /* イベントキューから取り出す */
    switch( event.type ) {
      case Expose:        /* 再描画など */
        if( event.xexpose.count == 0 ) {
          // XDrawString( xw.dpy, xw.win, xw.gc, 0, term.row, term.line, strlen(term.line) );
          // XDrawString( xw.dpy, xw.win, xw.gc, 0, term.row, shell, strlen(shell) );
          ttyread(readbuf);
          write(STDOUT_FILENO, readbuf, strlen(readbuf));
          // printf( "%s", readbuf);
          XDrawString( xw.dpy, xw.win, xw.gc, 0, 13, readbuf, strlen(readbuf) );
        }
        break;
      case KeyPress:
        XClearWindow(xw.dpy, xw.win);

        len = XmbLookupString(xw.xic, (XKeyEvent*)&event.xkey, keybuf, sizeof keybuf, &keySym, &status);
        XDrawString( xw.dpy, xw.win, xw.gc, 0, 30, keybuf, strlen(keybuf) );

        switch(keySym){
          case XK_Return:
            // keybuf[0] = '\n';
            // keybuf[1] = '\0';
            keybuf[0] = '\r';
            keybuf[1] = '\n';
            keybuf[2] = '\0';
            break;
          case XK_Escape:
            return 1;
          default:
            keybuf[1] = '\0';
        }
        ttysend(keybuf, strlen(keybuf));
        ttyread(readbuf);
        // printf( "%s", readbuf);
        write(STDOUT_FILENO, readbuf, strlen(readbuf));
        // write(STDOUT_FILENO, "\n", strlen("\n"));

        XDrawString( xw.dpy, xw.win, xw.gc, 0, 13, readbuf, strlen(readbuf) );
        break;
    }
  }
}

void xinit( void ){
  xw.x = 0;
  xw.y = 0;
  xw.w = 1377;
  xw.h = 768;
  term.histi = 0;
  term.fw = 6;
  term.fh = 15;
  term.row = term.fh;
  term.col = term.fw;
  buff_clear(term.line);
  strcpy(term.line, "bash> ");

  setlocale( LC_CTYPE, "" );
  XSetLocaleModifiers("");

  XTextProperty win_name;
  char *quit_str = "Quit";

  /* サーバとの接続 */
  if( (xw.dpy = XOpenDisplay( NULL )) == NULL ) {
    fprintf( stderr, "Can't connecting X window system\n" );
    exit(1);
  }

  /* メインウィンドウの作成 */
  xw.win = XCreateSimpleWindow( xw.dpy, RootWindow(xw.dpy, 0), xw.x, xw.y, xw.w, xw.h, 1,
      /* 幅 高さ 境界線 */
      BlackPixel( xw.dpy, 0 ),
      WhitePixel( xw.dpy, 0 ) );

  win_name.value = "term";
  win_name.encoding = XA_STRING;
  win_name.format = 8;
  win_name.nitems = strlen( win_name.value );
  XSetWMName( xw.dpy, xw.win, &win_name );

  /* GC の作成 */
  xw.gc = XCreateGC( xw.dpy, xw.win, 0, 0 );

  /* 受け取るイベントをサーバに通知 */
  XSelectInput( xw.dpy, xw.win, ExposureMask | KeyPressMask ); // 画面全体
  /* input method event */
  if((xw.xim = XOpenIM(xw.dpy, NULL, NULL, NULL)) == NULL){
    XSetLocaleModifiers("@im=local");
    if((xw.xim = XOpenIM(xw.dpy, NULL, NULL, NULL)) == NULL){
      XSetLocaleModifiers("@im=");
      if((xw.xim = XOpenIM(xw.dpy, NULL, NULL, NULL)) == NULL){
        die( "XOpenIM failed" );
        // exit(-1);
      }
    }
  }

  xw.xic = XCreateIC(xw.xim, XNInputStyle, XIMPreeditNothing
      | XIMStatusNothing, XNClientWindow, xw.win,
      XNFocusWindow, xw.win, NULL );
  if( xw.xic == NULL ){
    die( "XCreateIC faild." );
  }

  /* ウィンドウのマップ */
  XMapSubwindows(xw.dpy, xw.win);
  XMapWindow(xw.dpy, xw.win);
}

void die( char *str ){
  fprintf(stderr, "%s\n", str );
  exit(1);
}
void line_clear( void ){
  XClearArea( xw.dpy, xw.win, 0, term.row-term.fh+2, xw.w, term.fh, 1 );		// 画面クリア
}
void buff_clear( char* line ){
  int i;
  for( i = 0; i < LINE_MAX; i++ ){
    line[i] = '\0';
  }
}

