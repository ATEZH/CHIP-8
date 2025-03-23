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
void skip_vx_e_nn(uint16_t *PC, uint8_t *V, uint8_t value);
void skip_vx_not_e_nn(uint16_t *PC, uint8_t *V, uint8_t value);
void skip_vx_e_vy(uint16_t *PC, uint8_t *VX, uint8_t *VY);
void skip_vx_not_e_vy(uint16_t *PC, uint8_t *VX, uint8_t *VY);
void load_vy_to_vx(uint8_t *VX, uint8_t *VY);
void or_vx_vy(uint8_t *VX, uint8_t *VY);
void and_vx_vy(uint8_t *VX, uint8_t *VY);
void xor_vx_vy(uint8_t *VX, uint8_t *VY);
void add_vx_vy(uint8_t *VX, uint8_t *VY);
void subtract_vx_vy(uint8_t *VX, uint8_t *VY, uint8_t *VF);
void shiftr_vx_vy(uint8_t *VX, uint8_t *VY, uint8_t *VF);
void subtract_vy_vx(uint8_t *VX, uint8_t *VY, uint8_t *VF);
void shiftl_vx_vy(uint8_t *VX, uint8_t *VY, uint8_t *VF);
void call_subroutine(struct Stack *stack, uint16_t *PC, uint16_t location);
void return_from_subroutine(struct Stack *stack, uint16_t *PC);
void clear_screen(uint8_t *display);
void draw(uint8_t *display_grid, uint8_t *RAM, uint16_t *I, uint8_t *V, uint8_t VX, uint8_t VY, uint8_t N);
void render(SDL_Renderer *renderer, uint8_t *display_grid);

void write_program_to_memory(char *path, uint8_t *RAM);
void decode_execute(uint16_t opcode, uint16_t *PC, uint16_t *I, uint8_t *V, struct Stack *stack, uint8_t *RAM, uint8_t *display_grid, SDL_Renderer *renderer);
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
    SDL_Renderer *renderer;
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
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    while (!close) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) close = true;
        }
        opcode = fetch(&PC, RAM);
        decode_execute(opcode, &PC, &I, V, &stack, RAM, (uint8_t *) display_grid, renderer);
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

void skip_vx_e_nn(uint16_t *PC, uint8_t *V, uint8_t value) {
    if (*V == value) *PC += 2;
}

void skip_vx_not_e_nn(uint16_t *PC, uint8_t *V, uint8_t value) {
    if (*V != value) *PC += 2;
}

void skip_vx_e_vy(uint16_t *PC, uint8_t *VX, uint8_t *VY) {
    if (*VX == *VY) *PC += 2;
}

void skip_vx_not_e_vy(uint16_t *PC, uint8_t *VX, uint8_t *VY) {
    if (*VX != *VY) *PC += 2;
}

void load_vy_to_vx(uint8_t *VX, uint8_t *VY) {
    *VX = *VY;
}

void or_vx_vy(uint8_t *VX, uint8_t *VY) {
    *VX |= *VY;
}

void and_vx_vy(uint8_t *VX, uint8_t *VY) {
    *VX &= *VY;
}

void xor_vx_vy(uint8_t *VX, uint8_t *VY) {
    *VX ^= *VY;
}

void add_vx_vy(uint8_t *VX, uint8_t *VY) {
    *VX += *VY;
}

void subtract_vx_vy(uint8_t *VX, uint8_t *VY, uint8_t *VF) {
    *VF = *VX > *VY;
    *VX = *VX - *VY;
}

void shiftr_vx_vy(uint8_t *VX, uint8_t *VY, uint8_t *VF) {
    *VF = ((*VX & 0x01) == 0x01);
    *VX >>= 1;
}

void subtract_vy_vx(uint8_t *VX, uint8_t *VY, uint8_t *VF) {
    *VF = *VY > *VX;
    *VX = *VY - *VX;
}

void shiftl_vx_vy(uint8_t *VX, uint8_t *VY, uint8_t *VF) {
    *VF = ((*VX & 0x80) == 0x80);
    *VX <<= 1;
}

void call_subroutine(struct Stack *stack, uint16_t *PC, uint16_t location) {
    push(stack, *PC);
    *PC = location;
}

void return_from_subroutine(struct Stack *stack, uint16_t *PC) {
    *PC = pop(stack);
}

void clear_screen(uint8_t *display) {
    uint8_t *i = display;
    while (i < display + DISPLAY_WIDTH*DISPLAY_HEIGHT) *(i++) = 0;
}

void draw(uint8_t *display_grid, uint8_t *RAM, uint16_t *I, uint8_t *V, uint8_t VX, uint8_t VY, uint8_t N) {
    int8_t shift;
    uint8_t row, current_byte;
    uint16_t pixel_position;
    row = 0;
    VX = *(V + VX) % DISPLAY_WIDTH;
    VY = *(V + VY) % DISPLAY_HEIGHT;
    pixel_position = VX + DISPLAY_WIDTH * VY;
    while (row < N && pixel_position < DISPLAY_WIDTH * DISPLAY_HEIGHT) {
        shift = 7;
        current_byte = *(RAM + *I + row);
        while (shift >= 0 && pixel_position < DISPLAY_WIDTH * (VY + row + 1)) {
            if (*(display_grid + pixel_position) && (current_byte >> shift)) *(V + 0xF) = 1;
            *(display_grid + pixel_position) = *(display_grid + pixel_position) ^ ((current_byte >> shift) & 1);
            pixel_position++;
            shift--;
        }
        pixel_position += DISPLAY_WIDTH - 7 + shift;
        row++;
    }
}

void render(SDL_Renderer *render, uint8_t *display_grid) {
    uint16_t row, col;
    col = row = 0;
    SDL_Rect rect;
    while (display_grid + row * DISPLAY_WIDTH + col < display_grid + DISPLAY_WIDTH * DISPLAY_HEIGHT) {
        if (*(display_grid + row * DISPLAY_WIDTH + col)) {
            rect.h = BLOCK_SIZE; rect.w = BLOCK_SIZE; rect.x = col * BLOCK_SIZE; rect.y = row * BLOCK_SIZE;
            SDL_RenderFillRect(render, &rect);
        }
        if (++col == DISPLAY_WIDTH) {col = 0; row++;}
    }
    SDL_RenderPresent(render);
}

void decode_execute(uint16_t opcode,
                    uint16_t *PC,
                    uint16_t *I,
                    uint8_t *V,
                    struct Stack *stack,
                    uint8_t *RAM,
                    uint8_t *display_grid,
                    SDL_Renderer *renderer) {
    uint16_t first_nibble = opcode & 0xF000;
    uint16_t second_nibble = opcode & 0x0F00;
    uint8_t third_nibble = opcode & 0x00F0;
    uint8_t fourth_nibble = opcode & 0x000F;
    switch (first_nibble) {
        case 0x0000:
            switch (fourth_nibble) {
                case 0x0:
                    clear_screen(display_grid);
                    render(renderer, display_grid);
                    break;
                case 0xE: return_from_subroutine(stack, PC); break;
                default: break;
            } break;
        case 0x1000: jump(PC, opcode & 0x0FFF); break;
        case 0x2000: call_subroutine(stack, PC, opcode & 0x0FFF); break;
        case 0x3000: skip_vx_e_nn(PC, V + (second_nibble >> 8), opcode & 0x00FF); break;
        case 0x4000: skip_vx_not_e_nn(PC, V + (second_nibble >> 8), opcode & 0x00FF); break;
        case 0x5000: skip_vx_e_vy(PC, V + (second_nibble >> 8), V + (third_nibble >> 4)); break;
        case 0x6000: set_v(V + (second_nibble >> 8), opcode & 0x00FF); break;
        case 0x7000: add_v(V + (second_nibble >> 8), opcode & 0x00FF); break;
        case 0x8000:
            switch (fourth_nibble) {
                case 0x0: load_vy_to_vx(V + (second_nibble >> 8), V + (third_nibble >> 4)); break;
                case 0x1: or_vx_vy(V + (second_nibble >> 8), V + (third_nibble >> 4)); break;
                case 0x2: and_vx_vy(V + (second_nibble >> 8), V + (third_nibble >> 4)); break;
                case 0x3: xor_vx_vy(V + (second_nibble >> 8), V + (third_nibble >> 4)); break;
                case 0x4: add_vx_vy(V + (second_nibble >> 8), V + (third_nibble >> 4)); break;
                case 0x5: subtract_vx_vy(V + (second_nibble >> 8), V + (third_nibble >> 4), V + 0xF); break;
                case 0x6: shiftr_vx_vy(V + (second_nibble >> 8), V + (third_nibble >> 4), V + 0xF); break;
                case 0x7: subtract_vy_vx(V + (second_nibble >> 8), V + (third_nibble >> 4), V + 0xF); break;
                case 0xE: shiftl_vx_vy(V + (second_nibble >> 8), V + (third_nibble >> 4), V + 0xF); break;
                default: break;
            } break;
        case 0x9000: skip_vx_not_e_vy(PC, V + (second_nibble >> 8), V + (third_nibble >> 4)); break;
        case 0xA000: set_i(I, opcode & 0x0FFF); break;
        case 0xB000: break;
        case 0xC000: break;
        case 0xD000:
            draw(display_grid, RAM, I, V, second_nibble >> 8, third_nibble >> 4, fourth_nibble);
            render(renderer, display_grid);
            break;
        case 0xE000: break;
        case 0xF000: break;
        default: break;
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
