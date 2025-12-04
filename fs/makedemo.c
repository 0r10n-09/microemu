/*
 * Demo Binary Generator for MicroComputer
 * Creates a demo.bin that showcases the emulator
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
    while (*str) {
        emit_byte(p, *str++);
    }
    emit_byte(p, 0); // null terminator
}

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

// Demo effects
void banner_effect(Program *p) {
    clear_screen(p);
    
    const char *lines[] = {
        "================================================================================",
        "                                                                                ",
        "          M I C R O C O M P U T E R   E M U L A T O R   D E M O                ",
        "                                                                                ",
        "                           Stress Test v1.0                                    ",
        "                                                                                ",
        "================================================================================",
        NULL
    };
    
    for (int i = 0; lines[i] != NULL; i++) {
        print_str(p, lines[i]);
        print_str(p, "\n");
        sleep_ms(p, 100);
    }
    
    sleep_ms(p, 1000);
}

void text_effect(Program *p) {
    clear_screen(p);
    print_str(p, "TEXT OUTPUT TEST\n");
    print_str(p, "================\n\n");
    sleep_ms(p, 500);
    
    print_str(p, "Testing rapid character output...\n");
    sleep_ms(p, 300);
    
    const char *alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    for (int i = 0; i < strlen(alphabet); i++) {
        print_char(p, alphabet[i]);
        sleep_ms(p, 30);
    }
    print_str(p, "\n\n");
    sleep_ms(p, 500);
    
    print_str(p, "Testing numbers: ");
    for (int i = 0; i < 10; i++) {
        print_char(p, '0' + i);
        print_char(p, ' ');
        sleep_ms(p, 50);
    }
    print_str(p, "\n\n");
    sleep_ms(p, 1000);
}

void ascii_art(Program *p) {
    clear_screen(p);
    print_str(p, "ASCII ART TEST\n");
    print_str(p, "==============\n\n");
    sleep_ms(p, 500);
    
    const char *art[] = {
        "       .--.",
        "      /    \\",
        "     ## a  a",
        "     (   '._)",
        "      |'-- |",
        "    _.\\_-'/_._",
        "   / ` '*' ` \\",
        "  /  .'^'^'.  \\",
        " |  | o  o |  |",
        " |   \\_____/   |",
        "  \\  '-----'  /",
        "   '._______.'",
        "",
        "  Computer Cat",
        NULL
    };
    
    for (int i = 0; art[i] != NULL; i++) {
        print_str(p, art[i]);
        print_str(p, "\n");
        sleep_ms(p, 80);
    }
    
    sleep_ms(p, 2000);
}

void fill_screen_test(Program *p) {
    clear_screen(p);
    print_str(p, "SCREEN FILL TEST\n");
    print_str(p, "================\n\n");
    sleep_ms(p, 500);
    
    print_str(p, "Filling screen with characters...\n\n");
    sleep_ms(p, 500);
    
    // Fill multiple lines
    const char *patterns[] = {
        "########################################################################",
        "........................................................................",
        "========================================================================",
        "------------------------------------------------------------------------",
        "************************************************************************",
        "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"
    };
    
    for (int i = 0; i < 15; i++) {
        print_str(p, patterns[i % 6]);
        print_str(p, "\n");
        sleep_ms(p, 40);
    }
    
    sleep_ms(p, 1500);
}

void scroll_test(Program *p) {
    clear_screen(p);
    print_str(p, "SCROLL TEST\n");
    print_str(p, "===========\n\n");
    print_str(p, "Generating lines to force scrolling...\n\n");
    sleep_ms(p, 1000);
    
    char line[80];
    for (int i = 1; i <= 50; i++) {
        snprintf(line, sizeof(line), "Line %02d - Testing screen scrolling functionality...\n", i);
        for (int j = 0; line[j]; j++) {
            print_char(p, line[j]);
        }
        sleep_ms(p, 50);
    }
    
    sleep_ms(p, 1000);
}

void rapid_clear_test(Program *p) {
    clear_screen(p);
    print_str(p, "RAPID CLEAR TEST\n");
    print_str(p, "================\n\n");
    print_str(p, "Clearing screen rapidly...\n");
    sleep_ms(p, 1000);
    
    for (int i = 0; i < 10; i++) {
        clear_screen(p);
        char msg[40];
        snprintf(msg, sizeof(msg), "Clear #%d\n", i + 1);
        for (int j = 0; msg[j]; j++) {
            print_char(p, msg[j]);
        }
        sleep_ms(p, 100);
    }
    
    sleep_ms(p, 500);
}

void bouncing_text(Program *p) {
    clear_screen(p);
    print_str(p, "BOUNCING TEXT SIMULATION\n");
    print_str(p, "========================\n\n");
    sleep_ms(p, 1000);
    
    const char *ball = "[@]";
    
    for (int frame = 0; frame < 30; frame++) {
        clear_screen(p);
        
        // Create bouncing effect with spaces
        int position = (frame % 20);
        if (frame / 20 % 2 == 1) {
            position = 19 - position;
        }
        
        print_str(p, "Bouncing Ball Demo:\n\n");
        
        for (int i = 0; i < position; i++) {
            print_char(p, ' ');
        }
        print_str(p, ball);
        print_str(p, "\n");
        
        sleep_ms(p, 50);
    }
    
    sleep_ms(p, 1000);
}

void memory_stress(Program *p) {
    clear_screen(p);
    print_str(p, "MEMORY STRESS TEST\n");
    print_str(p, "==================\n\n");
    sleep_ms(p, 500);
    
    print_str(p, "Generating large amounts of text output...\n\n");
    sleep_ms(p, 500);
    
    const char *lorem = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. ";
    
    for (int i = 0; i < 30; i++) {
        char prefix[40];
        snprintf(prefix, sizeof(prefix), "[%02d] ", i + 1);
        for (int j = 0; prefix[j]; j++) {
            print_char(p, prefix[j]);
        }
        print_str(p, lorem);
        print_str(p, "\n");
        sleep_ms(p, 30);
    }
    
    sleep_ms(p, 2000);
}

void pattern_test(Program *p) {
    clear_screen(p);
    print_str(p, "PATTERN GENERATION TEST\n");
    print_str(p, "=======================\n\n");
    sleep_ms(p, 500);
    
    // Diagonal pattern
    for (int i = 0; i < 15; i++) {
        for (int j = 0; j < i; j++) {
            print_char(p, ' ');
        }
        print_str(p, "*\n");
        sleep_ms(p, 50);
    }
    
    sleep_ms(p, 500);
    
    // Pyramid
    clear_screen(p);
    print_str(p, "Pyramid Pattern:\n\n");
    sleep_ms(p, 500);
    
    for (int i = 1; i <= 10; i++) {
        for (int j = 0; j < 10 - i; j++) {
            print_char(p, ' ');
        }
        for (int j = 0; j < 2 * i - 1; j++) {
            print_char(p, '#');
        }
        print_str(p, "\n");
        sleep_ms(p, 80);
    }
    
    sleep_ms(p, 2000);
}

void finale(Program *p) {
    clear_screen(p);
    
    const char *msg[] = {
        "",
        "",
        "                         DEMO COMPLETE!",
        "",
        "              MicroComputer Emulator Stress Test",
        "",
        "                    All systems functional",
        "",
        "",
        "                   Press CTRL+C to exit demo",
        "",
        "",
        NULL
    };
    
    for (int i = 0; msg[i] != NULL; i++) {
        print_str(p, msg[i]);
        print_str(p, "\n");
        sleep_ms(p, 100);
    }
    
    sleep_ms(p, 3000);
}

int main(void) {
    Program prog;
    init_program(&prog);
    
    printf("Generating demo.bin...\n");
    
    // Build the demo
    banner_effect(&prog);
    text_effect(&prog);
    ascii_art(&prog);
    fill_screen_test(&prog);
    scroll_test(&prog);
    rapid_clear_test(&prog);
    bouncing_text(&prog);
    memory_stress(&prog);
    pattern_test(&prog);
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
    
    free(prog.data);
    return 0;
}
