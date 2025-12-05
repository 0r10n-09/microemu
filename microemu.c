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
#include <math.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h>
    #include <direct.h>
    #define mkdir(path, mode) _mkdir(path)
    #define PATH_SEP "\\"
    #define sleep_ms(ms) Sleep(ms)
    #define sleep_us(us) Sleep((us) / 1000)
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
    #define sleep_ms(ms) usleep((ms) * 1000)
    #define sleep_us(us) usleep(us)
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

// Pixel graphics mode
#define PIXEL_WIDTH 320
#define PIXEL_HEIGHT 200

// Opcodes
#define OP_HALT         0x00
#define OP_PRINT_CHAR   0x01
#define OP_PRINT_STR    0x02
#define OP_CLEAR_SCREEN 0x04
#define OP_SET_COLOR    0x05
#define OP_GET_CURSOR   0x06
#define OP_SET_CURSOR   0x07
#define OP_DRAW_LINE    0x08
#define OP_DRAW_RECT    0x09
#define OP_FILL_RECT    0x0A
#define OP_DRAW_CIRCLE  0x0B
#define OP_SLEEP_MS     0x20
#define OP_BEEP         0x21
#define OP_GET_TIME     0x22
#define OP_RANDOM       0x23
#define OP_SET_PIXEL    0x30
#define OP_CLEAR_PIXELS 0x31
#define OP_DRAW_SPRITE  0x32
#define OP_LOAD_REG     0x40
#define OP_STORE_REG    0x41
#define OP_PUSH         0x42
#define OP_POP          0x43
#define OP_ADD          0x50
#define OP_SUB          0x51
#define OP_MUL          0x52
#define OP_DIV          0x53
#define OP_MOD          0x54
#define OP_AND          0x55
#define OP_OR           0x56
#define OP_XOR          0x57
#define OP_NOT          0x58
#define OP_SHL          0x59
#define OP_SHR          0x5A
#define OP_CMP          0x5B
#define OP_JMP          0x60
#define OP_JZ           0x61
#define OP_JNZ          0x62
#define OP_JG           0x63
#define OP_JL           0x64
#define OP_CALL         0x65
#define OP_RET          0x66
#define OP_READ_CHAR    0x70
#define OP_KEY_PRESSED  0x71
#define OP_LOAD_MEM     0x80
#define OP_STORE_MEM    0x81
#define OP_COPY_MEM     0x82

// Color codes
#define COLOR_BLACK     0
#define COLOR_BLUE      1
#define COLOR_GREEN     2
#define COLOR_CYAN      3
#define COLOR_RED       4
#define COLOR_MAGENTA   5
#define COLOR_YELLOW    6
#define COLOR_WHITE     7
#define COLOR_GRAY      8
#define COLOR_BRIGHT_BLUE    9
#define COLOR_BRIGHT_GREEN   10
#define COLOR_BRIGHT_CYAN    11
#define COLOR_BRIGHT_RED     12
#define COLOR_BRIGHT_MAGENTA 13
#define COLOR_BRIGHT_YELLOW  14
#define COLOR_BRIGHT_WHITE   15

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
    uint8_t colors[SCREEN_HEIGHT][SCREEN_WIDTH]; // Color attributes
    uint8_t pixels[PIXEL_HEIGHT][PIXEL_WIDTH];
    int cursor_x;
    int cursor_y;
    int cursor_visible;
    int dirty;
    int pixel_mode;
    uint8_t current_color;
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
    char last_char;
    int char_ready;
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
time_t boot_time;

#ifdef _WIN32
HWND hwnd;
HDC hdc_mem;
HBITMAP hbm_mem;
HFONT hfont;

COLORREF get_color_rgb(uint8_t color) {
    switch(color) {
        case COLOR_BLACK:    return RGB(0, 0, 0);
        case COLOR_BLUE:     return RGB(0, 0, 170);
        case COLOR_GREEN:    return RGB(0, 170, 0);
        case COLOR_CYAN:     return RGB(0, 170, 170);
        case COLOR_RED:      return RGB(170, 0, 0);
        case COLOR_MAGENTA:  return RGB(170, 0, 170);
        case COLOR_YELLOW:   return RGB(170, 85, 0);
        case COLOR_WHITE:    return RGB(170, 170, 170);
        case COLOR_GRAY:     return RGB(85, 85, 85);
        case COLOR_BRIGHT_BLUE:    return RGB(85, 85, 255);
        case COLOR_BRIGHT_GREEN:   return RGB(85, 255, 85);
        case COLOR_BRIGHT_CYAN:    return RGB(85, 255, 255);
        case COLOR_BRIGHT_RED:     return RGB(255, 85, 85);
        case COLOR_BRIGHT_MAGENTA: return RGB(255, 85, 255);
        case COLOR_BRIGHT_YELLOW:  return RGB(255, 255, 85);
        case COLOR_BRIGHT_WHITE:   return RGB(255, 255, 255);
        default: return RGB(170, 170, 170);
    }
}

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
                input_buf.last_char = '\n';
                input_buf.char_ready = 1;
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
                    input_buf.last_char = (char)wParam;
                    input_buf.char_ready = 1;
                    if (screen.cursor_x < SCREEN_WIDTH) {
                        screen.chars[screen.cursor_y][screen.cursor_x++] = (char)wParam;
                        screen.dirty = 1;
                    }
                }
                pthread_mutex_unlock(&input_buf.mutex);
            }
            return 0;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            HFONT old_font = (HFONT)SelectObject(hdc_mem, hfont);
            
            if (screen.pixel_mode) {
                for (int y = 0; y < PIXEL_HEIGHT; y++) {
                    for (int x = 0; x < PIXEL_WIDTH; x++) {
                        int px = x * CHAR_WIDTH * SCREEN_WIDTH / PIXEL_WIDTH;
                        int py = y * CHAR_HEIGHT * SCREEN_HEIGHT / PIXEL_HEIGHT;
                        COLORREF color = screen.pixels[y][x] ? RGB(100, 200, 255) : RGB(0, 0, 0);
                        SetPixel(hdc_mem, px, py, color);
                    }
                }
            } else {
                for (int y = 0; y < SCREEN_HEIGHT; y++) {
                    for (int x = 0; x < SCREEN_WIDTH; x++) {
                        RECT rect = {x * CHAR_WIDTH, y * CHAR_HEIGHT, 
                                    (x + 1) * CHAR_WIDTH, (y + 1) * CHAR_HEIGHT};
                        
                        HBRUSH brush = CreateSolidBrush(RGB(0, 0, 0));
                        FillRect(hdc_mem, &rect, brush);
                        DeleteObject(brush);
                        
                        COLORREF text_color = get_color_rgb(screen.colors[y][x]);
                        SetTextColor(hdc_mem, text_color);
                        SetBkMode(hdc_mem, TRANSPARENT);
                        
                        char c = screen.chars[y][x];
                        if (x == screen.cursor_x && y == screen.cursor_y && screen.cursor_visible) {
                            c = '_';
                        }
                        TextOutA(hdc_mem, x * CHAR_WIDTH, y * CHAR_HEIGHT, &c, 1);
                    }
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
    (void)arg;
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "MicroEmuClass";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    
    RegisterClassA(&wc);
    
    int win_width = SCREEN_WIDTH * CHAR_WIDTH + 16;
    int win_height = SCREEN_HEIGHT * CHAR_HEIGHT + 39;
    
    hwnd = CreateWindowExA(0, "MicroEmuClass", "MicroComputer",
                         WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
                         CW_USEDEFAULT, CW_USEDEFAULT, win_width, win_height,
                         NULL, NULL, GetModuleHandle(NULL), NULL);
    
    if (!hwnd) {
        fprintf(stderr, "Failed to create window\n");
        return NULL;
    }
    
    HDC hdc = GetDC(hwnd);
    hdc_mem = CreateCompatibleDC(hdc);
    hbm_mem = CreateCompatibleBitmap(hdc, SCREEN_WIDTH * CHAR_WIDTH, 
                                     SCREEN_HEIGHT * CHAR_HEIGHT);
    SelectObject(hdc_mem, hbm_mem);
    ReleaseDC(hwnd, hdc);
    
    hfont = CreateFontA(16, 8, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                      DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Courier New");
    
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    
    MSG msg;
    while (window_running) {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                window_running = 0;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        
        if (!window_running) break;
        
        if (screen.dirty) {
            InvalidateRect(hwnd, NULL, FALSE);
            UpdateWindow(hwnd);
            screen.dirty = 0;
        }
        
        Sleep(16);
    }
    
    return NULL;
}

#else
Display *display;
Window window;
GC gc;
XFontStruct *font;
XColor xcolors[16];

unsigned long get_x11_color(uint8_t color) {
    if (color < 16) {
        return xcolors[color].pixel;
    }
    return xcolors[COLOR_WHITE].pixel;
}

void init_x11_colors(void) {
    Colormap colormap = DefaultColormap(display, DefaultScreen(display));
    
    const char *color_names[] = {
        "#000000", "#0000AA", "#00AA00", "#00AAAA",
        "#AA0000", "#AA00AA", "#AA5500", "#AAAAAA",
        "#555555", "#5555FF", "#55FF55", "#55FFFF",
        "#FF5555", "#FF55FF", "#FFFF55", "#FFFFFF"
    };
    
    for (int i = 0; i < 16; i++) {
        XParseColor(display, colormap, color_names[i], &xcolors[i]);
        XAllocColor(display, colormap, &xcolors[i]);
    }
}

void* window_thread(void* arg) {
    (void)arg;
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
    
    init_x11_colors();
    
    XMapWindow(display, window);
    XFlush(display);
    
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
                    input_buf.last_char = '\n';
                    input_buf.char_ready = 1;
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
                        input_buf.last_char = buf[0];
                        input_buf.char_ready = 1;
                        if (screen.cursor_x < SCREEN_WIDTH) {
                            screen.chars[screen.cursor_y][screen.cursor_x++] = buf[0];
                            screen.dirty = 1;
                        }
                    }
                }
                
                pthread_mutex_unlock(&input_buf.mutex);
            }
        }
        
        if (screen.dirty) {
            XSetForeground(display, gc, BlackPixel(display, DefaultScreen(display)));
            XFillRectangle(display, window, gc, 0, 0, 
                          SCREEN_WIDTH * CHAR_WIDTH, SCREEN_HEIGHT * CHAR_HEIGHT);
            
            if (screen.pixel_mode) {
                XSetForeground(display, gc, get_x11_color(COLOR_BRIGHT_CYAN));
                for (int y = 0; y < PIXEL_HEIGHT; y++) {
                    for (int x = 0; x < PIXEL_WIDTH; x++) {
                        if (screen.pixels[y][x]) {
                            int px = x * CHAR_WIDTH * SCREEN_WIDTH / PIXEL_WIDTH;
                            int py = y * CHAR_HEIGHT * SCREEN_HEIGHT / PIXEL_HEIGHT;
                            XDrawPoint(display, window, gc, px, py);
                        }
                    }
                }
            } else {
                for (int y = 0; y < SCREEN_HEIGHT; y++) {
                    for (int x = 0; x < SCREEN_WIDTH; x++) {
                        char c = screen.chars[y][x];
                        if (x == screen.cursor_x && y == screen.cursor_y && screen.cursor_visible) {
                            c = '_';
                        }
                        if (c != ' ') {
                            XSetForeground(display, gc, get_x11_color(screen.colors[y][x]));
                            XDrawString(display, window, gc, 
                                      x * CHAR_WIDTH, y * CHAR_HEIGHT + 12,
                                      &c, 1);
                        }
                    }
                }
            }
            XFlush(display);
            screen.dirty = 0;
        }
        
        sleep_us(16000);
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
            screen.colors[y][x] = COLOR_WHITE;
        }
    }
    screen.cursor_visible = 1;
    screen.dirty = 1;
    screen.pixel_mode = 0;
    screen.current_color = COLOR_WHITE;
}

void clear_screen_display(void) {
    init_screen();
}

void clear_pixels(void) {
    memset(screen.pixels, 0, sizeof(screen.pixels));
    screen.dirty = 1;
}

void set_pixel(int x, int y, int value) {
    if (x >= 0 && x < PIXEL_WIDTH && y >= 0 && y < PIXEL_HEIGHT) {
        screen.pixels[y][x] = value ? 1 : 0;
        screen.dirty = 1;
    }
}

void draw_line(int x0, int y0, int x1, int y1) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, e2;
    
    while (1) {
        set_pixel(x0, y0, 1);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void draw_rect(int x, int y, int w, int h) {
    for (int i = 0; i < w; i++) {
        set_pixel(x + i, y, 1);
        set_pixel(x + i, y + h - 1, 1);
    }
    for (int i = 0; i < h; i++) {
        set_pixel(x, y + i, 1);
        set_pixel(x + w - 1, y + i, 1);
    }
}

void fill_rect(int x, int y, int w, int h) {
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            set_pixel(x + j, y + i, 1);
        }
    }
}

void draw_circle(int cx, int cy, int radius) {
    int x = radius, y = 0;
    int err = 0;
    
    while (x >= y) {
        set_pixel(cx + x, cy + y, 1);
        set_pixel(cx + y, cy + x, 1);
        set_pixel(cx - y, cy + x, 1);
        set_pixel(cx - x, cy + y, 1);
        set_pixel(cx - x, cy - y, 1);
        set_pixel(cx - y, cy - x, 1);
        set_pixel(cx + y, cy - x, 1);
        set_pixel(cx + x, cy - y, 1);
        
        if (err <= 0) {
            y += 1;
            err += 2*y + 1;
        }
        if (err > 0) {
            x -= 1;
            err -= 2*x + 1;
        }
    }
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
                memcpy(screen.colors[y], screen.colors[y+1], SCREEN_WIDTH);
            }
            memset(screen.chars[SCREEN_HEIGHT-1], ' ', SCREEN_WIDTH);
            for (int x = 0; x < SCREEN_WIDTH; x++) {
                screen.colors[SCREEN_HEIGHT-1][x] = screen.current_color;
            }
            screen.cursor_y = SCREEN_HEIGHT - 1;
        }
        screen.chars[screen.cursor_y][screen.cursor_x] = c;
        screen.colors[screen.cursor_y][screen.cursor_x] = screen.current_color;
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
        sleep_ms(50);
    }
    
    pthread_mutex_lock(&input_buf.mutex);
    strcpy(line_buf, input_buf.buffer);
    input_buf.pos = 0;
    input_buf.ready = 0;
    pthread_mutex_unlock(&input_buf.mutex);
    
    putchar_screen('\n');
    
    return line_buf;
}

void play_beep(int freq, int duration) {
    (void)freq;
    (void)duration;
#ifdef _WIN32
    Beep(freq, duration);
#else
    printf("\a");
    fflush(stdout);
#endif
}

void show_boot_animation(void) {
    // Cool pixel-based boot animation
    clear_pixels();
    screen.pixel_mode = 1;
    screen.dirty = 1;
    
    // Draw expanding circle animation
    int cx = 160, cy = 100;
    for (int radius = 5; radius <= 60; radius += 3) {
        clear_pixels();
        for (int angle = 0; angle < 360; angle += 3) {
            double rad = angle * 3.14159 / 180.0;
            int x = cx + (int)(radius * cos(rad));
            int y = cy + (int)(radius * sin(rad));
            set_pixel(x, y, 1);
        }
        screen.dirty = 1;
        sleep_ms(30);
    }
    
    // Flash effect
    for (int i = 0; i < 3; i++) {
        clear_pixels();
        screen.dirty = 1;
        sleep_ms(100);
        for (int y = 0; y < PIXEL_HEIGHT; y++) {
            for (int x = 0; x < PIXEL_WIDTH; x++) {
                if ((x + y) % 20 == 0) set_pixel(x, y, 1);
            }
        }
        screen.dirty = 1;
        sleep_ms(100);
    }
    
    screen.dirty = 1;
    sleep_ms(1000);
    
    // Return to text mode
    clear_screen_display();
    screen.pixel_mode = 0;
    
    const char *spinner = "-\\|/";
    
    screen.current_color = COLOR_BRIGHT_YELLOW;
    print_to_screen("\n\n");
    print_to_screen("    +--------------------------------------+\n");
    print_to_screen("    |");
    screen.current_color = COLOR_BRIGHT_WHITE;
    print_to_screen("  MicroComputer Emulator v1.0       ");
    screen.current_color = COLOR_BRIGHT_YELLOW;
    print_to_screen("  |\n");
    print_to_screen("    |");
    screen.current_color = COLOR_CYAN;
    print_to_screen("  64KB RAM - 8 Registers - Graphics   ");
    screen.current_color = COLOR_BRIGHT_YELLOW;
    print_to_screen("|\n");
    print_to_screen("    +--------------------------------------+\n");
    screen.dirty = 1;
    sleep_ms(500);
    
    screen.current_color = COLOR_GREEN;
    print_to_screen("\n    Initializing");
    screen.dirty = 1;
    for (int i = 0; i < 12; i++) {
        char s[2] = {spinner[i % 4], 0};
        print_to_screen(s);
        screen.dirty = 1;
        sleep_ms(80);
        putchar_screen('\b');
    }
    
    print_to_screen("\n\n    [");
    screen.current_color = COLOR_BRIGHT_GREEN;
    for (int i = 0; i <= 30; i++) {
        print_to_screen("=");
        screen.dirty = 1;
        sleep_ms(25);
    }
    screen.current_color = COLOR_GREEN;
    print_to_screen("]\n");
    screen.dirty = 1;
    sleep_ms(300);
    
    screen.current_color = COLOR_BRIGHT_GREEN;
    print_to_screen("\n    > System Ready\n");
    screen.current_color = COLOR_WHITE;
    screen.dirty = 1;
    sleep_ms(500);
}

void show_loading_animation(const char *filename) {
    // Switch to pixel mode for cool loading animation
    screen.pixel_mode = 1;
    clear_pixels();
    screen.dirty = 1;
    
    // Draw loading bar frame
    for (int x = 60; x < 260; x++) {
        set_pixel(x, 90, 1);
        set_pixel(x, 110, 1);
    }
    for (int y = 90; y <= 110; y++) {
        set_pixel(60, y, 1);
        set_pixel(260, y, 1);
    }
    screen.dirty = 1;
    sleep_ms(200);
    
    // Animated loading bar with wave effect
    for (int pass = 0; pass < 2; pass++) {
        for (int x = 62; x < 258; x += 2) {
            // Fill bar
            for (int y = 92; y < 109; y++) {
                set_pixel(x, y, 1);
                set_pixel(x + 1, y, 1);
            }
            
            // Wave effect behind
            if (x > 70) {
                int wave_x = x - 10;
                for (int offset = -3; offset <= 3; offset++) {
                    int wave_y = 100 + (int)(3 * sin((wave_x + offset * 10) * 0.3));
                    if (wave_y >= 92 && wave_y < 109) {
                        set_pixel(wave_x, wave_y, 1);
                    }
                }
            }
            
            screen.dirty = 1;
            sleep_ms(8);
        }
    }
    
    // Flash effect
    for (int i = 0; i < 3; i++) {
        clear_pixels();
        screen.dirty = 1;
        sleep_ms(50);
        
        // Draw filled box
        for (int x = 60; x < 260; x++) {
            for (int y = 90; y <= 110; y++) {
                set_pixel(x, y, 1);
            }
        }
        screen.dirty = 1;
        sleep_ms(50);
    }
    
    sleep_ms(200);
    
    // Return to text mode
    clear_screen_display();
    screen.pixel_mode = 0;
    
    screen.current_color = COLOR_BRIGHT_CYAN;
    print_to_screen("\n    Loading: ");
    screen.current_color = COLOR_BRIGHT_YELLOW;
    print_to_screen(filename);
    screen.current_color = COLOR_BRIGHT_GREEN;
    print_to_screen(" [OK]\n");
    screen.current_color = COLOR_WHITE;
    screen.dirty = 1;
    sleep_ms(300);
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
    
    if (size < 0) {
        fclose(f);
        return -1;
    }
    
    uint8_t *data = (uint8_t*)malloc(size);
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
    if (stat(path, &st) == 0) {
        strncpy(fs.files[fs.file_count].name, filename, 63);
        fs.files[fs.file_count].name[63] = '\0';
        fs.files[fs.file_count].data = data;
        fs.files[fs.file_count].size = size;
        fs.files[fs.file_count].modified = st.st_mtime;
        fs.file_count++;
        return 0;
    }
    
    free(data);
    return -1;
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
    WIN32_FIND_DATAA find_data;
    char search_path[MAX_PATH_LEN];
    snprintf(search_path, MAX_PATH_LEN, "%s%s*", fs.root_dir, PATH_SEP);
    
    HANDLE hFind = FindFirstFileA(search_path, &find_data);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                load_file_from_disk(find_data.cFileName);
            }
        } while (FindNextFileA(hFind, &find_data));
        FindClose(hFind);
    }
#else
    DIR *dir = opendir(fs.root_dir);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type == DT_REG || entry->d_type == DT_UNKNOWN) {
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
        case OP_HALT:
            cpu.running = 0;
            break;
        case OP_PRINT_CHAR:
            if (cpu.pc < MEM_SIZE) {
                char c = cpu.memory[cpu.pc++];
                putchar_screen(c);
            }
            break;
        case OP_PRINT_STR:
            while (cpu.pc < MEM_SIZE && cpu.memory[cpu.pc]) {
                putchar_screen(cpu.memory[cpu.pc++]);
            }
            cpu.pc++;
            break;
        case OP_CLEAR_SCREEN:
            clear_screen_display();
            break;
        case OP_SET_COLOR:
            if (cpu.pc < MEM_SIZE) {
                uint8_t color = cpu.memory[cpu.pc++];
                if (color < 16) {
                    screen.current_color = color;
                }
            }
            break;
        case OP_GET_CURSOR:
            if (cpu.pc + 1 < MEM_SIZE) {
                uint8_t reg_x = cpu.memory[cpu.pc++];
                uint8_t reg_y = cpu.memory[cpu.pc++];
                if (reg_x < 8) cpu.regs[reg_x] = screen.cursor_x;
                if (reg_y < 8) cpu.regs[reg_y] = screen.cursor_y;
            }
            break;
        case OP_SET_CURSOR:
            if (cpu.pc + 1 < MEM_SIZE) {
                uint8_t x = cpu.memory[cpu.pc++];
                uint8_t y = cpu.memory[cpu.pc++];
                if (x < SCREEN_WIDTH) screen.cursor_x = x;
                if (y < SCREEN_HEIGHT) screen.cursor_y = y;
                screen.dirty = 1;
            }
            break;
        case OP_DRAW_LINE:
            if (cpu.pc + 7 < MEM_SIZE) {
                uint16_t x0 = cpu.memory[cpu.pc++] | (cpu.memory[cpu.pc++] << 8);
                uint16_t y0 = cpu.memory[cpu.pc++] | (cpu.memory[cpu.pc++] << 8);
                uint16_t x1 = cpu.memory[cpu.pc++] | (cpu.memory[cpu.pc++] << 8);
                uint16_t y1 = cpu.memory[cpu.pc++] | (cpu.memory[cpu.pc++] << 8);
                draw_line(x0, y0, x1, y1);
                screen.pixel_mode = 1;
            }
            break;
        case OP_DRAW_RECT:
            if (cpu.pc + 7 < MEM_SIZE) {
                uint16_t x = cpu.memory[cpu.pc++] | (cpu.memory[cpu.pc++] << 8);
                uint16_t y = cpu.memory[cpu.pc++] | (cpu.memory[cpu.pc++] << 8);
                uint16_t w = cpu.memory[cpu.pc++] | (cpu.memory[cpu.pc++] << 8);
                uint16_t h = cpu.memory[cpu.pc++] | (cpu.memory[cpu.pc++] << 8);
                draw_rect(x, y, w, h);
                screen.pixel_mode = 1;
            }
            break;
        case OP_FILL_RECT:
            if (cpu.pc + 7 < MEM_SIZE) {
                uint16_t x = cpu.memory[cpu.pc++] | (cpu.memory[cpu.pc++] << 8);
                uint16_t y = cpu.memory[cpu.pc++] | (cpu.memory[cpu.pc++] << 8);
                uint16_t w = cpu.memory[cpu.pc++] | (cpu.memory[cpu.pc++] << 8);
                uint16_t h = cpu.memory[cpu.pc++] | (cpu.memory[cpu.pc++] << 8);
                fill_rect(x, y, w, h);
                screen.pixel_mode = 1;
            }
            break;
        case OP_DRAW_CIRCLE:
            if (cpu.pc + 5 < MEM_SIZE) {
                uint16_t cx = cpu.memory[cpu.pc++] | (cpu.memory[cpu.pc++] << 8);
                uint16_t cy = cpu.memory[cpu.pc++] | (cpu.memory[cpu.pc++] << 8);
                uint16_t r = cpu.memory[cpu.pc++] | (cpu.memory[cpu.pc++] << 8);
                draw_circle(cx, cy, r);
                screen.pixel_mode = 1;
            }
            break;
        case OP_SLEEP_MS:
            if (cpu.pc + 1 < MEM_SIZE) {
                uint16_t ms = cpu.memory[cpu.pc++];
                ms |= (cpu.memory[cpu.pc++] << 8);
                sleep_ms(ms);
            }
            break;
        case OP_BEEP:
            if (cpu.pc + 3 < MEM_SIZE) {
                uint16_t freq = cpu.memory[cpu.pc++];
                freq |= (cpu.memory[cpu.pc++] << 8);
                uint16_t duration = cpu.memory[cpu.pc++];
                duration |= (cpu.memory[cpu.pc++] << 8);
                play_beep(freq, duration);
            }
            break;
        case OP_GET_TIME:
            if (cpu.pc < MEM_SIZE) {
                uint8_t reg = cpu.memory[cpu.pc++];
                if (reg < 8) {
                    time_t now = time(NULL);
                    cpu.regs[reg] = (uint16_t)(now & 0xFFFF);
                }
            }
            break;
        case OP_RANDOM:
            if (cpu.pc + 1 < MEM_SIZE) {
                uint8_t reg = cpu.memory[cpu.pc++];
                uint16_t max = cpu.memory[cpu.pc++] | (cpu.memory[cpu.pc++] << 8);
                if (reg < 8) {
                    cpu.regs[reg] = rand() % (max + 1);
                }
            }
            break;
        case OP_SET_PIXEL:
            if (cpu.pc + 2 < MEM_SIZE) {
                uint16_t x = cpu.memory[cpu.pc++];
                x |= (cpu.memory[cpu.pc++] << 8);
                uint16_t y = cpu.memory[cpu.pc++];
                y |= (cpu.memory[cpu.pc++] << 8);
                uint8_t val = cpu.memory[cpu.pc++];
                set_pixel(x, y, val);
                screen.pixel_mode = 1;
            }
            break;
        case OP_CLEAR_PIXELS:
            clear_pixels();
            screen.pixel_mode = 0;
            break;
        case OP_LOAD_REG:
            if (cpu.pc + 2 < MEM_SIZE) {
                uint8_t reg = cpu.memory[cpu.pc++];
                uint16_t val = cpu.memory[cpu.pc++];
                val |= (cpu.memory[cpu.pc++] << 8);
                if (reg < 8) cpu.regs[reg] = val;
            }
            break;
        case OP_STORE_REG:
            if (cpu.pc < MEM_SIZE) {
                uint8_t reg = cpu.memory[cpu.pc++];
                if (reg < 8 && cpu.pc + 1 < MEM_SIZE) {
                    uint16_t addr = cpu.memory[cpu.pc++];
                    addr |= (cpu.memory[cpu.pc++] << 8);
                    if (addr + 1 < MEM_SIZE) {
                        cpu.memory[addr] = cpu.regs[reg] & 0xFF;
                        cpu.memory[addr + 1] = (cpu.regs[reg] >> 8) & 0xFF;
                    }
                }
            }
            break;
        case OP_PUSH:
            if (cpu.pc < MEM_SIZE) {
                uint8_t reg = cpu.memory[cpu.pc++];
                if (reg < 8 && cpu.sp > 0) {
                    cpu.memory[MEM_SIZE - STACK_SIZE + cpu.sp] = cpu.regs[reg] & 0xFF;
                    cpu.sp--;
                    cpu.memory[MEM_SIZE - STACK_SIZE + cpu.sp] = (cpu.regs[reg] >> 8) & 0xFF;
                    cpu.sp--;
                }
            }
            break;
        case OP_POP:
            if (cpu.pc < MEM_SIZE) {
                uint8_t reg = cpu.memory[cpu.pc++];
                if (reg < 8 && cpu.sp < STACK_SIZE - 1) {
                    cpu.sp++;
                    uint16_t val = cpu.memory[MEM_SIZE - STACK_SIZE + cpu.sp];
                    cpu.sp++;
                    val |= (cpu.memory[MEM_SIZE - STACK_SIZE + cpu.sp] << 8);
                    cpu.regs[reg] = val;
                }
            }
            break;
        case OP_ADD:
            if (cpu.pc + 2 < MEM_SIZE) {
                uint8_t dst = cpu.memory[cpu.pc++];
                uint8_t src1 = cpu.memory[cpu.pc++];
                uint8_t src2 = cpu.memory[cpu.pc++];
                if (dst < 8 && src1 < 8 && src2 < 8) {
                    cpu.regs[dst] = cpu.regs[src1] + cpu.regs[src2];
                }
            }
            break;
        case OP_SUB:
            if (cpu.pc + 2 < MEM_SIZE) {
                uint8_t dst = cpu.memory[cpu.pc++];
                uint8_t src1 = cpu.memory[cpu.pc++];
                uint8_t src2 = cpu.memory[cpu.pc++];
                if (dst < 8 && src1 < 8 && src2 < 8) {
                    cpu.regs[dst] = cpu.regs[src1] - cpu.regs[src2];
                }
            }
            break;
        case OP_MUL:
            if (cpu.pc + 2 < MEM_SIZE) {
                uint8_t dst = cpu.memory[cpu.pc++];
                uint8_t src1 = cpu.memory[cpu.pc++];
                uint8_t src2 = cpu.memory[cpu.pc++];
                if (dst < 8 && src1 < 8 && src2 < 8) {
                    cpu.regs[dst] = cpu.regs[src1] * cpu.regs[src2];
                }
            }
            break;
        case OP_DIV:
            if (cpu.pc + 2 < MEM_SIZE) {
                uint8_t dst = cpu.memory[cpu.pc++];
                uint8_t src1 = cpu.memory[cpu.pc++];
                uint8_t src2 = cpu.memory[cpu.pc++];
                if (dst < 8 && src1 < 8 && src2 < 8 && cpu.regs[src2] != 0) {
                    cpu.regs[dst] = cpu.regs[src1] / cpu.regs[src2];
                }
            }
            break;
        case OP_MOD:
            if (cpu.pc + 2 < MEM_SIZE) {
                uint8_t dst = cpu.memory[cpu.pc++];
                uint8_t src1 = cpu.memory[cpu.pc++];
                uint8_t src2 = cpu.memory[cpu.pc++];
                if (dst < 8 && src1 < 8 && src2 < 8 && cpu.regs[src2] != 0) {
                    cpu.regs[dst] = cpu.regs[src1] % cpu.regs[src2];
                }
            }
            break;
        case OP_AND:
            if (cpu.pc + 2 < MEM_SIZE) {
                uint8_t dst = cpu.memory[cpu.pc++];
                uint8_t src1 = cpu.memory[cpu.pc++];
                uint8_t src2 = cpu.memory[cpu.pc++];
                if (dst < 8 && src1 < 8 && src2 < 8) {
                    cpu.regs[dst] = cpu.regs[src1] & cpu.regs[src2];
                }
            }
            break;
        case OP_OR:
            if (cpu.pc + 2 < MEM_SIZE) {
                uint8_t dst = cpu.memory[cpu.pc++];
                uint8_t src1 = cpu.memory[cpu.pc++];
                uint8_t src2 = cpu.memory[cpu.pc++];
                if (dst < 8 && src1 < 8 && src2 < 8) {
                    cpu.regs[dst] = cpu.regs[src1] | cpu.regs[src2];
                }
            }
            break;
        case OP_XOR:
            if (cpu.pc + 2 < MEM_SIZE) {
                uint8_t dst = cpu.memory[cpu.pc++];
                uint8_t src1 = cpu.memory[cpu.pc++];
                uint8_t src2 = cpu.memory[cpu.pc++];
                if (dst < 8 && src1 < 8 && src2 < 8) {
                    cpu.regs[dst] = cpu.regs[src1] ^ cpu.regs[src2];
                }
            }
            break;
        case OP_NOT:
            if (cpu.pc + 1 < MEM_SIZE) {
                uint8_t dst = cpu.memory[cpu.pc++];
                uint8_t src = cpu.memory[cpu.pc++];
                if (dst < 8 && src < 8) {
                    cpu.regs[dst] = ~cpu.regs[src];
                }
            }
            break;
        case OP_SHL:
            if (cpu.pc + 1 < MEM_SIZE) {
                uint8_t dst = cpu.memory[cpu.pc++];
                uint8_t src = cpu.memory[cpu.pc++];
                if (dst < 8 && src < 8) {
                    cpu.regs[dst] = cpu.regs[dst] << cpu.regs[src];
                }
            }
            break;
        case OP_SHR:
            if (cpu.pc + 1 < MEM_SIZE) {
                uint8_t dst = cpu.memory[cpu.pc++];
                uint8_t src = cpu.memory[cpu.pc++];
                if (dst < 8 && src < 8) {
                    cpu.regs[dst] = cpu.regs[dst] >> cpu.regs[src];
                }
            }
            break;
        case OP_CMP:
            if (cpu.pc + 1 < MEM_SIZE) {
                uint8_t src1 = cpu.memory[cpu.pc++];
                uint8_t src2 = cpu.memory[cpu.pc++];
                if (src1 < 8 && src2 < 8) {
                    cpu.flags = 0;
                    if (cpu.regs[src1] == cpu.regs[src2]) cpu.flags |= 0x01; // Zero
                    if (cpu.regs[src1] > cpu.regs[src2]) cpu.flags |= 0x02;  // Greater
                    if (cpu.regs[src1] < cpu.regs[src2]) cpu.flags |= 0x04;  // Less
                }
            }
            break;
        case OP_JMP:
            if (cpu.pc + 1 < MEM_SIZE) {
                uint16_t addr = cpu.memory[cpu.pc++];
                addr |= (cpu.memory[cpu.pc++] << 8);
                if (addr < MEM_SIZE) cpu.pc = addr;
            }
            break;
        case OP_JZ:
            if (cpu.pc + 1 < MEM_SIZE) {
                uint16_t addr = cpu.memory[cpu.pc++];
                addr |= (cpu.memory[cpu.pc++] << 8);
                if ((cpu.flags & 0x01) && addr < MEM_SIZE) cpu.pc = addr;
            }
            break;
        case OP_JNZ:
            if (cpu.pc + 1 < MEM_SIZE) {
                uint16_t addr = cpu.memory[cpu.pc++];
                addr |= (cpu.memory[cpu.pc++] << 8);
                if (!(cpu.flags & 0x01) && addr < MEM_SIZE) cpu.pc = addr;
            }
            break;
        case OP_JG:
            if (cpu.pc + 1 < MEM_SIZE) {
                uint16_t addr = cpu.memory[cpu.pc++];
                addr |= (cpu.memory[cpu.pc++] << 8);
                if ((cpu.flags & 0x02) && addr < MEM_SIZE) cpu.pc = addr;
            }
            break;
        case OP_JL:
            if (cpu.pc + 1 < MEM_SIZE) {
                uint16_t addr = cpu.memory[cpu.pc++];
                addr |= (cpu.memory[cpu.pc++] << 8);
                if ((cpu.flags & 0x04) && addr < MEM_SIZE) cpu.pc = addr;
            }
            break;
        case OP_CALL:
            if (cpu.pc + 1 < MEM_SIZE && cpu.sp > 1) {
                uint16_t addr = cpu.memory[cpu.pc++];
                addr |= (cpu.memory[cpu.pc++] << 8);
                // Push return address
                cpu.memory[MEM_SIZE - STACK_SIZE + cpu.sp] = cpu.pc & 0xFF;
                cpu.sp--;
                cpu.memory[MEM_SIZE - STACK_SIZE + cpu.sp] = (cpu.pc >> 8) & 0xFF;
                cpu.sp--;
                if (addr < MEM_SIZE) cpu.pc = addr;
            }
            break;
        case OP_RET:
            if (cpu.sp < STACK_SIZE - 1) {
                cpu.sp++;
                uint16_t addr = cpu.memory[MEM_SIZE - STACK_SIZE + cpu.sp];
                cpu.sp++;
                addr |= (cpu.memory[MEM_SIZE - STACK_SIZE + cpu.sp] << 8);
                if (addr < MEM_SIZE) cpu.pc = addr;
            }
            break;
        case OP_READ_CHAR:
            if (cpu.pc < MEM_SIZE) {
                uint8_t reg = cpu.memory[cpu.pc++];
                if (reg < 8) {
                    pthread_mutex_lock(&input_buf.mutex);
                    input_buf.char_ready = 0;
                    pthread_mutex_unlock(&input_buf.mutex);
                    
                    while (!input_buf.char_ready && window_running) {
                        sleep_ms(50);
                    }
                    
                    pthread_mutex_lock(&input_buf.mutex);
                    cpu.regs[reg] = input_buf.last_char;
                    pthread_mutex_unlock(&input_buf.mutex);
                }
            }
            break;
        case OP_LOAD_MEM:
            if (cpu.pc + 2 < MEM_SIZE) {
                uint8_t reg = cpu.memory[cpu.pc++];
                uint16_t addr = cpu.memory[cpu.pc++];
                addr |= (cpu.memory[cpu.pc++] << 8);
                if (reg < 8 && addr + 1 < MEM_SIZE) {
                    cpu.regs[reg] = cpu.memory[addr];
                    cpu.regs[reg] |= (cpu.memory[addr + 1] << 8);
                }
            }
            break;
        case OP_STORE_MEM:
            if (cpu.pc + 2 < MEM_SIZE) {
                uint16_t addr = cpu.memory[cpu.pc++];
                addr |= (cpu.memory[cpu.pc++] << 8);
                uint8_t reg = cpu.memory[cpu.pc++];
                if (reg < 8 && addr + 1 < MEM_SIZE) {
                    cpu.memory[addr] = cpu.regs[reg] & 0xFF;
                    cpu.memory[addr + 1] = (cpu.regs[reg] >> 8) & 0xFF;
                }
            }
            break;
        case OP_COPY_MEM:
            if (cpu.pc + 5 < MEM_SIZE) {
                uint16_t src = cpu.memory[cpu.pc++] | (cpu.memory[cpu.pc++] << 8);
                uint16_t dst = cpu.memory[cpu.pc++] | (cpu.memory[cpu.pc++] << 8);
                uint16_t len = cpu.memory[cpu.pc++] | (cpu.memory[cpu.pc++] << 8);
                if (src + len < MEM_SIZE && dst + len < MEM_SIZE) {
                    memmove(&cpu.memory[dst], &cpu.memory[src], len);
                }
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
    screen.current_color = COLOR_BRIGHT_YELLOW;
    print_to_screen("\nAvailable commands:\n");
    print_to_screen("===================\n");
    
    screen.current_color = COLOR_BRIGHT_CYAN;
    print_to_screen("\nFile Operations:\n");
    screen.current_color = COLOR_CYAN;
    print_to_screen("  ls, dir        ");
    screen.current_color = COLOR_WHITE;
    print_to_screen("- List files in current directory\n");
    screen.current_color = COLOR_CYAN;
    print_to_screen("  cat <file>     ");
    screen.current_color = COLOR_WHITE;
    print_to_screen("- Display file contents\n");
    screen.current_color = COLOR_CYAN;
    print_to_screen("  rm <file>      ");
    screen.current_color = COLOR_WHITE;
    print_to_screen("- Delete a file\n");
    screen.current_color = COLOR_CYAN;
    print_to_screen("  cp <src> <dst> ");
    screen.current_color = COLOR_WHITE;
    print_to_screen("- Copy a file\n");
    screen.current_color = COLOR_CYAN;
    print_to_screen("  mv <src> <dst> ");
    screen.current_color = COLOR_WHITE;
    print_to_screen("- Move/rename a file\n");
    screen.current_color = COLOR_CYAN;
    print_to_screen("  touch <file>   ");
    screen.current_color = COLOR_WHITE;
    print_to_screen("- Create empty file\n");
    screen.current_color = COLOR_CYAN;
    print_to_screen("  hexdump <file> ");
    screen.current_color = COLOR_WHITE;
    print_to_screen("- Display hexadecimal dump\n");
    
    screen.current_color = COLOR_BRIGHT_CYAN;
    print_to_screen("\nSystem Commands:\n");
    screen.current_color = COLOR_CYAN;
    print_to_screen("  help           ");
    screen.current_color = COLOR_WHITE;
    print_to_screen("- Display this help message\n");
    screen.current_color = COLOR_CYAN;
    print_to_screen("  clear, cls     ");
    screen.current_color = COLOR_WHITE;
    print_to_screen("- Clear the screen\n");
    screen.current_color = COLOR_CYAN;
    print_to_screen("  echo <text>    ");
    screen.current_color = COLOR_WHITE;
    print_to_screen("- Print text to screen\n");
    screen.current_color = COLOR_CYAN;
    print_to_screen("  sysinfo        ");
    screen.current_color = COLOR_WHITE;
    print_to_screen("- Display system information\n");
    screen.current_color = COLOR_CYAN;
    print_to_screen("  date           ");
    screen.current_color = COLOR_WHITE;
    print_to_screen("- Show current date and time\n");
    screen.current_color = COLOR_CYAN;
    print_to_screen("  uptime         ");
    screen.current_color = COLOR_WHITE;
    print_to_screen("- Show system uptime\n");
    screen.current_color = COLOR_CYAN;
    print_to_screen("  meminfo        ");
    screen.current_color = COLOR_WHITE;
    print_to_screen("- Display memory information\n");
    screen.current_color = COLOR_CYAN;
    print_to_screen("  history        ");
    screen.current_color = COLOR_WHITE;
    print_to_screen("- Show command history\n");
    
    screen.current_color = COLOR_BRIGHT_CYAN;
    print_to_screen("\nProgram Execution:\n");
    screen.current_color = COLOR_CYAN;
    print_to_screen("  run <file>     ");
    screen.current_color = COLOR_WHITE;
    print_to_screen("- Execute a binary program\n");
    
    screen.current_color = COLOR_BRIGHT_CYAN;
    print_to_screen("\nFun Commands:\n");
    screen.current_color = COLOR_CYAN;
    print_to_screen("  banner <text>  ");
    screen.current_color = COLOR_WHITE;
    print_to_screen("- Display large text banner\n");
    screen.current_color = COLOR_CYAN;
    print_to_screen("  color <0-15>   ");
    screen.current_color = COLOR_WHITE;
    print_to_screen("- Change terminal color\n");
    screen.current_color = COLOR_CYAN;
    print_to_screen("  matrix         ");
    screen.current_color = COLOR_WHITE;
    print_to_screen("- Matrix falling text effect\n");
    screen.current_color = COLOR_CYAN;
    print_to_screen("  starfield      ");
    screen.current_color = COLOR_WHITE;
    print_to_screen("- Starfield animation\n");
    screen.current_color = COLOR_CYAN;
    print_to_screen("  about          ");
    screen.current_color = COLOR_WHITE;
    print_to_screen("- About MicroComputer\n");
    
    screen.current_color = COLOR_CYAN;
    print_to_screen("  exit, quit     ");
    screen.current_color = COLOR_WHITE;
    print_to_screen("- Exit the system\n");
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
        if (tm_info) {
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M", tm_info);
        } else {
            strcpy(time_str, "Unknown");
        }
        
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
    
    if (tm_info) {
        strftime(buffer, sizeof(buffer), "%A, %B %d, %Y %H:%M:%S\n", tm_info);
        print_to_screen(buffer);
    } else {
        print_to_screen("Error: Could not get current time\n");
    }
}

void cmd_uptime(void) {
    time_t now = time(NULL);
    time_t uptime = now - boot_time;
    
    int hours = (int)(uptime / 3600);
    int minutes = (int)((uptime % 3600) / 60);
    int seconds = (int)(uptime % 60);
    
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
    print_to_screen("  Registers:\n");
    for (int i = 0; i < 8; i++) {
        snprintf(buffer, sizeof(buffer), "    R%d: 0x%04X (%u)\n", i, cpu.regs[i], cpu.regs[i]);
        print_to_screen(buffer);
    }
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
        strncpy(history.commands[history.count], cmd, INPUT_BUFFER_SIZE - 1);
        history.commands[history.count][INPUT_BUFFER_SIZE - 1] = '\0';
        history.count++;
    } else {
        for (int i = 0; i < MAX_HISTORY - 1; i++) {
            strcpy(history.commands[i], history.commands[i + 1]);
        }
        strncpy(history.commands[MAX_HISTORY - 1], cmd, INPUT_BUFFER_SIZE - 1);
        history.commands[MAX_HISTORY - 1][INPUT_BUFFER_SIZE - 1] = '\0';
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

void cmd_sysinfo(void) {
    char buffer[128];
    screen.current_color = COLOR_BRIGHT_CYAN;
    print_to_screen("\n=== MicroComputer System Information ===\n\n");
    
    screen.current_color = COLOR_YELLOW;
    print_to_screen("System: ");
    screen.current_color = COLOR_WHITE;
    print_to_screen("MicroOS v1.0\n");
    
    screen.current_color = COLOR_YELLOW;
    print_to_screen("CPU: ");
    screen.current_color = COLOR_WHITE;
    print_to_screen("Virtual 16-bit RISC\n");
    
    screen.current_color = COLOR_YELLOW;
    print_to_screen("RAM: ");
    screen.current_color = COLOR_WHITE;
    snprintf(buffer, sizeof(buffer), "%d KB\n", MEM_SIZE / 1024);
    print_to_screen(buffer);
    
    screen.current_color = COLOR_YELLOW;
    print_to_screen("Registers: ");
    screen.current_color = COLOR_WHITE;
    print_to_screen("8 x 16-bit\n");
    
    screen.current_color = COLOR_YELLOW;
    print_to_screen("Display: ");
    screen.current_color = COLOR_WHITE;
    snprintf(buffer, sizeof(buffer), "%dx%d text, %dx%d graphics\n", 
             SCREEN_WIDTH, SCREEN_HEIGHT, PIXEL_WIDTH, PIXEL_HEIGHT);
    print_to_screen(buffer);
    
    screen.current_color = COLOR_YELLOW;
    print_to_screen("Colors: ");
    screen.current_color = COLOR_WHITE;
    print_to_screen("16-color palette\n");
    
    screen.current_color = COLOR_YELLOW;
    print_to_screen("Files: ");
    screen.current_color = COLOR_WHITE;
    snprintf(buffer, sizeof(buffer), "%d loaded\n", fs.file_count);
    print_to_screen(buffer);
    
    screen.current_color = COLOR_YELLOW;
    print_to_screen("Uptime: ");
    screen.current_color = COLOR_WHITE;
    time_t now = time(NULL);
    time_t uptime = now - boot_time;
    int hours = (int)(uptime / 3600);
    int minutes = (int)((uptime % 3600) / 60);
    snprintf(buffer, sizeof(buffer), "%dh %dm\n", hours, minutes);
    print_to_screen(buffer);
    
    print_to_screen("\n");
}

void cmd_touch(const char *filename) {
    if (write_file(filename, (uint8_t*)"", 0) == 0) {
        screen.current_color = COLOR_BRIGHT_GREEN;
        print_to_screen("File created.\n");
        screen.current_color = COLOR_WHITE;
        scan_filesystem();
    } else {
        screen.current_color = COLOR_BRIGHT_RED;
        print_to_screen("Error: Could not create file\n");
        screen.current_color = COLOR_WHITE;
    }
}

void cmd_banner(const char *text) {
    if (!text || strlen(text) == 0) {
        print_to_screen("Usage: banner <text>\n");
        return;
    }
    
    print_to_screen("\n");
    screen.current_color = COLOR_BRIGHT_YELLOW;
    
    // Top border
    for (int i = 0; i < strlen(text) + 4; i++) print_to_screen("=");
    print_to_screen("\n");
    
    // Text
    screen.current_color = COLOR_BRIGHT_CYAN;
    print_to_screen("  ");
    print_to_screen(text);
    print_to_screen("  \n");
    
    // Bottom border
    screen.current_color = COLOR_BRIGHT_YELLOW;
    for (int i = 0; i < strlen(text) + 4; i++) print_to_screen("=");
    print_to_screen("\n\n");
    
    screen.current_color = COLOR_WHITE;
}

void cmd_color(const char *arg) {
    if (!arg) {
        print_to_screen("Current color codes:\n");
        for (int i = 0; i < 16; i++) {
            screen.current_color = i;
            char buf[32];
            snprintf(buf, sizeof(buf), "  %2d: Sample Text\n", i);
            print_to_screen(buf);
        }
        screen.current_color = COLOR_WHITE;
        return;
    }
    
    int color = atoi(arg);
    if (color >= 0 && color < 16) {
        screen.current_color = color;
        print_to_screen("Color changed.\n");
    } else {
        print_to_screen("Invalid color (0-15)\n");
    }
}

void cmd_matrix(void) {
    srand(time(NULL));
    screen.current_color = COLOR_BRIGHT_GREEN;
    
    for (int frame = 0; frame < 100; frame++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            if (rand() % 3 == 0) {
                int y = rand() % SCREEN_HEIGHT;
                screen.chars[y][x] = 33 + rand() % 94;
                screen.colors[y][x] = COLOR_BRIGHT_GREEN;
                screen.dirty = 1;
            }
        }
        sleep_ms(30);
    }
    
    screen.current_color = COLOR_WHITE;
}

void cmd_starfield(void) {
    clear_pixels();
    screen.pixel_mode = 1;
    
    typedef struct { int x, y, z; } Star;
    Star stars[50];
    
    srand(time(NULL));
    for (int i = 0; i < 50; i++) {
        stars[i].x = (rand() % 320) - 160;
        stars[i].y = (rand() % 200) - 100;
        stars[i].z = rand() % 100 + 1;
    }
    
    for (int frame = 0; frame < 200; frame++) {
        clear_pixels();
        
        for (int i = 0; i < 50; i++) {
            stars[i].z -= 2;
            if (stars[i].z <= 0) {
                stars[i].x = (rand() % 320) - 160;
                stars[i].y = (rand() % 200) - 100;
                stars[i].z = 100;
            }
            
            int sx = 160 + (stars[i].x * 100) / stars[i].z;
            int sy = 100 + (stars[i].y * 100) / stars[i].z;
            
            if (sx >= 0 && sx < PIXEL_WIDTH && sy >= 0 && sy < PIXEL_HEIGHT) {
                set_pixel(sx, sy, 1);
            }
        }
        
        screen.dirty = 1;
        sleep_ms(50);
    }
    
    clear_screen_display();
    screen.pixel_mode = 0;
}

void cmd_about(void) {
    clear_screen_display();
    screen.current_color = COLOR_BRIGHT_CYAN;
    print_to_screen("\n\n");
    print_to_screen("        ╔══════════════════════════════════════╗\n");
    print_to_screen("        ║                                      ║\n");
    print_to_screen("        ║     MicroComputer Emulator v1.0      ║\n");
    print_to_screen("        ║                                      ║\n");
    print_to_screen("        ╚══════════════════════════════════════╝\n\n");
    
    screen.current_color = COLOR_YELLOW;
    print_to_screen("  A fantasy computer for learning and creativity\n\n");
    
    screen.current_color = COLOR_WHITE;
    print_to_screen("  Features:\n");
    screen.current_color = COLOR_GREEN;
    print_to_screen("    • 64KB RAM with 8 registers\n");
    print_to_screen("    • 80x25 text mode with 16 colors\n");
    print_to_screen("    • 320x200 pixel graphics\n");
    print_to_screen("    • Sound synthesis\n");
    print_to_screen("    • Custom bytecode VM\n\n");
    
    screen.current_color = COLOR_CYAN;
    print_to_screen("  Inspired by PICO-8, TIC-80, and retro computers\n");
    print_to_screen("  Licensed under GPL-3.0\n\n");
    
    screen.current_color = COLOR_BRIGHT_WHITE;
    print_to_screen("  Type 'help' for available commands\n\n");
    
    screen.current_color = COLOR_WHITE;
}

void print_prompt(void) {
    screen.current_color = COLOR_BRIGHT_GREEN;
    print_to_screen("$ ");
    screen.current_color = COLOR_WHITE;
}

void shell_loop(void) {
    char cmd[256];
    
    show_boot_animation();
    clear_screen_display();
    
    screen.current_color = COLOR_BRIGHT_CYAN;
    print_to_screen("MicroOS v1.0\n");
    screen.current_color = COLOR_YELLOW;
    print_to_screen("Type 'help' for available commands.\n\n");
    screen.current_color = COLOR_WHITE;
    
    while (window_running) {
        print_prompt();
        
        char *line = read_line_from_screen();
        if (!window_running) break;
        
        strncpy(cmd, line, sizeof(cmd) - 1);
        cmd[sizeof(cmd) - 1] = '\0';
        
        int len = (int)strlen(cmd);
        while (len > 0 && isspace((unsigned char)cmd[len - 1])) {
            cmd[--len] = '\0';
        }
        
        if (len == 0) continue;
        
        add_to_history(cmd);
        
        char *token = strtok(cmd, " ");
        if (!token) continue;
        
        if (strcmp(token, "exit") == 0 || strcmp(token, "quit") == 0) {
            screen.current_color = COLOR_BRIGHT_YELLOW;
            print_to_screen("Goodbye!\n");
            screen.current_color = COLOR_WHITE;
            sleep_us(500000);
            break;
        } else if (strcmp(token, "help") == 0) {
            cmd_help();
        } else if (strcmp(token, "clear") == 0 || strcmp(token, "cls") == 0) {
            clear_screen_display();
        } else if (strcmp(token, "ls") == 0 || strcmp(token, "dir") == 0) {
            cmd_ls();
        } else if (strcmp(token, "sysinfo") == 0) {
            cmd_sysinfo();
        } else if (strcmp(token, "touch") == 0) {
            char *filename = strtok(NULL, " ");
            if (filename) {
                cmd_touch(filename);
            } else {
                print_to_screen("Usage: touch <filename>\n");
            }
        } else if (strcmp(token, "banner") == 0) {
            char *text = strtok(NULL, "");
            cmd_banner(text);
        } else if (strcmp(token, "color") == 0) {
            char *arg = strtok(NULL, " ");
            cmd_color(arg);
        } else if (strcmp(token, "matrix") == 0) {
            cmd_matrix();
        } else if (strcmp(token, "starfield") == 0) {
            cmd_starfield();
        } else if (strcmp(token, "about") == 0) {
            cmd_about();
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
                    show_loading_animation(filename);
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
            screen.current_color = COLOR_BRIGHT_RED;
            print_to_screen("Unknown command: ");
            print_to_screen(token);
            print_to_screen("\n");
            screen.current_color = COLOR_YELLOW;
            print_to_screen("Type 'help' for available commands.\n");
            screen.current_color = COLOR_WHITE;
        }
    }
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);
#endif
    
    srand(time(NULL));
    boot_time = time(NULL);
    
    pthread_mutex_init(&input_buf.mutex, NULL);
    input_buf.pos = 0;
    input_buf.ready = 0;
    input_buf.char_ready = 0;
    
    memset(&history, 0, sizeof(History));
    
    init_screen();
    init_filesystem();
    init_cpu();
    
    printf("MicroComputer Emulator v1.0\n");
    printf("===========================\n");
    printf("Filesystem: %s\n", fs.root_dir);
    printf("Loading files...\n");
    scan_filesystem();
    printf("Loaded %d files.\n\n", fs.file_count);
    printf("Starting display...\n");
    printf("All interaction in the display window!\n\n");
    
    pthread_t thread;
    pthread_create(&thread, NULL, window_thread, NULL);
    
    sleep_ms(1000);
    
    shell_loop();
    
    window_running = 0;
    pthread_join(thread, NULL);
    pthread_mutex_destroy(&input_buf.mutex);
    
#ifdef _WIN32
    WSACleanup();
#endif
    
    return 0;
}
