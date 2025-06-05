#ifndef CHIP_8_MAIN_H
#define CHIP_8_MAIN_H
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
#define DEBUG_PRINT(fmt, ...) do { if (debug_mode) fprintf(stderr, fmt, ##__VA_ARGS__); } while (0)

#define CPU_SPEED_HZ 700
#define TIMER_SPEED_HZ 60
#define FPS 60
#define CPU_INSTR_MS (1000.0f / CPU_SPEED_HZ)
#define TIMER_TICK_MS (1000.0f / TIMER_SPEED_HZ)
#define FRAME_MS (1000.0f / FPS)

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
void jump_offset(uint16_t *PC, uint16_t address, uint8_t V0, uint8_t VX);
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
int read_arguments(int argc, char *argv[], uint8_t *RAM);
void write_program_to_memory(char *path, uint8_t *RAM);
void write_font_to_memory(uint8_t *RAM);
void decrement_timers(uint8_t *delay_timer, uint8_t *sound_timer);
uint8_t keypad_to_scancode(uint8_t k);
void fetch(uint16_t *opcode, uint16_t *PC, uint8_t *RAM);
void decode_execute(uint16_t opcode, struct Context *ctx, uint8_t *display_grid, SDL_Renderer *renderer);
void render_drawing(SDL_Renderer *renderer, uint8_t *display_grid);
void render_clear(SDL_Renderer *renderer);

#endif //CHIP_8_MAIN_H