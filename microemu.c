/*
 * MicroComputer Emulator with GUI Display and CLI OS
 * Compile: 
 *   Windows: gcc -o microemu.exe microemu.c -lws2_32 -lgdi32 -lpthread
 *   Linux:   gcc -o microemu microemu.c -lX11 -lpthread -lm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <pthread.h>
#include <sys/stat.h>
#include <ctype.h>

#ifdef _WIN32
    #include <windows.h>
    #include <winsock2.h>
    #include <direct.h>
    #define mkdir(path, mode) _mkdir(path)
    #define PATH_SEP "\\"
#else
    #include <unistd.h>
    #include <termios.h>
    #include <sys/select.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <dirent.h>
    #include <X11/Xlib.h>
    #include <X11/Xutil.h>
    #include <X11/keysym.h>
    #define PATH_SEP "/"
#endif

// Virtual CPU Specifications
#define MEM_SIZE (64 * 1024)
#define STACK_SIZE 256
#define MAX_PATH_LEN 256
#define SCREEN_WIDTH 80
#define SCREEN_HEIGHT 25
#define CHAR_WIDTH 8
#define CHAR_HEIGHT 16
#define INPUT_BUFFER_SIZE 256
#define MAX_HISTORY 50

// Virtual CPU state
typedef struct {
    uint8_t memory[MEM_SIZE];
    uint16_t pc;
    uint16_t sp;
    uint16_t regs[8];
    uint8_t flags;
    int running;
} CPU;

// Screen buffer
typedef struct {
    char chars[SCREEN_HEIGHT][SCREEN_WIDTH];
    int cursor_x;
    int cursor_y;
    int cursor_visible;
    int dirty;
} VScreen;

// File system
typedef struct {
    char name[64];
    uint8_t *data;
    size_t size;
    time_t modified;
} File;

typedef struct {
    File files[64];
    int file_count;
    char root_dir[MAX_PATH_LEN];
} FileSystem;

// Input handling
typedef struct {
    char buffer[INPUT_BUFFER_SIZE];
    int pos;
    int ready;
    pthread_mutex_t mutex;
} InputBuffer;

// Command history
typedef struct {
    char commands[MAX_HISTORY][INPUT_BUFFER_SIZE];
    int count;
    int current;
} History;

// Global state
CPU cpu;
VScreen screen;
FileSystem fs;
char current_dir[MAX_PATH_LEN];
int window_running = 1;
int os_mode = 1;
InputBuffer input_buf;
History history;

#ifdef _WIN32
HWND hwnd;
HDC hdc_mem;
HBITMAP hbm_mem;
HFONT hfont;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_DESTROY:
            window_running = 0;
            PostQuitMessage(0);
            return 0;
        case WM_CHAR:
            if (wParam == VK_RETURN) {
                pthread_mutex_lock(&input_buf.mutex);
                input_buf.buffer[input_buf.pos] = '\0';
                input_buf.ready = 1;
                pthread_mutex_unlock(&input_buf.mutex);
            } else if (wParam == VK_BACK) {
                pthread_mutex_lock(&input_buf.mutex);
                if (input_buf.pos > 0) {
                    input_buf.pos--;
                    if (screen.cursor_x > 0) screen.cursor_x--;
                    screen.chars[screen.cursor_y][screen.cursor_x] = ' ';
                    screen.dirty = 1;
                }
                pthread_mutex_unlock(&input_buf.mutex);
            } else if (wParam >= 32 && wParam < 127) {
                pthread_mutex_lock(&input_buf.mutex);
                if (input_buf.pos < INPUT_BUFFER_SIZE - 1) {
                    input_buf.buffer[input_buf.pos++] = (char)wParam;
                    screen.chars[screen.cursor_y][screen.cursor_x++] = (char)wParam;
                    screen.dirty = 1;
                }
                pthread_mutex_unlock(&input_buf.mutex);
            }
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            HFONT old_font = SelectObject(hdc_mem, hfont);
            
            for (int y = 0; y < SCREEN_HEIGHT; y++) {
                for (int x = 0; x < SCREEN_WIDTH; x++) {
                    RECT rect = {x * CHAR_WIDTH, y * CHAR_HEIGHT, 
                                (x + 1) * CHAR_WIDTH, (y + 1) * CHAR_HEIGHT};
                    
                    HBRUSH brush = CreateSolidBrush(RGB(0, 0, 0));
                    FillRect(hdc_mem, &rect, brush);
                    DeleteObject(brush);
                    
                    SetTextColor(hdc_mem, RGB(255, 255, 255));
                    SetBkMode(hdc_mem, TRANSPARENT);
                    
                    char c = screen.chars[y][x];
                    if (x == screen.cursor_x && y == screen.cursor_y && screen.cursor_visible) {
                        c = '_';
                    }
                    TextOut(hdc_mem, x * CHAR_WIDTH, y * CHAR_HEIGHT, &c, 1);
                }
            }
            
            SelectObject(hdc_mem, old_font);
            BitBlt(hdc, 0, 0, SCREEN_WIDTH * CHAR_WIDTH, SCREEN_HEIGHT * CHAR_HEIGHT,
                   hdc_mem, 0, 0, SRCCOPY);
            
            EndPaint(hwnd, &ps);
            return 0;
        }
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void* window_thread(void* arg) {
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "MicroEmuClass";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW);
    
    RegisterClass(&wc);
    
    int win_width = SCREEN_WIDTH * CHAR_WIDTH + 16;
    int win_height = SCREEN_HEIGHT * CHAR_HEIGHT + 39;
    
    hwnd = CreateWindowEx(0, "MicroEmuClass", "MicroComputer",
                         WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
                         CW_USEDEFAULT, CW_USEDEFAULT, win_width, win_height,
                         NULL, NULL, GetModuleHandle(NULL), NULL);
    
    HDC hdc = GetDC(hwnd);
    hdc_mem = CreateCompatibleDC(hdc);
    hbm_mem = CreateCompatibleBitmap(hdc, SCREEN_WIDTH * CHAR_WIDTH, 
                                     SCREEN_HEIGHT * CHAR_HEIGHT);
    SelectObject(hdc_mem, hbm_mem);
    ReleaseDC(hwnd, hdc);
    
    hfont = CreateFont(16, 8, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                      DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Courier New");
    
    ShowWindow(hwnd, SW_SHOW);
    
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) && window_running) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        
        if (screen.dirty) {
            InvalidateRect(hwnd, NULL, FALSE);
            screen.dirty = 0;
        }
    }
    
    return NULL;
}

#else
Display *display;
Window window;
GC gc;
XFontStruct *font;
Colormap colormap;

void* window_thread(void* arg) {
    display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Cannot open X display\n");
        return NULL;
    }
    
    int screen_num = DefaultScreen(display);
    
    window = XCreateSimpleWindow(display, RootWindow(display, screen_num),
                                 10, 10, SCREEN_WIDTH * CHAR_WIDTH, 
                                 SCREEN_HEIGHT * CHAR_HEIGHT, 1,
                                 BlackPixel(display, screen_num),
                                 BlackPixel(display, screen_num));
    
    XSelectInput(display, window, ExposureMask | KeyPressMask | StructureNotifyMask);
    XStoreName(display, window, "MicroComputer");
    
    gc = XCreateGC(display, window, 0, NULL);
    font = XLoadQueryFont(display, "fixed");
    if (font) XSetFont(display, gc, font->fid);
    
    XSetForeground(display, gc, WhitePixel(display, screen_num));
    
    XMapWindow(display, window);
    
    XEvent event;
    while (window_running) {
        while (XPending(display) > 0) {
            XNextEvent(display, &event);
            if (event.type == DestroyNotify) {
                window_running = 0;
            } else if (event.type == Expose) {
                screen.dirty = 1;
            } else if (event.type == KeyPress) {
                char buf[32];
                KeySym keysym;
                int len = XLookupString(&event.xkey, buf, sizeof(buf), &keysym, NULL);
                
                pthread_mutex_lock(&input_buf.mutex);
                
                if (keysym == XK_Return || keysym == XK_KP_Enter) {
                    input_buf.buffer[input_buf.pos] = '\0';
                    input_buf.ready = 1;
                } else if (keysym == XK_BackSpace) {
                    if (input_buf.pos > 0) {
                        input_buf.pos--;
                        if (screen.cursor_x > 0) screen.cursor_x--;
                        screen.chars[screen.cursor_y][screen.cursor_x] = ' ';
                        screen.dirty = 1;
                    }
                } else if (len > 0 && buf[0] >= 32 && buf[0] < 127) {
                    if (input_buf.pos < INPUT_BUFFER_SIZE - 1) {
                        input_buf.buffer[input_buf.pos++] = buf[0];
                        screen.chars[screen.cursor_y][screen.cursor_x++] = buf[0];
                        screen.dirty = 1;
                    }
                }
                
                pthread_mutex_unlock(&input_buf.mutex);
            }
        }
        
        if (screen.dirty) {
            XSetForeground(display, gc, BlackPixel(display, DefaultScreen(display)));
            XFillRectangle(display, window, gc, 0, 0, 
                          SCREEN_WIDTH * CHAR_WIDTH, SCREEN_HEIGHT * CHAR_HEIGHT);
            
            XSetForeground(display, gc, WhitePixel(display, DefaultScreen(display)));
            
            for (int y = 0; y < SCREEN_HEIGHT; y++) {
                for (int x = 0; x < SCREEN_WIDTH; x++) {
                    char c = screen.chars[y][x];
                    if (x == screen.cursor_x && y == screen.cursor_y && screen.cursor_visible) {
                        c = '_';
                    }
                    if (c != ' ') {
                        XDrawString(display, window, gc, 
                                  x * CHAR_WIDTH, y * CHAR_HEIGHT + 12,
                                  &c, 1);
                    }
                }
            }
            screen.dirty = 0;
        }
        
        usleep(16000);
    }
    
    XCloseDisplay(display);
    return NULL;
}
#endif

void putchar_screen(char c);

void init_screen(void) {
    memset(&screen, 0, sizeof(VScreen));
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            screen.chars[y][x] = ' ';
        }
    }
    screen.cursor_visible = 1;
    screen.dirty = 1;
}

void clear_screen_display(void) {
    init_screen();
}

void putchar_screen(char c) {
    if (c == '\n') {
        screen.cursor_y++;
        screen.cursor_x = 0;
    } else if (c == '\r') {
        screen.cursor_x = 0;
    } else if (c == '\b') {
        if (screen.cursor_x > 0) screen.cursor_x--;
    } else if (c == '\t') {
        screen.cursor_x = (screen.cursor_x + 4) & ~3;
    } else {
        if (screen.cursor_x >= SCREEN_WIDTH) {
            screen.cursor_x = 0;
            screen.cursor_y++;
        }
        if (screen.cursor_y >= SCREEN_HEIGHT) {
            for (int y = 0; y < SCREEN_HEIGHT - 1; y++) {
                memcpy(screen.chars[y], screen.chars[y+1], SCREEN_WIDTH);
            }
            memset(screen.chars[SCREEN_HEIGHT-1], ' ', SCREEN_WIDTH);
            screen.cursor_y = SCREEN_HEIGHT - 1;
        }
        screen.chars[screen.cursor_y][screen.cursor_x] = c;
        screen.cursor_x++;
    }
    screen.dirty = 1;
}

void print_to_screen(const char *str) {
    while (*str) {
        putchar_screen(*str++);
    }
}

char* read_line_from_screen(void) {
    static char line_buf[INPUT_BUFFER_SIZE];
    
    pthread_mutex_lock(&input_buf.mutex);
    input_buf.pos = 0;
    input_buf.ready = 0;
    pthread_mutex_unlock(&input_buf.mutex);
    
    while (!input_buf.ready && window_running) {
        usleep(50000);
    }
    
    pthread_mutex_lock(&input_buf.mutex);
    strcpy(line_buf, input_buf.buffer);
    input_buf.pos = 0;
    input_buf.ready = 0;
    pthread_mutex_unlock(&input_buf.mutex);
    
    putchar_screen('\n');
    
    return line_buf;
}

// File system functions
void init_filesystem(void) {
    memset(&fs, 0, sizeof(FileSystem));
    snprintf(fs.root_dir, MAX_PATH_LEN, ".%sfs", PATH_SEP);
    strncpy(current_dir, "/", MAX_PATH_LEN - 1);
    mkdir(fs.root_dir, 0755);
}

int load_file_from_disk(const char *filename) {
    char path[MAX_PATH_LEN];
    snprintf(path, MAX_PATH_LEN, "%s%s%s", fs.root_dir, PATH_SEP, filename);
    
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    uint8_t *data = malloc(size);
    if (!data) {
        fclose(f);
        return -1;
    }
    
    fread(data, 1, size, f);
    fclose(f);
    
    if (fs.file_count >= 64) {
        free(data);
        return -1;
    }
    
    struct stat st;
    stat(path, &st);
    
    strncpy(fs.files[fs.file_count].name, filename, 63);
    fs.files[fs.file_count].data = data;
    fs.files[fs.file_count].size = size;
    fs.files[fs.file_count].modified = st.st_mtime;
    fs.file_count++;
    
    return 0;
}

File* find_file(const char *name) {
    for (int i = 0; i < fs.file_count; i++) {
        if (strcmp(fs.files[i].name, name) == 0) {
            return &fs.files[i];
        }
    }
    return NULL;
}

int write_file(const char *filename, const uint8_t *data, size_t size) {
    char path[MAX_PATH_LEN];
    snprintf(path, MAX_PATH_LEN, "%s%s%s", fs.root_dir, PATH_SEP, filename);
    
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    
    fwrite(data, 1, size, f);
    fclose(f);
    
    return 0;
}

int delete_file(const char *filename) {
    char path[MAX_PATH_LEN];
    snprintf(path, MAX_PATH_LEN, "%s%s%s", fs.root_dir, PATH_SEP, filename);
    
    return remove(path);
}

void scan_filesystem(void) {
    for (int i = 0; i < fs.file_count; i++) {
        if (fs.files[i].data) free(fs.files[i].data);
    }
    fs.file_count = 0;
    
#ifdef _WIN32
    WIN32_FIND_DATA find_data;
    char search_path[MAX_PATH_LEN];
    snprintf(search_path, MAX_PATH_LEN, "%s%s*", fs.root_dir, PATH_SEP);
    
    HANDLE hFind = FindFirstFile(search_path, &find_data);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                load_file_from_disk(find_data.cFileName);
            }
        } while (FindNextFile(hFind, &find_data));
        FindClose(hFind);
    }
#else
    DIR *dir = opendir(fs.root_dir);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type == DT_REG) {
                load_file_from_disk(entry->d_name);
            }
        }
        closedir(dir);
    }
#endif
}

// CPU functions
void init_cpu(void) {
    memset(&cpu, 0, sizeof(CPU));
    cpu.sp = STACK_SIZE - 1;
}

int load_program(const char *filename) {
    File *f = find_file(filename);
    if (!f) return -1;
    
    if (f->size > MEM_SIZE) {
        print_to_screen("Error: Program too large\n");
        return -1;
    }
    
    memcpy(cpu.memory, f->data, f->size);
    cpu.pc = 0;
    cpu.running = 1;
    return 0;
}

void execute_instruction(void) {
    if (cpu.pc >= MEM_SIZE) {
        cpu.running = 0;
        return;
    }
    
    uint8_t opcode = cpu.memory[cpu.pc++];
    
    switch (opcode) {
        case 0x00: // HALT
            cpu.running = 0;
            break;
        case 0x01: // PRINT_CHAR
            if (cpu.pc < MEM_SIZE) {
                char c = cpu.memory[cpu.pc++];
                putchar_screen(c);
            }
            break;
        case 0x02: // PRINT_STR
            while (cpu.pc < MEM_SIZE && cpu.memory[cpu.pc]) {
                putchar_screen(cpu.memory[cpu.pc++]);
            }
            cpu.pc++;
            break;
        case 0x04: // CLEAR_SCREEN
            clear_screen_display();
            break;
        case 0x20: // SLEEP_MS
            if (cpu.pc + 1 < MEM_SIZE) {
                uint16_t ms = cpu.memory[cpu.pc++];
                ms |= (cpu.memory[cpu.pc++] << 8);
#ifdef _WIN32
                Sleep(ms);
#else
                usleep(ms * 1000);
#endif
            }
            break;
        default: {
            char msg[64];
            snprintf(msg, sizeof(msg), "Error: Unknown opcode 0x%02X\n", opcode);
            print_to_screen(msg);
            cpu.running = 0;
            break;
        }
    }
}

void run_program(void) {
    os_mode = 0;
    while (cpu.running && cpu.pc < MEM_SIZE) {
        execute_instruction();
    }
    os_mode = 1;
}

// Command implementations
void cmd_help(void) {
    print_to_screen("\nAvailable commands:\n");
    print_to_screen("  help           - Display this help message\n");
    print_to_screen("  clear          - Clear the screen\n");
    print_to_screen("  ls             - List files in current directory\n");
    print_to_screen("  cat <file>     - Display file contents\n");
    print_to_screen("  rm <file>      - Delete a file\n");
    print_to_screen("  cp <src> <dst> - Copy a file\n");
    print_to_screen("  mv <src> <dst> - Move/rename a file\n");
    print_to_screen("  echo <text>    - Print text to screen\n");
    print_to_screen("  date           - Show current date and time\n");
    print_to_screen("  uptime         - Show system uptime\n");
    print_to_screen("  meminfo        - Display memory information\n");
    print_to_screen("  run <file>     - Execute a binary program\n");
    print_to_screen("  hexdump <file> - Display hexadecimal dump of file\n");
    print_to_screen("  history        - Show command history\n");
    print_to_screen("  exit           - Exit the system\n");
    print_to_screen("\n");
}

void cmd_ls(void) {
    if (fs.file_count == 0) {
        print_to_screen("No files found.\n");
        return;
    }
    
    print_to_screen("\n");
    char line[128];
    for (int i = 0; i < fs.file_count; i++) {
        struct tm *tm_info = localtime(&fs.files[i].modified);
        char time_str[64];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M", tm_info);
        
        snprintf(line, sizeof(line), "%-20s %8zu bytes  %s\n", 
                 fs.files[i].name, fs.files[i].size, time_str);
        print_to_screen(line);
    }
    print_to_screen("\n");
}

void cmd_cat(const char *filename) {
    File *f = find_file(filename);
    if (!f) {
        print_to_screen("Error: File not found\n");
        return;
    }
    
    print_to_screen("\n");
    for (size_t i = 0; i < f->size; i++) {
        if (f->data[i] >= 32 && f->data[i] < 127) {
            putchar_screen(f->data[i]);
        } else if (f->data[i] == '\n' || f->data[i] == '\r' || f->data[i] == '\t') {
            putchar_screen(f->data[i]);
        } else {
            putchar_screen('.');
        }
    }
    print_to_screen("\n\n");
}

void cmd_rm(const char *filename) {
    File *f = find_file(filename);
    if (!f) {
        print_to_screen("Error: File not found\n");
        return;
    }
    
    if (delete_file(filename) == 0) {
        print_to_screen("File deleted.\n");
        scan_filesystem();
    } else {
        print_to_screen("Error: Could not delete file\n");
    }
}

void cmd_cp(const char *src, const char *dst) {
    File *f = find_file(src);
    if (!f) {
        print_to_screen("Error: Source file not found\n");
        return;
    }
    
    if (write_file(dst, f->data, f->size) == 0) {
        print_to_screen("File copied.\n");
        scan_filesystem();
    } else {
        print_to_screen("Error: Could not copy file\n");
    }
}

void cmd_mv(const char *src, const char *dst) {
    cmd_cp(src, dst);
    cmd_rm(src);
}

void cmd_date(void) {
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    char buffer[128];
    
    strftime(buffer, sizeof(buffer), "%A, %B %d, %Y %H:%M:%S\n", tm_info);
    print_to_screen(buffer);
}

time_t boot_time;

void cmd_uptime(void) {
    time_t now = time(NULL);
    time_t uptime = now - boot_time;
    
    int hours = uptime / 3600;
    int minutes = (uptime % 3600) / 60;
    int seconds = uptime % 60;
    
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "Uptime: %d hours, %d minutes, %d seconds\n", hours, minutes, seconds);
    print_to_screen(buffer);
}

void cmd_meminfo(void) {
    char buffer[128];
    print_to_screen("\nMemory Information:\n");
    snprintf(buffer, sizeof(buffer), "  Total Memory: %d KB\n", MEM_SIZE / 1024);
    print_to_screen(buffer);
    snprintf(buffer, sizeof(buffer), "  Stack Size: %d bytes\n", STACK_SIZE);
    print_to_screen(buffer);
    snprintf(buffer, sizeof(buffer), "  Program Counter: 0x%04X\n", cpu.pc);
    print_to_screen(buffer);
    snprintf(buffer, sizeof(buffer), "  Stack Pointer: 0x%04X\n", cpu.sp);
    print_to_screen(buffer);
    print_to_screen("\n");
}

void cmd_hexdump(const char *filename) {
    File *f = find_file(filename);
    if (!f) {
        print_to_screen("Error: File not found\n");
        return;
    }
    
    print_to_screen("\n");
    char line[128];
    for (size_t i = 0; i < f->size; i += 16) {
        snprintf(line, sizeof(line), "%04zx: ", i);
        print_to_screen(line);
        
        for (size_t j = 0; j < 16 && i + j < f->size; j++) {
            snprintf(line, sizeof(line), "%02x ", f->data[i + j]);
            print_to_screen(line);
        }
        
        print_to_screen(" | ");
        
        for (size_t j = 0; j < 16 && i + j < f->size; j++) {
            char c = f->data[i + j];
            if (c >= 32 && c < 127) {
                putchar_screen(c);
            } else {
                putchar_screen('.');
            }
        }
        
        putchar_screen('\n');
    }
    print_to_screen("\n");
}

void add_to_history(const char *cmd) {
    if (history.count < MAX_HISTORY) {
        strcpy(history.commands[history.count++], cmd);
    } else {
        for (int i = 0; i < MAX_HISTORY - 1; i++) {
            strcpy(history.commands[i], history.commands[i + 1]);
        }
        strcpy(history.commands[MAX_HISTORY - 1], cmd);
    }
}

void cmd_history(void) {
    print_to_screen("\nCommand History:\n");
    for (int i = 0; i < history.count; i++) {
        char line[INPUT_BUFFER_SIZE + 16];
        snprintf(line, sizeof(line), "  %d: %s\n", i + 1, history.commands[i]);
        print_to_screen(line);
    }
    print_to_screen("\n");
}

void print_prompt(void) {
    print_to_screen("$ ");
}

void shell_loop(void) {
    char cmd[256];
    
    print_to_screen("MicroOS v1.0\n");
    print_to_screen("Type 'help' for available commands.\n\n");
    
    while (window_running) {
        print_prompt();
        
        char *line = read_line_from_screen();
        if (!window_running) break;
        
        strncpy(cmd, line, sizeof(cmd) - 1);
        cmd[sizeof(cmd) - 1] = '\0';
        
        // Trim whitespace
        int len = strlen(cmd);
        while (len > 0 && isspace(cmd[len - 1])) {
            cmd[--len] = '\0';
        }
        
        if (len == 0) continue;
        
        add_to_history(cmd);
        
        // Parse command
        char *token = strtok(cmd, " ");
        if (!token) continue;
        
        if (strcmp(token, "exit") == 0) {
            print_to_screen("Goodbye!\n");
            usleep(500000);
            break;
        } else if (strcmp(token, "help") == 0) {
            cmd_help();
        } else if (strcmp(token, "clear") == 0) {
            clear_screen_display();
        } else if (strcmp(token, "ls") == 0) {
            cmd_ls();
        } else if (strcmp(token, "cat") == 0) {
            char *filename = strtok(NULL, " ");
            if (filename) {
                cmd_cat(filename);
            } else {
                print_to_screen("Usage: cat <filename>\n");
            }
        } else if (strcmp(token, "rm") == 0) {
            char *filename = strtok(NULL, " ");
            if (filename) {
                cmd_rm(filename);
            } else {
                print_to_screen("Usage: rm <filename>\n");
            }
        } else if (strcmp(token, "cp") == 0) {
            char *src = strtok(NULL, " ");
            char *dst = strtok(NULL, " ");
            if (src && dst) {
                cmd_cp(src, dst);
            } else {
                print_to_screen("Usage: cp <source> <destination>\n");
            }
        } else if (strcmp(token, "mv") == 0) {
            char *src = strtok(NULL, " ");
            char *dst = strtok(NULL, " ");
            if (src && dst) {
                cmd_mv(src, dst);
            } else {
                print_to_screen("Usage: mv <source> <destination>\n");
            }
        } else if (strcmp(token, "echo") == 0) {
            char *text = strtok(NULL, "");
            if (text) {
                print_to_screen(text);
                putchar_screen('\n');
            } else {
                putchar_screen('\n');
            }
        } else if (strcmp(token, "date") == 0) {
            cmd_date();
        } else if (strcmp(token, "uptime") == 0) {
            cmd_uptime();
        } else if (strcmp(token, "meminfo") == 0) {
            cmd_meminfo();
        } else if (strcmp(token, "hexdump") == 0) {
            char *filename = strtok(NULL, " ");
            if (filename) {
                cmd_hexdump(filename);
            } else {
                print_to_screen("Usage: hexdump <filename>\n");
            }
        } else if (strcmp(token, "history") == 0) {
            cmd_history();
        } else if (strcmp(token, "run") == 0) {
            char *filename = strtok(NULL, " ");
            if (filename) {
                init_cpu();
                if (load_program(filename) == 0) {
                    print_to_screen("Running program...\n");
                    run_program();
                    print_to_screen("Program terminated.\n");
                } else {
                    print_to_screen("Error: Could not load program\n");
                }
            } else {
                print_to_screen("Usage: run <filename>\n");
            }
        } else {
            print_to_screen("Unknown command: ");
            print_to_screen(token);
            print_to_screen("\n");
        }
    }
}

int main(int argc, char *argv[]) {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);
#endif
    
    boot_time = time(NULL);
    
    pthread_mutex_init(&input_buf.mutex, NULL);
    input_buf.pos = 0;
    input_buf.ready = 0;
    
    memset(&history, 0, sizeof(History));
    
    init_screen();
    init_filesystem();
    init_cpu();
    
    printf("MicroComputer Emulator\n");
    printf("======================\n");
    printf("Filesystem: %s\n", fs.root_dir);
    printf("Loading files...\n");
    scan_filesystem();
    printf("Loaded %d files.\n\n", fs.file_count);
    printf("Starting display...\n");
    printf("All interaction in the display window!\n\n");
    
    pthread_t thread;
    pthread_create(&thread, NULL, window_thread, NULL);
    
#ifdef _WIN32
    Sleep(1000);
#else
    usleep(1000000);
#endif
    
    shell_loop();
    
    window_running = 0;
    pthread_join(thread, NULL);
    pthread_mutex_destroy(&input_buf.mutex);
    
#ifdef _WIN32
    WSACleanup();
#endif
    
    return 0;
}
