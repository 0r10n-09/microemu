/*
 * Enhanced Demo Binary Generator for MicroComputer
 * Creates a demo.bin that showcases ALL emulator features
 * Compile: gcc -o makedemo makedemo.c
 * Run: ./makedemo
 * Output: demo.bin (place in fs/ folder)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

// Opcodes
#define OP_HALT         0x00
#define OP_PRINT_CHAR   0x01
#define OP_PRINT_STR    0x02
#define OP_CLEAR_SCREEN 0x04
#define OP_SLEEP_MS     0x20
#define OP_BEEP         0x21
#define OP_SET_PIXEL    0x30
#define OP_CLEAR_PIXELS 0x31
#define OP_LOAD_REG     0x40
#define OP_STORE_REG    0x41
#define OP_ADD          0x50
#define OP_SUB          0x51
#define OP_MUL          0x52
#define OP_DIV          0x53
#define OP_AND          0x54
#define OP_OR           0x55
#define OP_XOR          0x56
#define OP_NOT          0x57
#define OP_CMP          0x58
#define OP_JMP          0x60
#define OP_JZ           0x61
#define OP_JNZ          0x62
#define OP_JG           0x63
#define OP_JL           0x64
#define OP_READ_CHAR    0x70
#define OP_LOAD_MEM     0x80
#define OP_STORE_MEM    0x81

typedef struct {
    uint8_t *data;
    size_t size;
    size_t capacity;
} Program;

void init_program(Program *p) {
    p->capacity = 8192;
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
    while (*str) {
        emit_byte(p, *str++);
    }
    emit_byte(p, 0);
}

// Basic functions
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

void beep(Program *p, uint16_t freq, uint16_t duration) {
    emit_byte(p, OP_BEEP);
    emit_word(p, freq);
    emit_word(p, duration);
}

void set_pixel(Program *p, uint16_t x, uint16_t y, uint8_t value) {
    emit_byte(p, OP_SET_PIXEL);
    emit_word(p, x);
    emit_word(p, y);
    emit_byte(p, value);
}

void clear_pixels(Program *p) {
    emit_byte(p, OP_CLEAR_PIXELS);
}

void load_reg(Program *p, uint8_t reg, uint16_t value) {
    emit_byte(p, OP_LOAD_REG);
    emit_byte(p, reg);
    emit_word(p, value);
}

void add_regs(Program *p, uint8_t dst, uint8_t src1, uint8_t src2) {
    emit_byte(p, OP_ADD);
    emit_byte(p, dst);
    emit_byte(p, src1);
    emit_byte(p, src2);
}

void sub_regs(Program *p, uint8_t dst, uint8_t src1, uint8_t src2) {
    emit_byte(p, OP_SUB);
    emit_byte(p, dst);
    emit_byte(p, src1);
    emit_byte(p, src2);
}

void mul_regs(Program *p, uint8_t dst, uint8_t src1, uint8_t src2) {
    emit_byte(p, OP_MUL);
    emit_byte(p, dst);
    emit_byte(p, src1);
    emit_byte(p, src2);
}

void cmp_regs(Program *p, uint8_t src1, uint8_t src2) {
    emit_byte(p, OP_CMP);
    emit_byte(p, src1);
    emit_byte(p, src2);
}

void jmp(Program *p, uint16_t addr) {
    emit_byte(p, OP_JMP);
    emit_word(p, addr);
}

void jnz(Program *p, uint16_t addr) {
    emit_byte(p, OP_JNZ);
    emit_word(p, addr);
}

void halt(Program *p) {
    emit_byte(p, OP_HALT);
}

size_t get_current_addr(Program *p) {
    return p->size;
}

// Demo sections
void banner_effect(Program *p) {
    clear_screen(p);
    
    print_str(p, "================================================================================\n");
    print_str(p, "                                                                                \n");
    print_str(p, "      M I C R O C O M P U T E R   E M U L A T O R   D E M O   v2.0            \n");
    print_str(p, "                                                                                \n");
    print_str(p, "           Featuring: Graphics | Sound | Registers | Arithmetic                \n");
    print_str(p, "                                                                                \n");
    print_str(p, "================================================================================\n");
    sleep_ms(p, 2000);
}

void sound_test(Program *p) {
    clear_screen(p);
    print_str(p, "SOUND TEST\n");
    print_str(p, "==========\n\n");
    print_str(p, "Playing musical scale...\n\n");
    sleep_ms(p, 500);
    
    // C major scale
    uint16_t notes[] = {262, 294, 330, 349, 392, 440, 494, 523};
    const char *names[] = {"C", "D", "E", "F", "G", "A", "B", "C"};
    
    for (int i = 0; i < 8; i++) {
        print_str(p, "Note: ");
        print_str(p, names[i]);
        print_str(p, "\n");
        beep(p, notes[i], 300);
        sleep_ms(p, 100);
    }
    
    sleep_ms(p, 500);
    print_str(p, "\nPlaying melody...\n");
    sleep_ms(p, 500);
    
    // Simple melody
    uint16_t melody[] = {262, 262, 392, 392, 440, 440, 392, 349, 349, 330, 330, 294, 294, 262};
    uint16_t durations[] = {200, 200, 200, 200, 200, 200, 400, 200, 200, 200, 200, 200, 200, 400};
    
    for (int i = 0; i < 14; i++) {
        beep(p, melody[i], durations[i]);
        sleep_ms(p, 50);
    }
    
    sleep_ms(p, 1000);
}

void register_test(Program *p) {
    clear_screen(p);
    print_str(p, "REGISTER & ARITHMETIC TEST\n");
    print_str(p, "==========================\n\n");
    sleep_ms(p, 500);
    
    print_str(p, "Loading values into registers...\n");
    load_reg(p, 0, 10);   // R0 = 10
    load_reg(p, 1, 5);    // R1 = 5
    sleep_ms(p, 500);
    
    print_str(p, "R0 = 10, R1 = 5\n\n");
    sleep_ms(p, 500);
    
    print_str(p, "R2 = R0 + R1 (Addition)\n");
    add_regs(p, 2, 0, 1);
    sleep_ms(p, 500);
    
    print_str(p, "R3 = R0 - R1 (Subtraction)\n");
    sub_regs(p, 3, 0, 1);
    sleep_ms(p, 500);
    
    print_str(p, "R4 = R0 * R1 (Multiplication)\n");
    mul_regs(p, 4, 0, 1);
    sleep_ms(p, 500);
    
    print_str(p, "\nCheck 'meminfo' command to see register values!\n");
    sleep_ms(p, 2000);
}

void loop_test(Program *p) {
    clear_screen(p);
    print_str(p, "LOOP & JUMP TEST\n");
    print_str(p, "================\n\n");
    print_str(p, "Counting down from 10 using jumps...\n\n");
    sleep_ms(p, 1000);
    
    // Initialize counter: R0 = 10
    load_reg(p, 0, 10);
    load_reg(p, 1, 1);  // R1 = 1 (decrement value)
    load_reg(p, 2, 0);  // R2 = 0 (compare target)
    
    size_t loop_start = get_current_addr(p);
    
    // Print current value (simple representation)
    print_str(p, "*");
    sleep_ms(p, 200);
    beep(p, 440, 50);
    
    // Decrement: R0 = R0 - R1
    sub_regs(p, 0, 0, 1);
    
    // Compare R0 with 0
    cmp_regs(p, 0, 2);
    
    // Jump back if not zero
    jnz(p, (uint16_t)loop_start);
    
    print_str(p, "\n\nLoop complete!\n");
    sleep_ms(p, 1500);
}

void graphics_test(Program *p) {
    clear_screen(p);
    print_str(p, "GRAPHICS MODE TEST\n");
    print_str(p, "==================\n\n");
    print_str(p, "Switching to pixel graphics...\n");
    sleep_ms(p, 1500);
    
    clear_pixels(p);
    
    // Draw a border
    for (int x = 0; x < 320; x += 10) {
        set_pixel(p, x, 0, 1);
        set_pixel(p, x, 199, 1);
    }
    for (int y = 0; y < 200; y += 10) {
        set_pixel(p, 0, y, 1);
        set_pixel(p, 319, y, 1);
    }
    sleep_ms(p, 1000);
    
    // Draw some diagonal lines
    for (int i = 0; i < 100; i += 2) {
        set_pixel(p, i, i, 1);
        set_pixel(p, 319 - i, i, 1);
    }
    sleep_ms(p, 1000);
    
    // Draw a circle (simple approximation)
    int cx = 160, cy = 100, r = 40;
    for (int angle = 0; angle < 360; angle += 5) {
        double rad = angle * 3.14159 / 180.0;
        int x = cx + (int)(r * cos(rad));
        int y = cy + (int)(r * sin(rad));
        set_pixel(p, x, y, 1);
    }
    sleep_ms(p, 2000);
    
    // Clear and draw expanding circles
    clear_pixels(p);
    for (int radius = 10; radius <= 80; radius += 10) {
        for (int angle = 0; angle < 360; angle += 3) {
            double rad = angle * 3.14159 / 180.0;
            int x = cx + (int)(radius * cos(rad));
            int y = cy + (int)(radius * sin(rad));
            set_pixel(p, x, y, 1);
        }
        beep(p, 200 + radius * 10, 50);
        sleep_ms(p, 200);
    }
    
    sleep_ms(p, 2000);
    
    // Return to text mode
    clear_screen(p);
}

void text_effects(Program *p) {
    clear_screen(p);
    print_str(p, "TEXT EFFECTS\n");
    print_str(p, "============\n\n");
    sleep_ms(p, 500);
    
    const char *msg = "The quick brown fox jumps over the lazy dog!";
    print_str(p, "Typewriter effect:\n");
    for (int i = 0; msg[i]; i++) {
        print_char(p, msg[i]);
        beep(p, 800, 20);
        sleep_ms(p, 50);
    }
    print_str(p, "\n\n");
    sleep_ms(p, 1000);
    
    print_str(p, "Animated ASCII:\n\n");
    const char *frames[] = {
        "  O  \n /|\\ \n / \\ \n",
        " \\O/ \n  |  \n / \\ \n",
        "  O  \n /|\\ \n / \\ \n",
        " /O\\ \n  |  \n / \\ \n"
    };
    
    for (int loop = 0; loop < 8; loop++) {
        for (int i = 0; i < 4; i++) {
            // Move cursor up 3 lines (simulated by clearing)
            if (loop > 0 || i > 0) {
                for (int j = 0; j < 3; j++) {
                    print_str(p, "\r                    \r");
                }
            }
            print_str(p, frames[i]);
            sleep_ms(p, 150);
        }
    }
    
    sleep_ms(p, 1000);
}

void pattern_showcase(Program *p) {
    clear_screen(p);
    print_str(p, "PATTERN GENERATION\n");
    print_str(p, "==================\n\n");
    sleep_ms(p, 500);
    
    // Diamond pattern
    print_str(p, "Diamond:\n\n");
    int size = 5;
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size - i - 1; j++) print_char(p, ' ');
        for (int j = 0; j < 2 * i + 1; j++) print_char(p, '*');
        print_str(p, "\n");
        sleep_ms(p, 100);
    }
    for (int i = size - 2; i >= 0; i--) {
        for (int j = 0; j < size - i - 1; j++) print_char(p, ' ');
        for (int j = 0; j < 2 * i + 1; j++) print_char(p, '*');
        print_str(p, "\n");
        sleep_ms(p, 100);
    }
    
    sleep_ms(p, 1500);
    
    clear_screen(p);
    print_str(p, "Spiral Pattern:\n\n");
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < i * 2; j++) print_char(p, ' ');
        print_str(p, "@\n");
        sleep_ms(p, 80);
    }
    for (int i = 10; i >= 0; i--) {
        for (int j = 0; j < i * 2; j++) print_char(p, ' ');
        print_str(p, "@\n");
        sleep_ms(p, 80);
    }
    
    sleep_ms(p, 1500);
}

void scroll_test(Program *p) {
    clear_screen(p);
    print_str(p, "SCROLL TEST\n");
    print_str(p, "===========\n\n");
    print_str(p, "Generating many lines to test scrolling...\n\n");
    sleep_ms(p, 1000);
    
    for (int i = 1; i <= 40; i++) {
        print_str(p, "Line ");
        // Simple number printing
        if (i >= 10) print_char(p, '0' + i / 10);
        print_char(p, '0' + i % 10);
        print_str(p, " - Scrolling test in progress...\n");
        sleep_ms(p, 50);
    }
    
    sleep_ms(p, 1000);
}

void finale(Program *p) {
    clear_screen(p);
    
    print_str(p, "\n\n\n");
    print_str(p, "                         *** DEMO COMPLETE! ***\n");
    print_str(p, "\n");
    print_str(p, "              MicroComputer Emulator Feature Showcase\n");
    print_str(p, "\n");
    print_str(p, "  Features Demonstrated:\n");
    print_str(p, "  [X] Text output and animation\n");
    print_str(p, "  [X] Sound generation and music\n");
    print_str(p, "  [X] Register operations\n");
    print_str(p, "  [X] Arithmetic (add, sub, mul)\n");
    print_str(p, "  [X] Loops and jumps\n");
    print_str(p, "  [X] Pixel graphics mode\n");
    print_str(p, "  [X] Screen scrolling\n");
    print_str(p, "\n");
    print_str(p, "                    All systems operational!\n");
    print_str(p, "\n\n");
    
    // Victory fanfare
    uint16_t fanfare[] = {523, 587, 659, 784};
    for (int i = 0; i < 4; i++) {
        beep(p, fanfare[i], 200);
        sleep_ms(p, 50);
    }
    beep(p, 1047, 600);
    
    sleep_ms(p, 2000);
}

int main(void) {
    Program prog;
    init_program(&prog);
    
    printf("Generating enhanced demo.bin...\n");
    
    // Build the comprehensive demo
    banner_effect(&prog);
    sound_test(&prog);
    register_test(&prog);
    loop_test(&prog);
    graphics_test(&prog);
    text_effects(&prog);
    pattern_showcase(&prog);
    scroll_test(&prog);
    finale(&prog);
    
    // End program
    halt(&prog);
    
    // Write to file
    FILE *f = fopen("demo.bin", "wb");
    if (!f) {
        fprintf(stderr, "Error: Could not create demo.bin\n");
        return 1;
    }
    
    fwrite(prog.data, 1, prog.size, f);
    fclose(f);
    
    printf("Created demo.bin (%zu bytes)\n", prog.size);
    printf("Copy this file to the fs/ directory and run with: run demo.bin\n");
    printf("\nNew features in this demo:\n");
    printf("  - Sound effects and music\n");
    printf("  - Register arithmetic\n");
    printf("  - Loops using jumps\n");
    printf("  - Pixel graphics mode\n");
    printf("  - Animated patterns\n");
    
    free(prog.data);
    return 0;
}
