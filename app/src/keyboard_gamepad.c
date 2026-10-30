#include "keyboard_gamepad.h"

#include <stdint.h>
#include <string.h>

#include "input_events.h"
#include "util/log.h"

#define SC_KEYBOARD_GAMEPAD_AUTO_R_INTERVAL 1300

static bool
key_down(const struct sc_keyboard_gamepad *kg, SDL_Scancode scancode) {
    return kg->keys[scancode];
}

static void
send_button(struct sc_keyboard_gamepad *kg, enum sc_gamepad_button button,
            bool down) {
    uint32_t mask = UINT32_C(1) << button;
    bool was_down = !!(kg->buttons_state & mask);
    if (was_down == down) {
        return;
    }

    if (down) {
        kg->buttons_state |= mask;
    } else {
        kg->buttons_state &= ~mask;
    }

    struct sc_gamepad_button_event event = {
        .gamepad_id = SC_GAMEPAD_ID_KEYBOARD,
        .action = down ? SC_ACTION_DOWN : SC_ACTION_UP,
        .button = button,
    };
    kg->gp->ops->process_gamepad_button(kg->gp, &event);
}

static void
send_axis(struct sc_keyboard_gamepad *kg, enum sc_gamepad_axis axis,
          int16_t value) {
    if (kg->axes_state[axis] == value) {
        return;
    }

    kg->axes_state[axis] = value;
    struct sc_gamepad_axis_event event = {
        .gamepad_id = SC_GAMEPAD_ID_KEYBOARD,
        .axis = axis,
        .value = value,
    };
    kg->gp->ops->process_gamepad_axis(kg->gp, &event);
}

static void
update_state(struct sc_keyboard_gamepad *kg) {
    // WASD: left stick. When opposing keys are held, D overrides A and W
    // overrides S, matching the order of the original key checks.
    int16_t left_x = 0;
    int16_t left_y = 0;
    if (key_down(kg, SDL_SCANCODE_A)) {
        left_x = INT16_MIN;
    }
    if (key_down(kg, SDL_SCANCODE_D)) {
        left_x = INT16_MAX;
    }
    if (key_down(kg, SDL_SCANCODE_S)) {
        left_y = INT16_MAX;
    }
    if (key_down(kg, SDL_SCANCODE_W)) {
        left_y = INT16_MIN;
    }

    // P/O are full right/left; 0/9 are intermediate positions.
    // Later conditions have priority, as in the supplied Python script.
    int16_t right_x = 0;
    if (key_down(kg, SDL_SCANCODE_P)) {
        right_x = INT16_MAX;
    }
    if (key_down(kg, SDL_SCANCODE_O)) {
        right_x = INT16_MIN;
    }
    if (key_down(kg, SDL_SCANCODE_0)) {
        right_x = INT16_MAX / 2;
    }
    if (key_down(kg, SDL_SCANCODE_9)) {
        right_x = INT16_MIN / 2;
    }

    int16_t left_trigger =
        key_down(kg, SDL_SCANCODE_F) || key_down(kg, SDL_SCANCODE_LCTRL)
        ? INT16_MAX : 0;
    int16_t right_trigger =
        key_down(kg, SDL_SCANCODE_LALT) ? INT16_MAX : 0;

    send_axis(kg, SC_GAMEPAD_AXIS_LEFTX, left_x);
    send_axis(kg, SC_GAMEPAD_AXIS_LEFTY, left_y);
    send_axis(kg, SC_GAMEPAD_AXIS_RIGHTX, right_x);
    send_axis(kg, SC_GAMEPAD_AXIS_RIGHTY, 0);
    send_axis(kg, SC_GAMEPAD_AXIS_LEFT_TRIGGER, left_trigger);
    send_axis(kg, SC_GAMEPAD_AXIS_RIGHT_TRIGGER, right_trigger);

    send_button(kg, SC_GAMEPAD_BUTTON_SOUTH,
                key_down(kg, SDL_SCANCODE_E));
    send_button(kg, SC_GAMEPAD_BUTTON_EAST,
                key_down(kg, SDL_SCANCODE_B));
    send_button(kg, SC_GAMEPAD_BUTTON_WEST,
                key_down(kg, SDL_SCANCODE_R) || kg->auto_x_pulse);
    send_button(kg, SC_GAMEPAD_BUTTON_NORTH,
                key_down(kg, SDL_SCANCODE_C));
    send_button(kg, SC_GAMEPAD_BUTTON_LEFT_SHOULDER,
                key_down(kg, SDL_SCANCODE_Q));
    send_button(kg, SC_GAMEPAD_BUTTON_RIGHT_SHOULDER,
                key_down(kg, SDL_SCANCODE_TAB)
                || key_down(kg, SDL_SCANCODE_G));
    send_button(kg, SC_GAMEPAD_BUTTON_LEFT_STICK,
                key_down(kg, SDL_SCANCODE_L));
    send_button(kg, SC_GAMEPAD_BUTTON_RIGHT_STICK,
                key_down(kg, SDL_SCANCODE_T));
    send_button(kg, SC_GAMEPAD_BUTTON_DPAD_UP,
                key_down(kg, SDL_SCANCODE_1));
}

static void
pulse_x(struct sc_keyboard_gamepad *kg) {
    // Combine the timed pulse with the physical R key, so one source cannot
    // release X while the other source is still holding it.
    kg->auto_x_pulse = true;
    update_state(kg);
    kg->auto_x_pulse = false;
    update_state(kg);
}

static Uint32 SDLCALL
auto_r_timer_callback(Uint32 interval, void *userdata) {
    struct sc_keyboard_gamepad *kg = userdata;

    SDL_Event event;
    SDL_zero(event);
    event.type = kg->auto_r_event_type;
    if (SDL_PushEvent(&event) < 0) {
        // The timer is independent from the UI thread. Do not call the
        // gamepad processor here; input reports must be sent on the UI thread.
        return interval;
    }

    // Keep one periodic timer alive while auto-X is enabled. Returning the
    // interval avoids removing/recreating timers from the UI event handler.
    return interval;
}

static void
schedule_auto_r(struct sc_keyboard_gamepad *kg) {
    kg->auto_r_timer = SDL_AddTimer(
        SC_KEYBOARD_GAMEPAD_AUTO_R_INTERVAL, auto_r_timer_callback, kg);
    if (!kg->auto_r_timer) {
        LOGW("Could not schedule keyboard gamepad auto-X: %s", SDL_GetError());
    }
}

bool
sc_keyboard_gamepad_init(struct sc_keyboard_gamepad *kg,
                         struct sc_gamepad_processor *gp) {
    memset(kg, 0, sizeof(*kg));

    if (!gp || !gp->ops) {
        LOGE("Keyboard gamepad requires a gamepad input processor");
        return false;
    }

    if (SDL_InitSubSystem(SDL_INIT_TIMER | SDL_INIT_EVENTS)) {
        LOGE("Could not initialize SDL timer: %s", SDL_GetError());
        return false;
    }

    kg->auto_r_event_type = SDL_RegisterEvents(1);
    if (kg->auto_r_event_type == (uint32_t) -1) {
        LOGE("Could not register keyboard gamepad timer event: %s",
             SDL_GetError());
        return false;
    }

    kg->gp = gp;
    kg->initialized = true;

    struct sc_gamepad_device_event event = {
        .gamepad_id = SC_GAMEPAD_ID_KEYBOARD,
    };
    gp->ops->process_gamepad_added(gp, &event);
    LOGI("Keyboard gamepad enabled (Xbox 360 HID)");
    return true;
}

void
sc_keyboard_gamepad_destroy(struct sc_keyboard_gamepad *kg) {
    if (!kg->initialized) {
        return;
    }

    if (kg->auto_r_timer) {
        SDL_RemoveTimer(kg->auto_r_timer);
        kg->auto_r_timer = 0;
    }

    for (int button = 0; button <= SC_GAMEPAD_BUTTON_DPAD_RIGHT; ++button) {
        if (kg->buttons_state & (UINT32_C(1) << button)) {
            send_button(kg, (enum sc_gamepad_button) button, false);
        }
    }

    for (int axis = 0; axis < SDL_CONTROLLER_AXIS_MAX; ++axis) {
        send_axis(kg, (enum sc_gamepad_axis) axis, 0);
    }

    struct sc_gamepad_device_event event = {
        .gamepad_id = SC_GAMEPAD_ID_KEYBOARD,
    };
    kg->gp->ops->process_gamepad_removed(kg->gp, &event);
    kg->initialized = false;
}

void
sc_keyboard_gamepad_process_key(struct sc_keyboard_gamepad *kg,
                                const SDL_KeyboardEvent *event) {
    if (!kg->initialized) {
        return;
    }

    SDL_Scancode scancode = event->keysym.scancode;
    if (scancode <= SDL_SCANCODE_UNKNOWN || scancode >= SDL_NUM_SCANCODES) {
        return;
    }

    bool down = event->type == SDL_KEYDOWN;
    bool was_down = kg->keys[scancode];
    if (down) {
        if (event->repeat || was_down) {
            return;
        }
        kg->keys[scancode] = true;
    } else {
        if (!was_down) {
            return;
        }
        kg->keys[scancode] = false;
    }

    if (scancode == SDL_SCANCODE_4 && down) {
        kg->auto_r_active = !kg->auto_r_active;
        if (kg->auto_r_active) {
            pulse_x(kg);
            schedule_auto_r(kg);
            LOGI("Keyboard gamepad auto-X enabled");
        } else {
            if (kg->auto_r_timer) {
                SDL_RemoveTimer(kg->auto_r_timer);
                kg->auto_r_timer = 0;
            }
            LOGI("Keyboard gamepad auto-X disabled");
        }
    }

    update_state(kg);
}

bool
sc_keyboard_gamepad_process_event(struct sc_keyboard_gamepad *kg,
                                  const SDL_Event *event) {
    if (!kg->initialized || event->type != kg->auto_r_event_type) {
        return false;
    }

    if (kg->auto_r_active) {
        pulse_x(kg);
    }
    return true;
}
