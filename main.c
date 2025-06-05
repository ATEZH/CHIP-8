#include "main.h"

char instructions[] = "\n\nWelcome to my CHIP-8 interpreter :)"
                      "\nTo run a program, enter the path as an argument (e.g. CHIP_8 [PATH TO .CH8/.ROM FILE] --[OPTION] ...)"
                      "\nPress SPACE to pause or resume (the program will be initially paused on debug mode),"
                      "\nPress N to step (when paused),"
                      "\nHere is the list of options:"
                      "\n\t--debug, -d, turn on debugger"
                      "\n\t--shift-quirk, enable shift instruction quirk,"
                      "\n\t--store-load-quirk, enable store and load instructions quirk,"
                      "\n\t--jump-offset-quirk, enable jump with offset instruction quirk\n\n";

const uint8_t *key_state;
bool debug_mode = false;
bool paused = false;
bool step = false;
bool shift_quirk = false;
bool store_load_quirk = false;
bool jump_offset_quirk = false;

int main(int argc, char *argv[]) {
    bool close = false;
    struct Context context = {
            .RAM = {0},
            .I = 0,
            .V = {0},
            .PC = PROGRAM_START_POSITION,
            .stack = { .stack = {0}, .top = 0 },
            .delay_timer = UINT8_MAX,
            .sound_timer = UINT8_MAX
    };
    uint8_t display_grid[DISPLAY_WIDTH][DISPLAY_HEIGHT] = {0};
    uint16_t opcode;

    uint32_t last_cpu_tick = SDL_GetTicks();
    uint32_t last_timer_tick = SDL_GetTicks();
    uint32_t last_frame_tick = SDL_GetTicks();
    uint32_t current_ticks;

    SDL_Window *window;
    SDL_Renderer *renderer;

    srand((unsigned int)(time(NULL) ^ clock() ^ getpid()));

    write_font_to_memory(context.RAM);
    if (read_arguments(argc, argv, context.RAM) != 0) {
        exit(EXIT_SUCCESS);
    }
    printf("Settings:\n  Debug mode: %s\n  Shift quirk: %s\n  Load/store quirk: %s\n  Jump offset quirk: %s\n",
           debug_mode ? "On" : "Off",
           shift_quirk ? "On" : "Off",
           store_load_quirk ? "On" : "Off",
           jump_offset_quirk ? "On" : "Off");

    if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_DEBUG);
        SDL_Log("SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }
    key_state = SDL_GetKeyboardState(NULL);
    window = SDL_CreateWindow("CHIP-8",
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              DISPLAY_WIDTH * BLOCK_SIZE, DISPLAY_HEIGHT * BLOCK_SIZE, 0);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    while (!close) {
        current_ticks = SDL_GetTicks();

        // --- 1. INPUT HANDLING ---
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            SDL_Scancode sc = event.key.keysym.scancode;
            if (event.type == SDL_QUIT) {
                close = true;
            } else if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
                if (sc == SDL_SCANCODE_SPACE) {
                    paused = !paused;
                    if (paused || !debug_mode) {
                        step = false;
                    }
                    DEBUG_PRINT("Paused: %s\n", paused ? "Yes" : "No");
                } else if (sc == SDL_SCANCODE_N && paused) {
                    step = true;
                    DEBUG_PRINT("Step one instruction\n");
                }
            }
        }

        // === 2. CPU INSTRUCTION EXECUTION ===
        if (!paused || step) {
            while ((double)(current_ticks - last_cpu_tick) >= CPU_INSTR_MS) {
                fetch(&opcode, &context.PC, context.RAM);
                decode_execute(opcode, &context, (uint8_t *) display_grid, renderer);

                last_cpu_tick += CPU_INSTR_MS;

                if (step) {
                    step = false;
                    break;
                }
            }
        }

        // === 3. TIMER DECREMENT ===
        while ((double)(current_ticks - last_timer_tick) >= TIMER_TICK_MS) {
            decrement_timers(&context.delay_timer, &context.sound_timer);
            last_timer_tick += TIMER_TICK_MS;
        }

        // === 4. RENDERING ===
        if ((double)(current_ticks - last_frame_tick) >= FRAME_MS) {
            render_clear(renderer);
            render_drawing(renderer, (uint8_t *) display_grid);
            SDL_RenderPresent(renderer);
            last_frame_tick = current_ticks;
        }
        SDL_Delay(1);
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

void clear_screen(uint8_t *display) {
    DEBUG_PRINT("00E0 - Clear the display\n\n");
    uint8_t *i = display;
    while (i < display + DISPLAY_WIDTH * DISPLAY_HEIGHT) *(i++) = 0;
}

void return_from_subroutine(struct Stack *stack, uint16_t *PC) {
    *PC = pop(stack);
    DEBUG_PRINT("00EE - Return from subroutine,\n"
                "          PC  = %.4X\n\n", *PC);
}

void jump(uint16_t *PC, uint16_t location) {
    DEBUG_PRINT("1NNN - Jump to location,\n"
                "          NNN = %.4X\n\n", location);
    *PC = location;
}

void call_subroutine(struct Stack *stack, uint16_t *PC, uint16_t location) {
    DEBUG_PRINT("2NNN - Call subroutine at NNN,\n"
                "          PC  = %.4X, NNN = %.4X\n\n", *PC, location);
    push(stack, *PC);
    *PC = location;
}

void skip_vx_e_nn(uint16_t *PC, uint8_t V, uint8_t value) {
    DEBUG_PRINT("3XKK - Skip next instruction if VX = KK,\n"
                "          VX  = %.2X, KK  = %.2X\n\n", V, value);
    if (V == value) *PC += 2;
}

void skip_vx_not_e_nn(uint16_t *PC, uint8_t V, uint8_t value) {
    DEBUG_PRINT("4XKK - Skip next instruction if VX != KK,\n"
                "          VX  = %.2X, KK  = %.2X\n\n", V, value);
    if (V != value) *PC += 2;
}

void skip_vx_e_vy(uint16_t *PC, uint8_t VX, uint8_t VY) {
    DEBUG_PRINT("5XY0 - Skip next instruction if VX = VY,\n"
                "          VX  = %.2X, VY  = %.2X\n\n", VX, VY);
    if (VX == VY) *PC += 2;
}

void set_v(uint8_t *V, uint8_t value) {
    DEBUG_PRINT("6XKK - Set VX = KK,\n"
                "          VX  = %.2X, KK  = %.2X\n\n", *V, value);
    *V = value;
}

void add_v(uint8_t *V, uint8_t value) {
    DEBUG_PRINT("7XKK - Set VX = VX + KK,\n"
                "          VX  = %.2X, KK  = %.2X, VX + KK   = %.2X\n\n", *V, value, *V + value);
    *V += value;
}

void set_vx_to_vy(uint8_t *VX, uint8_t VY) {
    DEBUG_PRINT("8XY0 - Set VX = VY,\n"
                "          VX  = %.2X, VY  = %.2X\n\n", *VX, VY);
    *VX = VY;
}

void or_vx_vy(uint8_t *VX, uint8_t VY) {
    DEBUG_PRINT("8XY1 - Set VX = VX OR VY,\n"
                "          VX  = %.2X, VY  = %.2X, VX OR VY  = %.2X\n\n", *VX, VY, *VX | VY);
    *VX |= VY;
}

void and_vx_vy(uint8_t *VX, uint8_t VY) {
    DEBUG_PRINT("8XY2 - Set VX = VX AND VY,\n"
                "          VX  = %.2X, VY  = %.2X, VX AND VY = %.2X\n\n", *VX, VY, *VX & VY);
    *VX &= VY;
}

void xor_vx_vy(uint8_t *VX, uint8_t VY) {
    DEBUG_PRINT("8XY3 - Set VX = VX XOR VY,\n"
                "          VX  = %.2X, VY  = %.2X, VX XOR VY = %.2X\n\n", *VX, VY, *VX ^ VY);
    *VX ^= VY;
}

void add_vx_vy(uint8_t *VX, uint8_t VY, uint8_t *VF) {
    DEBUG_PRINT("8XY4 - Set VX = VX + VY, set VF = carry,\n"
                "          VX  = %.2X, VY  = %.2X, VX + VY   = %.2X, VF  = %.2X\n\n", *VX, VY, *VX + VY, *VX > (0xFF - VY));
    uint8_t VF_t = *VX > (0xFF - VY);
    *VX += VY;
    *VF = VF_t;
}

void subtract_vx_vy(uint8_t *VX, uint8_t VY, uint8_t *VF) {
    DEBUG_PRINT("8XY5 - Set VX = VX - VY, set VF = NOT borrow,\n"
                "          VX  = %.2X, VY  = %.2X, VX - VY   = %.2X, VF  = %.2X\n\n", *VX, VY, *VX - VY, *VX >= VY);
    uint8_t VF_t = *VX >= VY;
    *VX = *VX - VY;
    *VF = VF_t;
}

void shiftr_vx_vy(uint8_t *VX, uint8_t VY, uint8_t *VF) {
    uint8_t target = shift_quirk ? VY : *VX;
    DEBUG_PRINT("8XY6 - Set VX = %s >> 1, set VF = LSb of value\n"
                "          VX  = %.2X, VY  = %.2X, VX >> 1   = %.2X, VF  = %.2X\n\n",
                shift_quirk ? "VY" : "VX", *VX, VY, target >> 1, target & 0x01);
    *VF = target & 0x01;
    *VX = target >> 1;
}

void subtract_vy_vx(uint8_t *VX, uint8_t VY, uint8_t *VF) {
    DEBUG_PRINT("8XY7 - Set VX = VY - VX, set VF = NOT borrow,\n"
                "          VX  = %.2X, VY  = %.2X, VY - VX   = %.2X, VF  = %.2X\n\n", *VX, VY, VY - *VX, VY >= *VX);
    uint8_t VF_t = VY >= *VX;
    *VX = VY - *VX;
    *VF = VF_t;
}

void shiftl_vx_vy(uint8_t *VX, uint8_t VY, uint8_t *VF) {
    uint8_t target = shift_quirk ? VY : *VX;
    DEBUG_PRINT("8XYE - Set VX = %s << 1, set VF = MSb of value\n"
                "          VX  = %.2X, VY  = %.2X, VX << 1   = %.2X, VF  = %.2X\n\n",
                shift_quirk ? "VY" : "VX", *VX, VY, target << 1, (target & 0x80) >> 7);
    *VF = (target & 0x80) >> 7;
    *VX = target << 1;
}
void skip_vx_not_e_vy(uint16_t *PC, uint8_t VX, uint8_t VY) {
    DEBUG_PRINT("9XY0 - Skip one instruction if VX == VY,\n"
                "          VX  = %.2X, VY  = %.2X, VX == VY  = %d\n\n", VX, VY, VX == VY);
    if (VX != VY) *PC += 2;
}

void set_i(uint16_t *I, uint16_t address) {
    DEBUG_PRINT("ANNN - Jump to location nnn,\n"
                "          NNN = %.3X\n\n", address);
    *I = address;
}

void jump_offset(uint16_t *PC, uint16_t address, uint8_t V0, uint8_t VX) {
    uint8_t offset = jump_offset_quirk ? VX : V0;
    DEBUG_PRINT("BNNN - Jump to location NNN + %s,\n"
                "          NNN = %.3X, %s = %.2X, target = %.4X\n\n",
                jump_offset_quirk ? "VX" : "V0", address,
                jump_offset_quirk ? "VX" : "V0", offset, address + offset);

    *PC = address + offset;
}
void random(uint8_t *V, uint8_t value) {
    *V = (uint8_t)(rand() & 0xFF) & value;
    DEBUG_PRINT("CXKK - Set VX = (random byte & KK),\n"
                "          VX  = %.2X\n\n", *V);
}

void skip_key_v(uint8_t V, uint16_t *PC) {
    DEBUG_PRINT("EXA1 - Skip if key with the value VX is pressed,\n"
                "          VX  = %.2X, Pressed = %d\n\n", V, key_state[keypad_to_scancode(V)]);
    if (key_state[keypad_to_scancode(V)]) *PC += 2;
}

void skip_key_n_v(uint8_t V, uint16_t *PC) {
    DEBUG_PRINT("EXA1 - Skip if key with the value VX is NOT pressed,\n"
                "          VX  = %.2X, Pressed = %d\n\n", V, key_state[keypad_to_scancode(V)]);
    if (!key_state[keypad_to_scancode(V)]) *PC += 2;
}

void set_v_delay(uint8_t *V, uint8_t delay_timer) {
    DEBUG_PRINT("FX07 - Set VX = delay timer value,\n"
                "          Delay Timer = %.2X\n\n", delay_timer);
    *V = delay_timer;
}

void set_delay_v(uint8_t *delay_timer, uint8_t V) {
    DEBUG_PRINT("FX15 - Set delay timer = VX,\n"
                "          VX  = %.2X\n\n", V);
    *delay_timer = V;
}

void set_sound_v(uint8_t *sound_timer, uint8_t V) {
    DEBUG_PRINT("FX18 - Set sound timer = VX,\n"
                "          VX  = %.2X\n\n", V);
    *sound_timer = V;
}

void add_i_v(uint16_t *I, uint8_t V, uint8_t *VF) {
    DEBUG_PRINT("FX1E - Set I = I + VX,\n"
                "          I = %.4X, VX  = %.2X, I + VX = %.4X\n\n", *I, V, *I + V);
    if (*I + V > 0xFFF) *VF = 1;
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
    DEBUG_PRINT("FX0A - Wait for a key press, store in VX,\n"
                "          Pressed = %d, Key = %.1X\n\n", pressed, pressed ? *V : 0);
    if (!pressed) *PC -= 2;
}

void font_character(uint16_t *I, uint8_t V) {
    DEBUG_PRINT("FX29 - Set I = location of sprite for VX,\n"
                "          VX  = %.2X, I = %.4X\n\n", V, FONT_START_POSITION + NUM_OF_FONT_CHARACTER_BYTES * V);
    if (V <= 0xF) *I = FONT_START_POSITION + NUM_OF_FONT_CHARACTER_BYTES * V;
}

void binary_coded_decimal_conversion(uint8_t *RAM, uint16_t I, uint8_t V) {
    DEBUG_PRINT("FX33 - Store BCD of VX in memory at I, I+1, I+2,\n"
                "          VX  = %.2X -> [%d, %d, %d] at I = %.4X\n\n",
                V, V / 100, (V / 10) % 10, V % 10, I);
    *(RAM + I + 2) = V % 10;
    V /= 10;
    *(RAM + I + 1) = V % 10;
    V /= 10;
    *(RAM + I) = V;
}

void store_to_memory(uint8_t *RAM, uint16_t *I, uint8_t *V, uint8_t VX) {
    DEBUG_PRINT("FX55 - Store V0 through VX into memory starting at I,\n"
                "          I = %.4X, VX  = %.2X\n\n", *I, VX);
    uint16_t tempI = *I;
    for (uint8_t i = 0; i <= VX; i++) {
        RAM[tempI++] = V[i];
    }
    if (store_load_quirk) {
        *I += VX + 1;
    }
}

void load_from_memory(uint8_t *RAM, uint16_t *I, uint8_t *V, uint8_t VX) {
    DEBUG_PRINT("FX65 - Load V0 through VX from memory starting at I,\n"
                "          I = %.4X, VX  = %.2X\n\n", *I, VX);
    uint16_t tempI = *I;
    for (uint8_t i = 0; i <= VX; i++) {
        V[i] = RAM[tempI++];
    }
    if (store_load_quirk) {
        *I += VX + 1;
    }
}
void draw(uint8_t *display_grid, uint8_t *RAM, uint16_t *I, uint8_t *V, uint8_t VX, uint8_t VY, uint8_t N) {
    uint8_t col, pixel, row, byte;
    uint16_t position;
    VX = *(V + VX) & (DISPLAY_WIDTH - 1);
    VY = *(V + VY) & (DISPLAY_HEIGHT - 1);
    *(V + 0xF) = 0;
    DEBUG_PRINT("DXYN - Draw sprite at (VX, VY) = (%d, %d) with height %u from I = %.4X\n\n", VX, VY, N, *I);
    for (row = 0; row < N; row++) {
        byte = *(RAM + *I + row);
        for (col = 0; col < 8; col++) {
            pixel = (byte >> (7 - col)) & 0x1;
            if (pixel) {
                position = (VY + row) % DISPLAY_HEIGHT * DISPLAY_WIDTH + (VX + col) % DISPLAY_WIDTH;
                *(V + 0xF) |= *(display_grid + position);
                *(display_grid + position) ^= 1;
            }
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

void fetch(uint16_t *opcode, uint16_t *PC, uint8_t *RAM) {
    *opcode = ((uint16_t)(*(RAM + *PC)) << 8) + *(RAM + *PC + 1);
    *PC += 2;
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
        case 0xB000: jump_offset(&ctx->PC, opcode & 0x0FFF, ctx->V[0x0], ctx->V[nib2]); break;
        case 0xC000: random(&ctx->V[nib2], opcode & 0x00FF); break;
        case 0xD000:
            draw(display_grid, ctx->RAM, &ctx->I, ctx->V, nib2, nib3, nib4);
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

int read_arguments(int argc, char *argv[], uint8_t *RAM) {
    int32_t i = 1;
    if (!argv[1] || strlen(argv[1]) == 0) {
        printf("%s", instructions);
        return -1;
    }
    for (; i < argc; i++) {
        if ((strcmp("--help", argv[i]) == 0) || (strcmp("-h", argv[i]) == 0)) {
            printf("%s", instructions);
            return -1;
        } else if (strcmp("--debug", argv[i]) == 0 || strcmp("-d", argv[i]) == 0) {
            debug_mode = true; paused = true;
        } else if (strcmp("--shift-quirk", argv[i]) == 0) {
            shift_quirk = true;
        } else if (strcmp("--store-load-quirk", argv[i]) == 0) {
            store_load_quirk = true;
        } else if (strcmp("--jump-offset-quirk", argv[i]) == 0) {
            jump_offset_quirk = true;
        } else if (i == 1) {
            write_program_to_memory(argv[1], RAM + PROGRAM_START_POSITION);
        }
    }
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

void decrement_timers(uint8_t *delay_timer, uint8_t *sound_timer) {
    if (*delay_timer > 0) {
        (*delay_timer)--;
    }
    if (*sound_timer > 0) {
        (*sound_timer)--;
    }
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
