# MicroComputer Emulator

A lightweight fantasy-computer emulator with a custom instruction set, text-based graphics display, and a built-in command-line OS. Write programs in C that compile to custom bytecode and run them in a virtual environment with 64KB of RAM.

![License](https://img.shields.io/badge/license-GPL--3.0-blue.svg)
![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-lightgrey.svg)

## Features

- 🖥️ **Virtual CPU** with custom instruction set
- 📺 **80×25 character display** with GUI window
- 🎨 **320×200 pixel graphics mode** for simple graphics
- 🔊 **Sound support** with beep/tone generation
- 💾 **64KB RAM** with 256-byte stack
- 🎮 **8 general-purpose registers** for computation
- 🔀 **Conditional jumps** and flow control
- ⌨️ **Keyboard input** opcodes
- 🗂️ **Virtual filesystem** for program storage
- 🐚 **Built-in shell** with Unix-like commands
- 🎬 **Boot and loading animations**
- 🔧 **Cross-platform** (Windows & Linux)
- 📝 **Program generator** system using C

## Table of Contents

- [Installation](#installation)
- [Quick Start](#quick-start)
- [Using the Shell](#using-the-shell)
- [Programming Guide](#programming-guide)
- [CPU Architecture](#cpu-architecture)
- [Example Programs](#example-programs)
- [API Reference](#api-reference)
- [Troubleshooting](#troubleshooting)
- [Contributing](#contributing)
- [License](#license)

---

## Installation

### Prerequisites

**Linux:**
- GCC compiler
- X11 development libraries
- pthread library

```bash
sudo apt-get install build-essential libx11-dev
```

**Windows:**
- MinGW-w64 or similar GCC toolchain
- Windows SDK (included with MinGW)

### Build Instructions

**Linux:**
```bash
gcc -o microemu microemu.c -lX11 -lpthread -lm
./microemu
```

**Windows (MinGW):**
```bash
gcc -o microemu.exe microemu.c -lws2_32 -lgdi32 -lpthread
microemu.exe
```

On first run, the emulator creates a `./fs/` directory for storing programs.

---

## Quick Start

### 1. Start the Emulator

```bash
./microemu
```

A graphical window will appear with the MicroOS shell prompt.

### 2. Create Your First Program

Create `hello.c`:

```c
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define OP_HALT         0x00
#define OP_PRINT_STR    0x02

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

void emit_string(Program *p, const char *str) {
    while (*str) emit_byte(p, *str++);
    emit_byte(p, 0);
}

void print_str(Program *p, const char *str) {
    emit_byte(p, OP_PRINT_STR);
    emit_string(p, str);
}

void halt(Program *p) {
    emit_byte(p, OP_HALT);
}

int main() {
    Program prog;
    init_program(&prog);
    
    print_str(&prog, "Hello, MicroComputer!\n");
    halt(&prog);
    
    FILE *f = fopen("hello.bin", "wb");
    fwrite(prog.data, 1, prog.size, f);
    fclose(f);
    
    free(prog.data);
    return 0;
}
```

### 3. Compile and Run

```bash
# Compile the program generator
gcc -o hello hello.c

# Generate the bytecode
./hello

# Move to emulator filesystem
mv hello.bin fs/

# In the emulator window, type:
run hello.bin
```

---

## Using the Shell

The MicroOS shell provides Unix-like commands for file management and system control.

### Available Commands

| Command | Description | Example |
|---------|-------------|---------|
| `help` | Display all commands | `help` |
| `clear` | Clear the screen | `clear` |
| `ls` | List files | `ls` |
| `cat <file>` | Display file contents | `cat readme.txt` |
| `rm <file>` | Delete a file | `rm old.bin` |
| `cp <src> <dst>` | Copy a file | `cp prog.bin backup.bin` |
| `mv <src> <dst>` | Move/rename a file | `mv old.bin new.bin` |
| `echo <text>` | Print text | `echo Hello World` |
| `date` | Show current date/time | `date` |
| `uptime` | Show system uptime | `uptime` |
| `meminfo` | Display memory info | `meminfo` |
| `run <file>` | Execute a program | `run demo.bin` |
| `hexdump <file>` | Hex dump of file | `hexdump prog.bin` |
| `history` | Show command history | `history` |
| `exit` | Exit the emulator | `exit` |

---

## Programming Guide

### Program Structure

Programs are created by writing C code that generates bytecode using helper functions. The general workflow is:

1. Initialize a `Program` struct
2. Emit opcodes and data using helper functions
3. Write the bytecode to a `.bin` file
4. Copy the `.bin` to the `fs/` directory
5. Run it in the emulator

### Core Program Template

```c
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// Opcode definitions
#define OP_HALT         0x00
#define OP_PRINT_CHAR   0x01
#define OP_PRINT_STR    0x02
#define OP_CLEAR_SCREEN 0x04
#define OP_SLEEP_MS     0x20

// Program structure
typedef struct {
    uint8_t *data;
    size_t size;
    size_t capacity;
} Program;

// Initialize program buffer
void init_program(Program *p) {
    p->capacity = 1024;
    p->data = malloc(p->capacity);
    p->size = 0;
}

// Emit a single byte
void emit_byte(Program *p, uint8_t byte) {
    if (p->size >= p->capacity) {
        p->capacity *= 2;
        p->data = realloc(p->data, p->capacity);
    }
    p->data[p->size++] = byte;
}

// Emit a 16-bit word (little-endian)
void emit_word(Program *p, uint16_t word) {
    emit_byte(p, word & 0xFF);
    emit_byte(p, (word >> 8) & 0xFF);
}

// Emit a null-terminated string
void emit_string(Program *p, const char *str) {
    while (*str) emit_byte(p, *str++);
    emit_byte(p, 0);
}

// Save program to file
void save_program(Program *p, const char *filename) {
    FILE *f = fopen(filename, "wb");
    if (f) {
        fwrite(p->data, 1, p->size, f);
        fclose(f);
    }
    free(p->data);
}
```

### Helper Functions

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

## CPU Architecture

### Specifications

- **Memory:** 64KB RAM (65,536 bytes)
- **Stack:** 256 bytes
- **Registers:** 8 general-purpose 16-bit registers
- **Program Counter:** 16-bit
- **Stack Pointer:** 16-bit
- **Display:** 80×25 character text mode

### Instruction Set

| Opcode | Mnemonic | Arguments | Description |
|--------|----------|-----------|-------------|
| `0x00` | HALT | None | Stop execution |
| `0x01` | PRINT_CHAR | 1 byte | Print single character |
| `0x02` | PRINT_STR | String + null | Print null-terminated string |
| `0x04` | CLEAR_SCREEN | None | Clear display |
| `0x20` | SLEEP_MS | 2 bytes (LE) | Sleep for N milliseconds |
| `0x21` | BEEP | 4 bytes (freq, duration) | Play beep sound |
| `0x30` | SET_PIXEL | 5 bytes (x, y, value) | Set pixel in graphics mode |
| `0x31` | CLEAR_PIXELS | None | Clear pixel buffer |
| `0x40` | LOAD_REG | 3 bytes (reg, value) | Load immediate to register |
| `0x41` | STORE_REG | 3 bytes (reg, addr) | Store register to memory |
| `0x50` | ADD | 3 bytes (dst, src1, src2) | Add registers |
| `0x51` | SUB | 3 bytes (dst, src1, src2) | Subtract registers |
| `0x52` | MUL | 3 bytes (dst, src1, src2) | Multiply registers |
| `0x53` | DIV | 3 bytes (dst, src1, src2) | Divide registers |
| `0x54` | AND | 3 bytes (dst, src1, src2) | Bitwise AND |
| `0x55` | OR | 3 bytes (dst, src1, src2) | Bitwise OR |
| `0x56` | XOR | 3 bytes (dst, src1, src2) | Bitwise XOR |
| `0x57` | NOT | 2 bytes (dst, src) | Bitwise NOT |
| `0x58` | CMP | 2 bytes (src1, src2) | Compare registers |
| `0x60` | JMP | 2 bytes (addr) | Unconditional jump |
| `0x61` | JZ | 2 bytes (addr) | Jump if zero |
| `0x62` | JNZ | 2 bytes (addr) | Jump if not zero |
| `0x63` | JG | 2 bytes (addr) | Jump if greater |
| `0x64` | JL | 2 bytes (addr) | Jump if less |
| `0x70` | READ_CHAR | 1 byte (reg) | Read character into register |
| `0x80` | LOAD_MEM | 3 bytes (reg, addr) | Load from memory to register |
| `0x81` | STORE_MEM | 3 bytes (addr, reg) | Store register to memory |

### Memory Map

```
0x0000 - 0xFFFF : RAM (64KB)
  0x0000 - ...  : Program code/data
  Stack grows from top
```

---

## Example Programs

### Example 1: Hello World

```c
int main() {
    Program prog;
    init_program(&prog);
    
    print_str(&prog, "Hello, World!\n");
    halt(&prog);
    
    save_program(&prog, "hello.bin");
    return 0;
}
```

### Example 2: Countdown Timer

```c
int main() {
    Program prog;
    init_program(&prog);
    
    for (int i = 10; i >= 0; i--) {
        clear_screen(&prog);
        
        char buf[32];
        snprintf(buf, sizeof(buf), "Countdown: %d\n", i);
        print_str(&prog, buf);
        
        sleep_ms(&prog, 1000);
    }
    
    print_str(&prog, "\nBlastoff!\n");
    halt(&prog);
    
    save_program(&prog, "countdown.bin");
    return 0;
}
```

### Example 3: Text Animation

```c
int main() {
    Program prog;
    init_program(&prog);
    
    const char *frames[] = {
        "  O  \n /|\\ \n / \\ ",
        " \\O/ \n  |  \n / \\ ",
        "  O  \n /|\\ \n / \\ ",
        " /O\\ \n  |  \n / \\ "
    };
    
    for (int loop = 0; loop < 10; loop++) {
        for (int i = 0; i < 4; i++) {
            clear_screen(&prog);
            print_str(&prog, "Dancing Stick Figure\n\n");
            print_str(&prog, frames[i]);
            sleep_ms(&prog, 200);
        }
    }
    
    halt(&prog);
    save_program(&prog, "dance.bin");
    return 0;
}
```

---

## API Reference

### Program Management

#### `void init_program(Program *p)`
Initializes a program buffer with 1KB initial capacity.

#### `void emit_byte(Program *p, uint8_t byte)`
Appends a single byte to the program. Automatically grows buffer if needed.

#### `void emit_word(Program *p, uint16_t word)`
Appends a 16-bit value in little-endian format.

#### `void emit_string(Program *p, const char *str)`
Appends a null-terminated string (including the null byte).

#### `void save_program(Program *p, const char *filename)`
Writes the program buffer to a file and frees memory.

### Display Functions

#### `void clear_screen(Program *p)`
Clears the entire display and resets cursor to top-left.

#### `void print_str(Program *p, const char *str)`
Prints a string at the current cursor position. Supports `\n` for newlines.

#### `void print_char(Program *p, char c)`
Prints a single character at the current cursor position.

### Timing

#### `void sleep_ms(Program *p, uint16_t ms)`
Pauses execution for the specified number of milliseconds (0-65535).

### Control Flow

#### `void halt(Program *p)`
Stops program execution. **Always required at the end of your program.**

---

## Troubleshooting

### Common Issues

| Problem | Solution |
|---------|----------|
| **Program doesn't stop** | Add `halt()` at the end |
| **Nothing displays** | Ensure you're using `print_str()` or `print_char()` |
| **File not found in emulator** | Move `.bin` file to `fs/` directory |
| **Garbled text** | Check for missing null terminators in strings |
| **Window not updating (Windows)** | Latest patch fixes this - recompile emulator |
| **Compilation errors** | Ensure all helper functions are defined |

### Debug Tips

1. **Inspect bytecode:**
   ```bash
   hexdump -C myprogram.bin
   ```

2. **Test generator compilation:**
   ```bash
   gcc -Wall -Wextra -o test test.c
   ```

3. **Check file size:**
   ```bash
   ls -lh fs/
   ```

4. **Use `hexdump` command in emulator:**
   ```
   hexdump myprogram.bin
   ```

---

## Best Practices

- ✅ Always end programs with `halt()`
- ✅ Use `clear_screen()` before each animation frame
- ✅ Keep filenames short and lowercase
- ✅ Use `.bin` extension for all programs
- ✅ Test your generator program before copying to `fs/`
- ✅ Add delays with `sleep_ms()` for animations
- ✅ Use `snprintf()` for formatted output

---

## File Naming Conventions

- Use lowercase letters only
- No spaces (use underscores or hyphens)
- Must end with `.bin` extension
- Keep names under 60 characters
- Examples: `hello.bin`, `countdown.bin`, `demo_game.bin`

---

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for:

- New opcodes or CPU features
- Additional example programs
- Documentation improvements
- Bug fixes
- Cross-platform compatibility enhancements

---

## License

This project is licensed under the GNU General Public License v3.0 - see the [LICENSE](LICENSE) file for details.

### GPL-3.0 Summary

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

---

## Roadmap

Future enhancements planned:

- [x] Pixel graphics mode
- [x] Sound/beep support
- [x] Keyboard input opcodes
- [x] Arithmetic and logic operations
- [x] Conditional jumps and loops
- [x] Register operations
- [x] Memory read/write opcodes
- [ ] Advanced file I/O operations
- [ ] Networking capabilities
- [ ] Interrupt system
- [ ] DMA controller
- [ ] More graphics primitives (lines, circles)
- [ ] Sprite system
- [ ] Multi-channel audio

---

## Acknowledgments

Inspired by fantasy consoles like PICO-8 and TIC-80, designed as a learning platform for low-level programming and computer architecture concepts.

---

**Happy coding!** 🚀

For questions or support, please open an issue on GitHub.
