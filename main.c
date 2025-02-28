#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define RAM_SIZE 4096
#define STACK_SIZE 32

uint16_t pop(const uint16_t *stack, uint16_t *stack_pointer);
void push(const uint16_t *stack, uint16_t *stack_pointer, uint16_t value);
void stack_overflow(void);

int main(void) {
    uint8_t RAM[RAM_SIZE] = { //stores a byte for each pixel
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
    uint8_t display_grid[64*32];
    uint16_t I; // index register
    uint16_t PC; // program counter
    uint8_t delay_timer, sound_timer = UINT8_MAX;
    int8_t V0, V1, V2, V3, V4, V5, V6, V7, V8, V9, VA, VB, VC, VD, VE, VF; // general purpose variable registers
    uint16_t stack[STACK_SIZE], *stack_pointer;
    stack_pointer = stack;

    return 0;
}

void stack_overflow() {
    printf("Stack Overflow");
    exit(EXIT_FAILURE);
}

uint16_t pop(const uint16_t *stack, uint16_t *stack_pointer) {
    if (stack_pointer <= stack) {
        stack_overflow();
    }
    return *(--stack_pointer);
}

void push(const uint16_t *stack, uint16_t *stack_pointer, uint16_t value) {
    if (stack_pointer > stack + STACK_SIZE) {
        stack_overflow();
    }
    *(stack_pointer++) = value;
}
