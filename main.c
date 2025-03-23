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

void jump(uint16_t *PC, uint16_t location);
void set_v(uint8_t *V, uint8_t value);
void set_i(uint16_t *I, uint16_t value);
void add_v(uint8_t *V, uint8_t value);
void write_program_to_memory(char *path, uint8_t *RAM);
void decode_execute(uint16_t opcode, uint16_t *PC, uint16_t *I, uint8_t *V, uint8_t *RAM, uint8_t *display_grid);
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
    uint16_t I; // index register
    uint16_t PC = PROGRAM_START_POSITION; // program counter
    uint16_t opcode;
    uint8_t delay_timer, sound_timer = UINT8_MAX;
    uint8_t V[16] = {0};
    struct Stack stack = { .stack = {0}, .top = 0 };
    uint8_t display_grid[DISPLAY_WIDTH][DISPLAY_HEIGHT];
    SDL_Window *window;
    SDL_Event event;
    bool close = false;

    write_program_to_memory(argv[1], RAM + PROGRAM_START_POSITION);

    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_DEBUG);

    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        SDL_Log("SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }
    window = SDL_CreateWindow("CHIP_8",
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              DISPLAY_WIDTH * BLOCK_SIZE, DISPLAY_HEIGHT * BLOCK_SIZE, 0);

    while (!close) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) close = true;
        }
        opcode = fetch(&PC, RAM);
        decode_execute(opcode, &PC, &I, V, RAM, (uint8_t *) display_grid);
    }
    SDL_Quit();
    return 0;
}

void jump(uint16_t *PC, uint16_t location) {
    *PC = location;
}

void set_v(uint8_t *V, uint8_t value) {
    *V = value;
}

void set_i(uint16_t *I, uint16_t value) {
    *I = value;
}

void add_v(uint8_t *V, uint8_t value) {
    *V += value;
}

void decode_execute(uint16_t opcode, uint16_t *PC, uint16_t *I, uint8_t *V, uint8_t *RAM, uint8_t *display_grid) {
    uint16_t first_nibble = opcode & 0xF000;
    uint16_t second_nibble = opcode & 0x0F00;
    uint8_t third_nibble = opcode & 0x00F0;
    uint8_t fourth_nibble = opcode & 0x000F;
    switch (first_nibble) {
        case 0x0000: break;
        case 0x1000: jump(PC, opcode & 0x0FFF); break;
        case 0x2000: break;
        case 0x3000: break;
        case 0x4000: break;
        case 0x5000: break;
        case 0x6000: set_v(V + (second_nibble >> 8), third_nibble + fourth_nibble); break;
        case 0x7000: add_v(V + (second_nibble >> 8), third_nibble + fourth_nibble); break;
        case 0x8000: break;
        case 0x9000: break;
        case 0xA000: set_i(I, opcode & 0x0FFF); break;
        case 0xB000: break;
        case 0xC000: break;
        case 0xD000: break;
        case 0xE000: break;
        case 0xF000: break;
    }
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
