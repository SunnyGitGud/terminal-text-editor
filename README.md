# 📄 Terminal Text Editor (C++ Version)

A minimalistic text editor written in **C++**, inspired by [kilo](https://github.com/antirez/kilo) by Salvatore Sanfilippo (the author of Redis).  
This project reimplements kilo’s core in modern C++ (using `std::string`, `std::vector`, RAII, and streams), while keeping the spirit of the original lightweight TUI editor.

---

## ✨ Features

- Basic text editing in the terminal (insert, delete, new lines).
- Open and save files.
- Supports scrolling through large files.
- Status bar with:
  - Filename
  - Modification indicator (`(modified)`)
  - Current line/total lines
  - File extension (as a naive filetype)
- Message bar for short-lived status/help messages.
- Cursor movement:
  - Arrow keys (`↑ ↓ ← →`)
  - `Page Up`, `Page Down`
  - `Home`, `End`
- Tab support (expands tabs into spaces).
- Minimal and dependency-free: just C++ standard library and POSIX headers.

---

## 🛠️ Build & Run

### Requirements
- A C++ compiler (g++/clang++) supporting C++11 or later.
- A POSIX-compatible system (Linux, macOS, WSL, BSD).

### Build
```bash
g++ -std=c++11 -Wall -Wextra -pedantic kilo.cpp -o kilo
```

### Run
```bash
./kilo [filename]
```

If you don’t pass a filename, you start with an empty buffer.

---

## ⌨️ Keybindings

| Key              | Action                  |
|------------------|-------------------------|
| **Ctrl-Q**       | Quit                    |
| **Ctrl-S**       | Save file               |
| **Arrow Keys**   | Move cursor             |
| **Page Up/Down** | Scroll one screen       |
| **Home/End**     | Jump to line start/end  |
| **Enter**        | Insert newline          |
| **Backspace/Del**| Delete character        |

---

## 📂 File I/O

- `Ctrl-S` writes the buffer to the file currently opened.
- If the file is new (started without a filename), saving is skipped silently (can be extended to ask for a filename).

---

## 🧩 Differences from the original kilo

- Implemented in **C++** instead of C.
- Uses `std::string` and `std::vector` for memory safety.
- `Abuf` is implemented as a thin wrapper around `std::string`.
- RAII and destructors handle cleanup where possible.
- Code is modularized into functions and structures while keeping the single-file spirit.

---

## 📜 License

This project follows the **BSD license** of the original kilo.  
You’re free to use, modify, and share it.

---

## 🙏 Acknowledgements

- [Salvatore Sanfilippo](https://github.com/antirez) for the original kilo editor in C.
- The [Build Your Own Text Editor](https://viewsourcecode.org/snaptoken/kilo/) tutorial by [snaptoken](https://viewsourcecode.org/) for detailed explanations.

