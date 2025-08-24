#include <sstream>
#include <cctype>
#include <sys/ioctl.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string>
#include <utility>

#define CTRL_KEY(k) ((k) & 0x1f)

class Abuf {
private:
  std::string buffer;

public:
  Abuf() = default;
  ~Abuf() = default;

  void append(const std::string& s) {
    buffer +=s;
  }

  void append(const char *s, size_t len) {
    buffer.append(s, len);
  }

  void clear() {
    buffer.clear();
  }

  const char* data() const {
    return buffer.data();
  }

  int size() const {
    return static_cast<int>(buffer.size()) ;
  }
  
  std::string str() const {
    return buffer;
  }
};



struct editorConfig {
  int cx, cy;
  int screenrows;
  int screencols;
  struct termios orig_terminos;
} E;

void die(const char *s) {
  write(STDIN_FILENO, "\x1b[2j", 4);
  write(STDIN_FILENO, "\x1b[H",3);
  perror(s);
  exit(1);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_terminos) == -1) die("tcsetattr");
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &E.orig_terminos) == -1) die("tcgetattr") ;
    atexit(disableRawMode);
    struct termios raw = E.orig_terminos;
    raw.c_iflag &= ~(ICRNL | IXON | INPCK | BRKINT);
    raw.c_cflag |= (CS8);
    raw.c_oflag &= ~(OPOST);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

char editorReadKey() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) die("read") ; 
  }
  return c;
}
int getCursorPosition(int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;
  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return -1;
  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
    if (buf[i] == 'R') break;
    i++;
  }
  if (buf[0] != '\x1b' || buf[1] != '[') return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
  return 0;
}



int getWindowSize(int *rows, int *cols) {
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col ==0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
    return getCursorPosition(rows, cols);
    return -1;
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}
  
void editorMoveCursor(char key) {
  switch (key) {
    case 'a':
      E.cx--;
    break;
    case 'd':
      E.cx++;
    break;
    case 'w':
      E.cy--;
    break;
     case 's':
      E.cy++;
    break; 
  }
}


void editorProcesskeypress() {
  char c = editorReadKey();

  switch (c) {
    case CTRL_KEY('q'): 
    exit(0);
    break;

    case 'w':
    case 's':
    case 'a':
    case 'd':
    editorMoveCursor(c);
    break;
  }
}


const int VERSION = 1;
void editorDrawRows(Abuf &ab) {
for (int y = 0; y < E.screenrows; y++) {
  if (y == E.screenrows / 3) {
    std::ostringstream oss;
      oss << "TUI text editor "   << VERSION;
      std::string welcome = oss.str();
    if ((int)welcome.size() > E.screencols) {
      welcome.resize(E.screencols);
    }

    ab.append(welcome.data(), welcome.size());
  } else {
    ab.append("~", 1);
}

  ab.append("\x1b[K", 3);  // erase to end of line

  if (y < E.screenrows - 1) {
    ab.append("\r\n", 2); // newline except last row
  }
  }
}

void editorRefreshScreen() { 
  Abuf ab;
  ab.append("\x1b[?25l", 6);
  ab.append("\x1b[H", 3);
  editorDrawRows(ab);

  std::ostringstream oss;
  oss << "\x1b[" << (E.cy + 10) << ";" << (E.cx + 1) << "H";
  std::string buf = oss.str();
  ab.append(buf.data(), buf.size());

  ab.append("\x1b[?25h", 6);

  write(STDOUT_FILENO, ab.data(), ab.size());
}


void initEditor() {
  E.cy = 0;
  E.cx = 0;
  if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
}
int main() {
  enableRawMode();
  initEditor();

  while (1) {
    editorRefreshScreen();
    editorProcesskeypress();
    }
  return 0;

};
