#ifndef SC_KEYBOARD_GAMEPAD_H
#define SC_KEYBOARD_GAMEPAD_H

#include "common.h"

#include <stdbool.h>
#include <stdint.h>
#include <SDL3/SDL.h>

#include "trait/gamepad_processor.h"

struct sc_keyboard_gamepad {
    struct sc_gamepad_processor *gp;
    bool keys[SDL_SCANCODE_COUNT];
    uint32_t buttons_state;
    int16_t axes_state[SDL_GAMEPAD_AXIS_COUNT];
    bool auto_x_active;
    bool auto_x_pulse;
    SDL_TimerID auto_x_timer;
    bool initialized;
};

bool
sc_keyboard_gamepad_init(struct sc_keyboard_gamepad *kg,
                         struct sc_gamepad_processor *gp);

void
sc_keyboard_gamepad_destroy(struct sc_keyboard_gamepad *kg);

void
sc_keyboard_gamepad_process_key(struct sc_keyboard_gamepad *kg,
                                const SDL_KeyboardEvent *event);

bool
sc_keyboard_gamepad_process_event(struct sc_keyboard_gamepad *kg,
                                  const SDL_Event *event);

#endif
