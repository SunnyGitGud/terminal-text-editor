#include <cstddef>
#include <fstream>
#include <iterator>
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
#include <vector>

#define CTRL_KEY(k) ((k) & 0x1f)

class Abuf {
private:
  std::string buffer;

public:
  Abuf() = default;
  ~Abuf() = default;

  void append(const std::string& s) {
    buffer += s;
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
    return static_cast<int>(buffer.size());
  }
  
  std::string str() const {
    return buffer;
  }
};

enum editorKey {
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DEL_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN
};

struct erow {
  std::string chars;
  std::size_t size() const {
    return chars.size();
  }
};

struct editorConfig {
  int cx, cy;
  int rowoff;
  int coloff;
  int screenrows;
  int screencols;
  int numrows;
  std::vector<erow> rows; 
  struct termios orig_terminos;
} E;

void die(const char *s) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);
  perror(s);
  exit(1);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_terminos) == -1) die("tcsetattr");
}

void enableRawMode() {
    if (tcgetattr(STDIN_FILENO, &E.orig_terminos) == -1) die("tcgetattr");
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

int editorReadKey() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) die("read");
  }
  if (c == '\x1b') {
    char seq[3];
    if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
    if (seq[0] == '[') {
      if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
        if (seq[2] == '~') {
          switch (seq[1]) {
            case '1': return HOME_KEY;
            case '3': return DEL_KEY;
            case '4': return END_KEY;
            case '5': return PAGE_UP;
            case '6': return PAGE_DOWN;
            case '7': return HOME_KEY;
            case '8': return END_KEY;
          }
        }
      } else {
        switch (seq[1]) {
          case 'A': return ARROW_UP;
          case 'B': return ARROW_DOWN;
          case 'C': return ARROW_RIGHT;
          case 'D': return ARROW_LEFT;
          case 'H': return HOME_KEY;
          case 'F': return END_KEY;
        }
      }
    } else if (seq[0] == 'O') {
      switch (seq[1]) {
        case 'H': return HOME_KEY;
        case 'F': return END_KEY;
      }
    }
    return '\x1b';
  } else {
    return c;
  }
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
  buf[i] = '\0'; // Null terminate
  if (buf[0] != '\x1b' || buf[1] != '[') return -1;
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;
  return 0;
}

int getWindowSize(int *rows, int *cols) {
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return -1;
    return getCursorPosition(rows, cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

void editorOpen(const std::string& filename) {
  std::ifstream file(filename);
  if (!file.is_open()) {
    die("fopen"); 
  }

  E.rows.clear();           // clear existing rows
  std::string line;
  while (std::getline(file, line)) {
    // Trim trailing newlines / carriage returns
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
      line.pop_back();
    }
    erow row;
    row.chars = line;
    E.rows.push_back(row);
  }

  E.numrows = E.rows.size();  // update row count
}

void editorMoveCursor(int key) {
  switch (key) {
    case ARROW_LEFT:
      if (E.cx > 0) {
        E.cx--;
      } else if (E.cy > 0) {
        // Move to end of previous line
        E.cy--;
        E.cx = E.rows[E.cy].size();
      }
      break;

    case ARROW_RIGHT: {
      if (E.cy < E.numrows) {
        int rowlen = E.rows[E.cy].size();
        if (E.cx < rowlen) {
          E.cx++;
        } else if (E.cx == rowlen && E.cy + 1 < E.numrows) {
          // Move to beginning of next line
          E.cy++;
          E.cx = 0;
        }
      }
    } break;

    case ARROW_UP:
      if (E.cy > 0) {
        E.cy--;
      }
      break;

    case ARROW_DOWN:
      if (E.cy + 1 < E.numrows) {
        E.cy++;
      }
      break;
  }

  // Clamp cx to current row length
  int rowlen = (E.cy < E.numrows) ? E.rows[E.cy].size() : 0;
  if (E.cx > rowlen) {
    E.cx = rowlen;
  }
}

void editorProcessKeypress() {
  int c = editorReadKey();
  switch (c) {
    case CTRL_KEY('q'):
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;
    case HOME_KEY:
      E.cx = 0;
      break;
    case END_KEY:
      if (E.cy < E.numrows) {
        E.cx = E.rows[E.cy].size();
      }
      break;
    case PAGE_UP:
    case PAGE_DOWN:
      {
        if (c == PAGE_UP) {
          E.cy = E.rowoff;
        } else if (c == PAGE_DOWN) {
          E.cy = E.rowoff + E.screenrows - 1;
          if (E.cy > E.numrows) E.cy = E.numrows;
        }
        
        int times = E.screenrows;
        while (times--)
          editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
      }
      break;
    case ARROW_UP:
    case ARROW_DOWN:
    case ARROW_LEFT:
    case ARROW_RIGHT:
      editorMoveCursor(c);
      break;
  }
}

void editorScroll() {
  // Clamp vertical scroll
  if (E.cy < E.rowoff) {
    E.rowoff = E.cy;
  }
  if (E.cy >= E.rowoff + E.screenrows) {
    E.rowoff = E.cy - E.screenrows + 1;
  }

  // Clamp horizontal scroll
  if (E.cx < E.coloff) {
    E.coloff = E.cx;
  }
  if (E.cx >= E.coloff + E.screencols) {
    E.coloff = E.cx - E.screencols + 1;
  }
}

const int VERSION = 1;

void editorDrawRows(Abuf &ab) {
  for (int y = 0; y < E.screenrows; y++) {
    int filerow = y + E.rowoff;

    if (filerow >= E.numrows) {
      // Empty row
      if (E.numrows == 0 && y == E.screenrows / 3) {
        std::ostringstream oss;
        oss << "TUI text editor -- version " << VERSION;
        std::string welcome = oss.str();
        int padding = (E.screencols - welcome.length()) / 2;
        if (padding) {
          ab.append("~", 1);
          padding--;
        }
        while (padding--) ab.append(" ", 1);
        if ((int)welcome.size() > E.screencols) welcome.resize(E.screencols);
        ab.append(welcome.data(), welcome.size());
      } else {
        ab.append("~", 1);
      }
    } else {
      // Draw file row
      int len = E.rows[filerow].size() - E.coloff;
      if (len < 0) len = 0;
      if (len > E.screencols) len = E.screencols;
      
      if (len > 0) {
        ab.append(E.rows[filerow].chars.data() + E.coloff, len);
      }
    }

    ab.append("\x1b[K", 3); // clear line
    if (y < E.screenrows - 1) ab.append("\r\n", 2);
  }
}

void editorRefreshScreen() {
  editorScroll();
  Abuf ab;
  ab.append("\x1b[?25l", 6);
  ab.append("\x1b[H", 3); 
  editorDrawRows(ab);

  std::ostringstream oss;
  oss << "\x1b[" << (E.cy - E.rowoff + 1) << ";" << (E.cx - E.coloff + 1) << "H";
  std::string buf = oss.str();
  ab.append(buf.data(), buf.size());

  ab.append("\x1b[?25h", 6);

  write(STDOUT_FILENO, ab.data(), ab.size());
}

void initEditor() {
  E.cx = 0;
  E.cy = 0;
  E.rowoff = 0;
  E.coloff = 0; 
  E.numrows = 0;
  if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("getWindowSize");
}

int main(int argc, char *argv[]) {
  enableRawMode();
  initEditor();

  if (argc >= 2) {
    editorOpen(argv[1]);
  }
  
  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }
  return 0;
}
