#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define RAM_SIZE 4096
#define STACK_SIZE 32
#define DISPLAY_WIDTH 64
#define DISPLAY_HEIGHT 32

struct Stack {
    uint16_t stack[STACK_SIZE];
    uint8_t top;
};

uint16_t pop(struct Stack *stack);
void push(struct Stack *stack, uint16_t value);
void stack_overflow(void);
void stack_underflow(void);

void write_program_to_memory(char *path, uint8_t *RAM);

int main(int argc, char *argv[]) {
    uint8_t RAM[RAM_SIZE] = {
            0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
            0x20, 0x60, 0x20, 0x20, 0x70, // 1
            0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
            0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
            0x90, 0x90, 0xF0, 0x10, 0x10, // 4
            0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
            0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
            0xF0, 0x10, 0x20, 0x40, 0x40, // 7
            0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
            0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
            0xF0, 0x90, 0xF0, 0x90, 0x90, // A
            0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
            0xF0, 0x80, 0x80, 0x80, 0xF0, // C
            0xE0, 0x90, 0x90, 0x90, 0xE0, // D
            0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
            0xF0, 0x80, 0xF0, 0x80, 0x80  // F, this is the font
    };
    uint8_t display_grid[DISPLAY_WIDTH*DISPLAY_HEIGHT];
    uint16_t I; // index register
    uint16_t PC = 0x200; // program counter
    uint8_t delay_timer, sound_timer = UINT8_MAX;
    int8_t V0, V1, V2, V3, V4, V5, V6, V7, V8, V9, VA, VB, VC, VD, VE, VF; // general purpose variable registers
    struct Stack stack = {
        .stack = {0},
        .top = 0,
    };

    write_program_to_memory(argv[0], RAM + 0x200);

    return 0;
}

void write_program_to_memory(char *path, uint8_t *RAM) {
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        printf("Can't open %s\n", path);
        exit(EXIT_FAILURE);
    }
    while (fread(RAM, sizeof(*RAM), 1, fp) == 1) {
        RAM++;
    }
    if (ferror(fp)) {
        printf("Error reading from file %s\n", path);
        fclose(fp);
        exit(EXIT_FAILURE);
    }
    fclose(fp);
}

void stack_overflow() {
    printf("Stack Overflow");
    exit(EXIT_FAILURE);
}

void stack_underflow() {
    printf("Stack Underflow");
    exit(EXIT_FAILURE);
}

uint16_t pop(struct Stack *stack) {
    if (stack->top == 0) {
        stack_underflow();
    }
    return stack->stack[--stack->top];
}

void push(struct Stack *stack, uint16_t value) {
    if (stack->top == STACK_SIZE) {
        stack_overflow();
    }
    stack->stack[stack->top++] = value;
}
