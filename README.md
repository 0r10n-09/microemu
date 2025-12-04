# MicroComputer Emulator  
A lightweight fantasy-computer emulator with a custom instruction set, simple graphics, and a C-based program generator system.

This README explains:

- How to compile the emulator  
- How to write programs for it  
- How the CPU works  
- Example programs  
- A full template you can copy to start your own projects  

---

# 📦 Compilation Instructions

These are the commands for building the **microemu** emulator itself.

### **Linux**
```bash
gcc -o microemu microemu.c -lX11 -lpthread -lm
```

### **Windows (MinGW)**
```bash
gcc -o microemu.exe microemu.c -lws2_32 -lgdi32 -lpthread
```

---

# 🧰 Programming Guide

Programs for the MicroComputer emulator are written in **C** and compiled into **.bin** files that the emulator can run.

---

# 🚀 Quick Start

1. Copy `makedemo.c` (or the template at the end of this README)
2. Write program logic using the provided helper functions
3. Compile your generator program:
   ```bash
   gcc -o myprogram myprogram.c
   ```
4. Run it to generate a `.bin`:
   ```bash
   ./myprogram
   ```
5. Move the `.bin` file to the `fs/` directory of your emulator
6. Run it inside the emulator:
   ```
   run myfile.bin
   ```

---

# 🧠 CPU Overview

## **Available Opcodes**

| Opcode | Name | Description |
|--------|------|-------------|
| `0x00` | HALT | Stop program execution |
| `0x01` | PRINT_CHAR | Print a single character |
| `0x02` | PRINT_STR | Print a null-terminated string |
| `0x04` | CLEAR_SCREEN | Clear the display |
| `0x20` | SLEEP_MS | Delay execution by N ms (2-byte argument) |

---

# 🗄 Memory Layout

- **Total RAM:** 64KB  
- **Stack size:** 256 bytes  
- Programs load at `0x0000`  
- Program counter starts at `0x0000`

---

# 📝 Writing Your First Program

### **Step 1 — Basic Template**

```c
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define OP_HALT         0x00
#define OP_PRINT_CHAR   0x01
#define OP_PRINT_STR    0x02
#define OP_CLEAR_SCREEN 0x04
#define OP_SLEEP_MS     0x20

typedef struct {
    uint8_t *data;
    size_t size;
    size_t capacity;
} Program;

void init_program(Program *p) {
    p->capacity = 1024;
    p->data = malloc(p->capacity);
    p->size = 0;
}

void emit_byte(Program *p, uint8_t byte) {
    if (p->size >= p->capacity) {
        p->capacity *= 2;
        p->data = realloc(p->data, p->capacity);
    }
    p->data[p->size++] = byte;
}

void emit_word(Program *p, uint16_t word) {
    emit_byte(p, word & 0xFF);
    emit_byte(p, (word >> 8) & 0xFF);
}

void emit_string(Program *p, const char *str) {
    while (*str) emit_byte(p, *str++);
    emit_byte(p, 0);
}
```

### **Step 2 — Helper Functions**

```c
void clear_screen(Program *p) {
    emit_byte(p, OP_CLEAR_SCREEN);
}

void print_str(Program *p, const char *str) {
    emit_byte(p, OP_PRINT_STR);
    emit_string(p, str);
}

void print_char(Program *p, char c) {
    emit_byte(p, OP_PRINT_CHAR);
    emit_byte(p, c);
}

void sleep_ms(Program *p, uint16_t ms) {
    emit_byte(p, OP_SLEEP_MS);
    emit_word(p, ms);
}

void halt(Program *p) {
    emit_byte(p, OP_HALT);
}
```

---

# 🧪 Example Programs

(Examples omitted here for brevity in file; full examples were included in chat.)

---

# 🧭 Tips and Best Practices

- **Always end with `halt()`**  
- Use `sleep_ms()` to control animation speed  
- Clear the screen before drawing each animation frame  
- Use `snprintf()` for advanced formatted messages  
- Test your generator program before copying `.bin` files

---

# 🔧 Debugging

| Problem | Fix |
|--------|------|
| Program crashes | You forgot `halt()` |
| Nothing shows on screen | You didn’t call `print_str()` or `print_char()` |
| Emulator won't load file | `.bin` not in `fs/` |
| Text is corrupted | Missing null terminators |
| Timing feels wrong | Adjust `sleep_ms()` |

---

# 🗃 File Naming Rules

- Lowercase only  
- Use `.bin` extension for final output  
- Avoid long names (limit < 60 chars)  
- No spaces  

---

# ❓ Getting Help

If you're stuck:

1. Make sure your program compiles with no warnings  
2. Ensure your `.bin` exists  
3. Place it inside `fs/`  
4. Verify emulator runs other known-good programs  
5. Inspect binaries using:
   ```bash
   hexdump -C file.bin
   ```

---

Happy programming! 🚀
