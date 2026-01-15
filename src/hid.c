#include "./hid.h"
#include "./log.h"

#include "north/hid/pad.h"
#include "north/hid/kbd.h"

#include <SDL3/SDL.h>

void hid_init() {
    SDL_Init(SDL_INIT_GAMEPAD);
    hid_key_up(0);  // fake phantom key up to initialize kbd
}

void hid_shutdown() {
    SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
}

// SDL3 event handling
void sdl_poll_events(void) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT: {
                sapp_request_quit();
            } break;

            case SDL_EVENT_JOYSTICK_ADDED: {
                if (SDL_IsGamepad(event.jdevice.which)) {
                    // Let's wait for the gamepad added event
                    continue;
                }

                LOG_INFO("SDL Joystick %d added.", event.jdevice.which);
                SDL_Joystick* joystick = SDL_OpenJoystick(event.jdevice.which);
                if (!joystick) {
                    LOG_ERROR("SDL_OpenJoystick failed: %s", SDL_GetError());
                    continue;
                }

                if (!pad_mount(
                        event.jdevice.which,
                        (void*)joystick,
                        0,
                        SDL_GetJoystickVendor(joystick),
                        SDL_GetJoystickProduct(joystick))) {
                    LOG_ERROR("pad_mount failed for joystick %d", event.jdevice.which);
                    SDL_CloseJoystick(joystick);
                }
            } break;
            case SDL_EVENT_JOYSTICK_REMOVED: {
                if (SDL_IsGamepad(event.jdevice.which)) continue;
                LOG_INFO("SDL Joystick %d removed", event.jdevice.which);
                SDL_CloseJoystick(SDL_GetJoystickFromID(event.jdevice.which));
                pad_umount(event.jdevice.which);
            } break;
            case SDL_EVENT_GAMEPAD_ADDED: {
                LOG_INFO("SDL Gamepad %d added", event.gdevice.which);
                SDL_Gamepad* gamepad = SDL_OpenGamepad(event.gdevice.which);
                if (!gamepad) {
                    LOG_ERROR("SDL_OpenGamepad failed: %s", SDL_GetError());
                    continue;
                }

                if (!pad_mount(
                        event.gdevice.which,
                        (void*)gamepad,
                        0,
                        SDL_GetGamepadVendor(gamepad),
                        SDL_GetGamepadProduct(gamepad))) {
                    LOG_ERROR("pad_mount failed for Gamepad %d", event.gdevice.which);
                    SDL_CloseGamepad(gamepad);
                }
            } break;
            case SDL_EVENT_GAMEPAD_REMOVED: {
                LOG_INFO("SDL Gamepad %d removed", event.gdevice.which);
                SDL_CloseGamepad(SDL_GetGamepadFromID(event.gdevice.which));
                pad_umount(event.gdevice.which);
            } break;

            case SDL_EVENT_JOYSTICK_UPDATE_COMPLETE:
            case SDL_EVENT_GAMEPAD_UPDATE_COMPLETE: {
                // could use this to update force feedback effects
            } break;

            default: {
                if (event.type >= SDL_EVENT_JOYSTICK_AXIS_MOTION && event.type < SDL_EVENT_JOYSTICK_UPDATE_COMPLETE) {
                    // Joystick event
                    if (SDL_IsGamepad(event.jdevice.which)) continue;
                    pad_report(
                        event.jdevice.which,
                        (const void*)SDL_GetJoystickFromID(event.jdevice.which),
                        event.type);
                }
                else if (
                    event.type >= SDL_EVENT_GAMEPAD_AXIS_MOTION && event.type < SDL_EVENT_GAMEPAD_UPDATE_COMPLETE) {
                    // Gamepad event
                    pad_report(event.jdevice.which, (const void*)SDL_GetGamepadFromID(event.jdevice.which), event.type);
                }
                else {
                    LOG_WARNING("Unhandled SDL event type: 0x%X", event.type)
                };
            } break;
        }
    }
}

#include <class/hid/hid.h>
static uint8_t sokol2usb[] = {
    [SAPP_KEYCODE_SPACE] = HID_KEY_SPACE,
    [SAPP_KEYCODE_APOSTROPHE] = HID_KEY_APOSTROPHE,
    [SAPP_KEYCODE_COMMA] = HID_KEY_COMMA,
    [SAPP_KEYCODE_MINUS] = HID_KEY_MINUS,
    [SAPP_KEYCODE_PERIOD] = HID_KEY_PERIOD,
    [SAPP_KEYCODE_SLASH] = HID_KEY_SLASH,
    [SAPP_KEYCODE_0] = HID_KEY_0,
    [SAPP_KEYCODE_1] = HID_KEY_1,
    [SAPP_KEYCODE_2] = HID_KEY_2,
    [SAPP_KEYCODE_3] = HID_KEY_3,
    [SAPP_KEYCODE_4] = HID_KEY_4,
    [SAPP_KEYCODE_5] = HID_KEY_5,
    [SAPP_KEYCODE_6] = HID_KEY_6,
    [SAPP_KEYCODE_7] = HID_KEY_7,
    [SAPP_KEYCODE_8] = HID_KEY_8,
    [SAPP_KEYCODE_9] = HID_KEY_9,
    [SAPP_KEYCODE_SEMICOLON] = HID_KEY_SEMICOLON,
    [SAPP_KEYCODE_EQUAL] = HID_KEY_EQUAL,
    [SAPP_KEYCODE_A] = HID_KEY_A,
    [SAPP_KEYCODE_B] = HID_KEY_B,
    [SAPP_KEYCODE_C] = HID_KEY_C,
    [SAPP_KEYCODE_D] = HID_KEY_D,
    [SAPP_KEYCODE_E] = HID_KEY_E,
    [SAPP_KEYCODE_F] = HID_KEY_F,
    [SAPP_KEYCODE_G] = HID_KEY_G,
    [SAPP_KEYCODE_H] = HID_KEY_H,
    [SAPP_KEYCODE_I] = HID_KEY_I,
    [SAPP_KEYCODE_J] = HID_KEY_J,
    [SAPP_KEYCODE_K] = HID_KEY_K,
    [SAPP_KEYCODE_L] = HID_KEY_L,
    [SAPP_KEYCODE_M] = HID_KEY_M,
    [SAPP_KEYCODE_N] = HID_KEY_N,
    [SAPP_KEYCODE_O] = HID_KEY_O,
    [SAPP_KEYCODE_P] = HID_KEY_P,
    [SAPP_KEYCODE_Q] = HID_KEY_Q,
    [SAPP_KEYCODE_R] = HID_KEY_R,
    [SAPP_KEYCODE_S] = HID_KEY_S,
    [SAPP_KEYCODE_T] = HID_KEY_T,
    [SAPP_KEYCODE_U] = HID_KEY_U,
    [SAPP_KEYCODE_V] = HID_KEY_V,
    [SAPP_KEYCODE_W] = HID_KEY_W,
    [SAPP_KEYCODE_X] = HID_KEY_X,
    [SAPP_KEYCODE_Y] = HID_KEY_Y,
    [SAPP_KEYCODE_Z] = HID_KEY_Z,
    [SAPP_KEYCODE_LEFT_BRACKET] = HID_KEY_BRACKET_LEFT,
    [SAPP_KEYCODE_BACKSLASH] = HID_KEY_BACKSLASH,
    [SAPP_KEYCODE_RIGHT_BRACKET] = HID_KEY_BRACKET_RIGHT,
    [SAPP_KEYCODE_GRAVE_ACCENT] = HID_KEY_GRAVE,
    [SAPP_KEYCODE_WORLD_1] = HID_KEY_EUROPE_1,
    [SAPP_KEYCODE_WORLD_2] = HID_KEY_EUROPE_2,
    [SAPP_KEYCODE_ESCAPE] = HID_KEY_ESCAPE,
    [SAPP_KEYCODE_ENTER] = HID_KEY_ENTER,
    [SAPP_KEYCODE_TAB] = HID_KEY_TAB,
    [SAPP_KEYCODE_BACKSPACE] = HID_KEY_BACKSPACE,
    [SAPP_KEYCODE_INSERT] = HID_KEY_INSERT,
    [SAPP_KEYCODE_DELETE] = HID_KEY_DELETE,
    [SAPP_KEYCODE_RIGHT] = HID_KEY_ARROW_RIGHT,
    [SAPP_KEYCODE_LEFT] = HID_KEY_ARROW_LEFT,
    [SAPP_KEYCODE_DOWN] = HID_KEY_ARROW_DOWN,
    [SAPP_KEYCODE_UP] = HID_KEY_ARROW_UP,
    [SAPP_KEYCODE_PAGE_UP] = HID_KEY_PAGE_UP,
    [SAPP_KEYCODE_PAGE_DOWN] = HID_KEY_PAGE_DOWN,
    [SAPP_KEYCODE_HOME] = HID_KEY_HOME,
    [SAPP_KEYCODE_END] = HID_KEY_END,
    [SAPP_KEYCODE_CAPS_LOCK] = HID_KEY_CAPS_LOCK,
    [SAPP_KEYCODE_SCROLL_LOCK] = HID_KEY_SCROLL_LOCK,
    [SAPP_KEYCODE_NUM_LOCK] = HID_KEY_NUM_LOCK,
    [SAPP_KEYCODE_PRINT_SCREEN] = HID_KEY_PRINT_SCREEN,
    [SAPP_KEYCODE_PAUSE] = HID_KEY_PAUSE,
    [SAPP_KEYCODE_F1] = HID_KEY_F1,
    [SAPP_KEYCODE_F2] = HID_KEY_F2,
    [SAPP_KEYCODE_F3] = HID_KEY_F3,
    [SAPP_KEYCODE_F4] = HID_KEY_F4,
    [SAPP_KEYCODE_F5] = HID_KEY_F5,
    [SAPP_KEYCODE_F6] = HID_KEY_F6,
    [SAPP_KEYCODE_F7] = HID_KEY_F7,
    [SAPP_KEYCODE_F8] = HID_KEY_F8,
    [SAPP_KEYCODE_F9] = HID_KEY_F9,
    [SAPP_KEYCODE_F10] = HID_KEY_F10,
    [SAPP_KEYCODE_F11] = HID_KEY_F11,
    [SAPP_KEYCODE_F12] = HID_KEY_F12,
    [SAPP_KEYCODE_F13] = HID_KEY_F13,
    [SAPP_KEYCODE_F14] = HID_KEY_F14,
    [SAPP_KEYCODE_F15] = HID_KEY_F15,
    [SAPP_KEYCODE_F16] = HID_KEY_F16,
    [SAPP_KEYCODE_F17] = HID_KEY_F17,
    [SAPP_KEYCODE_F18] = HID_KEY_F18,
    [SAPP_KEYCODE_F19] = HID_KEY_F19,
    [SAPP_KEYCODE_F20] = HID_KEY_F20,
    [SAPP_KEYCODE_F21] = HID_KEY_F21,
    [SAPP_KEYCODE_F22] = HID_KEY_F22,
    [SAPP_KEYCODE_F23] = HID_KEY_F23,
    [SAPP_KEYCODE_F24] = HID_KEY_F24,
    [SAPP_KEYCODE_KP_0] = HID_KEY_KEYPAD_0,
    [SAPP_KEYCODE_KP_1] = HID_KEY_KEYPAD_1,
    [SAPP_KEYCODE_KP_2] = HID_KEY_KEYPAD_2,
    [SAPP_KEYCODE_KP_3] = HID_KEY_KEYPAD_3,
    [SAPP_KEYCODE_KP_4] = HID_KEY_KEYPAD_4,
    [SAPP_KEYCODE_KP_5] = HID_KEY_KEYPAD_5,
    [SAPP_KEYCODE_KP_6] = HID_KEY_KEYPAD_6,
    [SAPP_KEYCODE_KP_7] = HID_KEY_KEYPAD_7,
    [SAPP_KEYCODE_KP_8] = HID_KEY_KEYPAD_8,
    [SAPP_KEYCODE_KP_9] = HID_KEY_KEYPAD_9,
    [SAPP_KEYCODE_KP_DECIMAL] = HID_KEY_KEYPAD_DECIMAL,
    [SAPP_KEYCODE_KP_DIVIDE] = HID_KEY_KEYPAD_DIVIDE,
    [SAPP_KEYCODE_KP_MULTIPLY] = HID_KEY_KEYPAD_MULTIPLY,
    [SAPP_KEYCODE_KP_SUBTRACT] = HID_KEY_KEYPAD_SUBTRACT,
    [SAPP_KEYCODE_KP_ADD] = HID_KEY_KEYPAD_ADD,
    [SAPP_KEYCODE_KP_ENTER] = HID_KEY_KEYPAD_ENTER,
    [SAPP_KEYCODE_KP_EQUAL] = HID_KEY_KEYPAD_EQUAL,
    [SAPP_KEYCODE_LEFT_SHIFT] = HID_KEY_SHIFT_LEFT,
    [SAPP_KEYCODE_LEFT_CONTROL] = HID_KEY_CONTROL_LEFT,
    [SAPP_KEYCODE_LEFT_ALT] = HID_KEY_ALT_LEFT,
    [SAPP_KEYCODE_LEFT_SUPER] = HID_KEY_GUI_LEFT,
    [SAPP_KEYCODE_RIGHT_SHIFT] = HID_KEY_SHIFT_RIGHT,
    [SAPP_KEYCODE_RIGHT_CONTROL] = HID_KEY_CONTROL_RIGHT,
    [SAPP_KEYCODE_RIGHT_ALT] = HID_KEY_ALT_RIGHT,
    [SAPP_KEYCODE_RIGHT_SUPER] = HID_KEY_GUI_RIGHT,
    [SAPP_KEYCODE_MENU] = HID_KEY_MENU,
};

#define KBD_KEY_BIT_SET(data, keycode) (data[keycode >> 5] |= 1 << (keycode & 31))
#define KBD_KEY_BIT_RES(data, keycode) (data[keycode >> 5] &= ~(1 << (keycode & 31)))
#define KBD_KEY_BIT_VAL(data, keycode) (data[keycode >> 5] & (1 << (keycode & 31)))

static uint32_t kbd_keys[8] = { 0 };

void hid_key_down(sapp_keycode key_code) {
    if (key_code) {
        uint8_t usb_keycode = sokol2usb[key_code];
        if (usb_keycode) {
            KBD_KEY_BIT_SET(kbd_keys, usb_keycode);
        }
    }
    kbd_report(1, (void*)kbd_keys, 0);
}

void hid_key_up(sapp_keycode key_code) {
    if (key_code) {
        uint8_t usb_keycode = sokol2usb[key_code];
        if (usb_keycode) {
            KBD_KEY_BIT_RES(kbd_keys, usb_keycode);
        }
    }
    kbd_report(1, (void*)kbd_keys, 0);
}
