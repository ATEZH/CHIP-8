#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <SDL.h>

#define RAM_SIZE 4096
#define STACK_SIZE 32
#define DISPLAY_WIDTH 64
#define DISPLAY_HEIGHT 32
#define BLOCK_SIZE 20
#define PROGRAM_START_POSITION 0x200

struct Stack {
    uint16_t stack[STACK_SIZE];
    uint8_t top;
};

uint16_t pop(struct Stack *stack);
void push(struct Stack *stack, uint16_t value);
void stack_overflow(void);
void stack_underflow(void);

void write_program_to_memory(char *path, uint8_t *RAM);
uint16_t fetch(uint16_t *PC, uint8_t *RAM);

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
    uint16_t PC = PROGRAM_START_POSITION; // program counter
    uint8_t delay_timer, sound_timer = UINT8_MAX;
    int8_t V0, V1, V2, V3, V4, V5, V6, V7, V8, V9, VA, VB, VC, VD, VE, VF; // general purpose variable registers
    struct Stack stack = {
        .stack = {0},
        .top = 0,
    };
    bool close = false;

    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        printf("SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window* win = SDL_CreateWindow("CHIP_8",
                                       SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOWPOS_CENTERED,
                                       DISPLAY_WIDTH*BLOCK_SIZE, DISPLAY_HEIGHT*BLOCK_SIZE, 0);
    while (!close) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) close = true;
        }
    }
    SDL_Quit();
    return 0;
}

uint16_t fetch(uint16_t *PC, uint8_t *RAM) {
    uint16_t instruction = ((uint16_t)(*(RAM + *PC)) << 8) + *(RAM + *PC + 1);
    *PC += 2;
    return instruction;
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
