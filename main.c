#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <SDL.h>

#define RAM_SIZE 4096
#define STACK_SIZE 32
#define NUM_OF_VREGISTERS 16
#define DISPLAY_WIDTH 64
#define DISPLAY_HEIGHT 32
#define BLOCK_SIZE 10
#define PROGRAM_START_POSITION 0x200
#define FONT_START_POSITION 0x0
#define NUM_OF_FONT_CHARACTER_BYTES 5

struct Stack {
    uint16_t stack[STACK_SIZE];
    uint8_t top;
};

struct Context {
    uint8_t RAM[RAM_SIZE];
    uint16_t I;
    uint8_t V[NUM_OF_VREGISTERS];
    uint16_t PC;
    struct Stack stack;
    uint8_t delay_timer;
    uint8_t sound_timer;
};

uint16_t pop(struct Stack *stack);
void push(struct Stack *stack, uint16_t value);
void stack_overflow(void);
void stack_underflow(void);

// 0
void clear_screen(uint8_t *display);
void return_from_subroutine(struct Stack *stack, uint16_t *PC);
// 1
void jump(uint16_t *PC, uint16_t location);
// 2
void call_subroutine(struct Stack *stack, uint16_t *PC, uint16_t location);
// 3
void skip_vx_e_nn(uint16_t *PC, uint8_t V, uint8_t value);
// 4
void skip_vx_not_e_nn(uint16_t *PC, uint8_t V, uint8_t value);
// 5
void skip_vx_e_vy(uint16_t *PC, uint8_t VX, uint8_t VY);
// 6
void set_v(uint8_t *V, uint8_t value);
// 7
void add_v(uint8_t *V, uint8_t value);
// 8
void set_vx_to_vy(uint8_t *VX, uint8_t VY);
void or_vx_vy(uint8_t *VX, uint8_t VY);
void and_vx_vy(uint8_t *VX, uint8_t VY);
void xor_vx_vy(uint8_t *VX, uint8_t VY);
void add_vx_vy(uint8_t *VX, uint8_t VY, uint8_t *VF);
void subtract_vx_vy(uint8_t *VX, uint8_t VY, uint8_t *VF);
void shiftr_vx_vy(uint8_t *VX, uint8_t VY, uint8_t *VF);
void subtract_vy_vx(uint8_t *VX, uint8_t VY, uint8_t *VF);
void shiftl_vx_vy(uint8_t *VX, uint8_t VY, uint8_t *VF);
// 9
void skip_vx_not_e_vy(uint16_t *PC, uint8_t VX, uint8_t VY);
// A
void set_i(uint16_t *I, uint16_t value);
// B
void jump_offset(uint16_t *PC, uint16_t address, uint8_t V0);
// C
void random(uint8_t *V, uint8_t value);
// D
void draw(uint8_t *display_grid, uint8_t *RAM, uint16_t *I, uint8_t *V, uint8_t VX, uint8_t VY, uint8_t N);
// E
void skip_key_v(uint8_t V, uint16_t *PC);
void skip_key_n_v(uint8_t V, uint16_t *PC);
// F
void set_v_delay(uint8_t *V, uint8_t delay_timer);
void set_delay_v(uint8_t *delay_timer, uint8_t V);
void set_sound_v(uint8_t *sound_timer, uint8_t V);
void add_i_v(uint16_t *I, uint8_t V, uint8_t *VF);
void get_key(uint8_t *V, uint16_t *PC);
void font_character(uint16_t *I, uint8_t V);
void binary_coded_decimal_conversion(uint8_t *RAM, uint16_t I, uint8_t V);
void store_to_memory(uint8_t *RAM, uint16_t *I, uint8_t *V, uint8_t VX);
void load_from_memory(uint8_t *RAM, uint16_t *I, uint8_t *V, uint8_t VX);
// utility
void write_program_to_memory(char *path, uint8_t *RAM);
void write_font_to_memory(uint8_t *RAM);
void decrement_timers(uint8_t *delay_timer, uint8_t *sound_timer, uint32_t delta);
uint8_t keypad_to_scancode(uint8_t k);
void fetch(uint16_t *opcode, uint16_t *PC, uint8_t *RAM);
void decode_execute(uint16_t opcode, struct Context *ctx, uint8_t *display_grid, SDL_Renderer *renderer);
void render_drawing(SDL_Renderer *renderer, uint8_t *display_grid);
void render_clear(SDL_Renderer *renderer);

const Uint8 *key_state;
int32_t global_delta = 0;

int main(int argc, char *argv[]) {
    key_state = SDL_GetKeyboardState(NULL);
    struct Context context = {
            .RAM = {0},
            .I = 0,
            .V = {0},
            .PC = PROGRAM_START_POSITION,
            .stack = { .stack = {0}, .top = 0 },
            .delay_timer = UINT8_MAX,
            .sound_timer = UINT8_MAX
    };
    uint16_t opcode;
    uint32_t last_ticks = SDL_GetTicks();
    uint32_t last_delta = 0;

    uint8_t display_grid[DISPLAY_WIDTH][DISPLAY_HEIGHT] = {0};
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Event event;
    bool close = false;

    srand(time(NULL) ^ clock() ^ getpid());

    write_font_to_memory(context.RAM);
    write_program_to_memory(argv[1], context.RAM + PROGRAM_START_POSITION);

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
        last_delta = SDL_GetTicks() - last_ticks;
        last_ticks = SDL_GetTicks();
        for (int i = 0; i < 16; i++) {
            fetch(&opcode, &context.PC, context.RAM);
            decode_execute(opcode, &context, (uint8_t *) display_grid, renderer);
            decrement_timers(&context.delay_timer, &context.sound_timer, last_delta);
        }
        uint32_t render_time = SDL_GetTicks() - last_ticks;
        if (render_time < 16) {
            SDL_Delay(16 - render_time);
        } else {
            SDL_Delay(1);
        }
    }
    SDL_Quit();
    return 0;
}

void write_font_to_memory(uint8_t *RAM) {
    uint8_t font[] = {
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
            0xF0, 0x80, 0xF0, 0x80, 0x80  // F
    };
    memcpy(RAM + FONT_START_POSITION, font, sizeof(font));
}

void decrement_timers(uint8_t *delay_timer, uint8_t *sound_timer, uint32_t delta) {
    global_delta += delta;
    while (global_delta > (1000 / 60)) {
        global_delta -= (1000 / 60);
        if (*delay_timer > 0) {
            (*delay_timer)--;
        }
        if (*sound_timer > 0) {
            (*sound_timer)--;
        }
    }
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

void jump_offset(uint16_t *PC, uint16_t address, uint8_t V0) {
    *PC = address + V0;
}

void random(uint8_t *V, uint8_t value) {
    *V = (uint8_t)(rand() & 0xFF) & value;
}

void add_v(uint8_t *V, uint8_t value) {
    *V += value;
}

void skip_vx_e_nn(uint16_t *PC, uint8_t V, uint8_t value) {
    if (V == value) *PC += 2;
}

void skip_vx_not_e_nn(uint16_t *PC, uint8_t V, uint8_t value) {
    if (V != value) *PC += 2;
}

void skip_vx_e_vy(uint16_t *PC, uint8_t VX, uint8_t VY) {
    if (VX == VY) *PC += 2;
}

void skip_vx_not_e_vy(uint16_t *PC, uint8_t VX, uint8_t VY) {
    if (VX != VY) *PC += 2;
}

void set_vx_to_vy(uint8_t *VX, uint8_t VY) {
    *VX = VY;
}

void or_vx_vy(uint8_t *VX, uint8_t VY) {
    *VX |= VY;
}

void and_vx_vy(uint8_t *VX, uint8_t VY) {
    *VX &= VY;
}

void xor_vx_vy(uint8_t *VX, uint8_t VY) {
    *VX ^= VY;
}

void add_vx_vy(uint8_t *VX, uint8_t VY, uint8_t *VF) {
    uint8_t VF_t = *VX > ((*VX + VY) & 0xFF);
    *VX += VY;
    *VF = VF_t;
}

void subtract_vx_vy(uint8_t *VX, uint8_t VY, uint8_t *VF) {
    uint8_t VF_t = *VX > VY;
    *VX = *VX - VY;
    *VF = VF_t;
}

void shiftr_vx_vy(uint8_t *VX, uint8_t VY, uint8_t *VF) {
    uint8_t VF_t = ((*VX & 0x01) == 0x01);
    *VX >>= 1;
    *VF = VF_t;
}

void subtract_vy_vx(uint8_t *VX, uint8_t VY, uint8_t *VF) {
    uint8_t VF_t = VY > *VX;
    *VX = VY - *VX;
    *VF = VF_t;
}

void shiftl_vx_vy(uint8_t *VX, uint8_t VY, uint8_t *VF) {
    uint8_t VF_t = ((*VX & 0x80) != 0x0);
    *VX <<= 1;
    *VF = VF_t;
}

void skip_key_v(uint8_t V, uint16_t *PC) {
    if (key_state[keypad_to_scancode(V)]) *PC += 2;
}

void skip_key_n_v(uint8_t V, uint16_t *PC) {
    if (!key_state[keypad_to_scancode(V)]) *PC += 2;
}

void set_v_delay(uint8_t *V, uint8_t delay_timer) {
    *V = delay_timer;
}

void set_delay_v(uint8_t *delay_timer, uint8_t V) {
    *delay_timer = V;
}

void set_sound_v(uint8_t *sound_timer, uint8_t V) {
    *sound_timer = V;
}

void add_i_v(uint16_t *I, uint8_t V, uint8_t *VF) {
    *VF = (*I + V > 0xFFF);
    *I += V;
}

void get_key(uint8_t *V, uint16_t *PC) {
    uint8_t key;
    bool pressed = false;
    for (key = 0; key < 0x10; key++) {
        if (key_state[keypad_to_scancode(key)]) {
            pressed = true;
            *V = key;
            break;
        }
    }
    if (!pressed) *PC -= 2;
}

void font_character(uint16_t *I, uint8_t V) {
    if (V <= 0xF) *I = FONT_START_POSITION + NUM_OF_FONT_CHARACTER_BYTES * V;
}

void binary_coded_decimal_conversion(uint8_t *RAM, uint16_t I, uint8_t V) {
    *(RAM + I + 2) = V % 10;
    V /= 10;
    *(RAM + I + 1) = V % 10;
    V /= 10;
    *(RAM + I) = V;
}

void store_to_memory(uint8_t *RAM, uint16_t *I, uint8_t *V, uint8_t VX) {
    uint16_t tempI = *I;
    while (tempI <= *I + VX) {
        *(RAM + tempI++) = *(V++);
    }
}

void load_from_memory(uint8_t *RAM, uint16_t *I, uint8_t *V, uint8_t VX) {
    uint16_t tempI = *I;
    while (tempI <= *I + VX) {
         *(V++) = *(RAM + tempI++);
    }
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
    while (i < display + DISPLAY_WIDTH * DISPLAY_HEIGHT) *(i++) = 0;
}

void draw(uint8_t *display_grid, uint8_t *RAM, uint16_t *I, uint8_t *V, uint8_t VX, uint8_t VY, uint8_t N) {
    int8_t shift;
    uint8_t pixel, row, byte;
    uint16_t position;
    VX = *(V + VX) & (DISPLAY_WIDTH - 1);
    VY = *(V + VY) & (DISPLAY_HEIGHT - 1);
    *(V + 0xF) = 0;
    position = VX + DISPLAY_WIDTH * VY;
    for (row = 0; row < N && position < DISPLAY_WIDTH * DISPLAY_HEIGHT; position = VX + DISPLAY_WIDTH * (VY + (++row))) {
        byte = *(RAM + *I + row);
        for (shift = 7; shift >= 0 && position < DISPLAY_WIDTH * (VY + row + 1); shift--, position++) {
            pixel = (byte >> shift) & 0x1;
            *(V + 0xF) |= *(display_grid + position) & pixel;
            *(display_grid + position) ^= (pixel);
        }
    }
}

void render_drawing(SDL_Renderer *renderer, uint8_t *display_grid) {
    uint16_t row, col;
    col = row = 0;
    SDL_Rect rect;
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    while (display_grid + row * DISPLAY_WIDTH + col < display_grid + DISPLAY_WIDTH * DISPLAY_HEIGHT) {
        if (*(display_grid + row * DISPLAY_WIDTH + col)) {
            rect.h = BLOCK_SIZE; rect.w = BLOCK_SIZE; rect.x = col * BLOCK_SIZE; rect.y = row * BLOCK_SIZE;
            SDL_RenderFillRect(renderer, &rect);
        }
        if (++col == DISPLAY_WIDTH) {col = 0; row++;}
    }
    SDL_RenderPresent(renderer);
}

void render_clear(SDL_Renderer *renderer) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
}

void decode_execute(uint16_t opcode, struct Context *ctx, uint8_t *display_grid, SDL_Renderer *renderer) {
    uint16_t nib1 = opcode & 0xF000; // first nibble
    uint16_t nib2 = (opcode & 0x0F00) >> 8; // second nibble
    uint8_t nib3 = (opcode & 0x00F0) >> 4;  // third nibble
    uint8_t nib4 = opcode & 0x000F;  // fourth nibble
    switch (nib1) {
        case 0x0000:
            switch (nib4) {
                case 0x0:
                    clear_screen(display_grid);
                    render_clear(renderer);
                    break;
                case 0xE: return_from_subroutine(&ctx->stack, &ctx->PC); break;
                default: break;
            } break;
        case 0x1000: jump(&ctx->PC, opcode & 0x0FFF); break;
        case 0x2000: call_subroutine(&ctx->stack, &ctx->PC, opcode & 0x0FFF); break;
        case 0x3000: skip_vx_e_nn(&ctx->PC, ctx->V[nib2], opcode & 0x00FF); break;
        case 0x4000: skip_vx_not_e_nn(&ctx->PC, ctx->V[nib2], opcode & 0x00FF); break;
        case 0x5000: skip_vx_e_vy(&ctx->PC, ctx->V[nib2], ctx->V[nib3]); break;
        case 0x6000: set_v(&ctx->V[nib2], opcode & 0x00FF); break;
        case 0x7000: add_v(&ctx->V[nib2], opcode & 0x00FF); break;
        case 0x8000:
            switch (nib4) {
                case 0x0: set_vx_to_vy(&ctx->V[nib2], ctx->V[nib3]); break;
                case 0x1: or_vx_vy(&ctx->V[nib2], ctx->V[nib3]); break;
                case 0x2: and_vx_vy(&ctx->V[nib2], ctx->V[nib3]); break;
                case 0x3: xor_vx_vy(&ctx->V[nib2], ctx->V[nib3]); break;
                case 0x4: add_vx_vy(&ctx->V[nib2], ctx->V[nib3], &ctx->V[0xF]); break;
                case 0x5: subtract_vx_vy(&ctx->V[nib2], ctx->V[nib3], &ctx->V[0xF]); break;
                case 0x6: shiftr_vx_vy(&ctx->V[nib2], ctx->V[nib3], &ctx->V[0xF]); break;
                case 0x7: subtract_vy_vx(&ctx->V[nib2], ctx->V[nib3], &ctx->V[0xF]); break;
                case 0xE: shiftl_vx_vy(&ctx->V[nib2], ctx->V[nib3], &ctx->V[0xF]); break;
                default: break;
            } break;
        case 0x9000: skip_vx_not_e_vy(&ctx->PC, ctx->V[nib2], ctx->V[nib3]); break;
        case 0xA000: set_i(&ctx->I, opcode & 0x0FFF); break;
        case 0xB000: jump_offset(&ctx->PC, opcode & 0x0FFF, ctx->V[0x0]); break;
        case 0xC000: random(&ctx->V[nib2], opcode & 0x00FF); break;
        case 0xD000:
            render_clear(renderer);
            draw(display_grid, ctx->RAM, &ctx->I, ctx->V, nib2, nib3, nib4);
            render_drawing(renderer, display_grid);
            break;
        case 0xE000:
            switch (opcode & 0x00FF) {
                case 0x9E: skip_key_v(ctx->V[nib2], &ctx->PC); break;
                case 0xA1: skip_key_n_v(ctx->V[nib2], &ctx->PC); break;
            } break;
        case 0xF000:
            switch (opcode & 0x00FF) {
                case 0x07: set_v_delay(&ctx->V[nib2], ctx->delay_timer); break;
                case 0x15: set_delay_v(&ctx->delay_timer, ctx->V[nib2]); break;
                case 0x18: set_sound_v(&ctx->sound_timer, ctx->V[nib2]); break;
                case 0x1E: add_i_v(&ctx->I, ctx->V[nib2], &ctx->V[0xF]); break;
                case 0x0A: get_key(ctx->V + nib2, &ctx->PC); break;
                case 0x29: font_character(&ctx->I, ctx->V[nib2]); break;
                case 0x33: binary_coded_decimal_conversion(ctx->RAM, ctx->I, ctx->V[nib2]); break;
                case 0x55: store_to_memory(ctx->RAM, &ctx->I, ctx->V, nib2); break;
                case 0x65: load_from_memory(ctx->RAM, &ctx->I, ctx->V, nib2); break;
                default: break;
            } break;
        default: break;
    }
}

uint8_t keypad_to_scancode(uint8_t k) {
    uint8_t SCANCODE;
    switch (k) {
        case 0x1: SCANCODE = SDL_SCANCODE_1; break;
        case 0x2: SCANCODE = SDL_SCANCODE_2; break;
        case 0x3: SCANCODE = SDL_SCANCODE_3; break;
        case 0xC: SCANCODE = SDL_SCANCODE_4; break;
        case 0x4: SCANCODE = SDL_SCANCODE_Q; break;
        case 0x5: SCANCODE = SDL_SCANCODE_W; break;
        case 0x6: SCANCODE = SDL_SCANCODE_E; break;
        case 0xD: SCANCODE = SDL_SCANCODE_R; break;
        case 0x7: SCANCODE = SDL_SCANCODE_A; break;
        case 0x8: SCANCODE = SDL_SCANCODE_S; break;
        case 0x9: SCANCODE = SDL_SCANCODE_D; break;
        case 0xE: SCANCODE = SDL_SCANCODE_F; break;
        case 0xA: SCANCODE = SDL_SCANCODE_Z; break;
        case 0x0: SCANCODE = SDL_SCANCODE_X; break;
        case 0xB: SCANCODE = SDL_SCANCODE_C; break;
        case 0xF: SCANCODE = SDL_SCANCODE_V; break;
        default: SCANCODE = 0xFF; break;
    }
    return SCANCODE;
}

void fetch(uint16_t *opcode, uint16_t *PC, uint8_t *RAM) {
    *opcode = ((uint16_t)(*(RAM + *PC)) << 8) + *(RAM + *PC + 1);
    *PC += 2;
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
