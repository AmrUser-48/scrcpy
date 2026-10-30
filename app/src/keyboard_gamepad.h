#ifndef SC_KEYBOARD_GAMEPAD_H
#define SC_KEYBOARD_GAMEPAD_H

#include "common.h"

#include <stdbool.h>
#include <SDL2/SDL.h>

#include "trait/gamepad_processor.h"

struct sc_keyboard_gamepad {
    struct sc_gamepad_processor *gp;
    bool keys[SDL_NUM_SCANCODES];
    uint32_t buttons_state;
    int16_t axes_state[SDL_CONTROLLER_AXIS_MAX];
    bool auto_r_active;
    bool auto_x_pulse;
    SDL_TimerID auto_r_timer;
    uint32_t auto_r_event_type;
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
