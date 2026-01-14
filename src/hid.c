#include <SDL3/SDL.h>

#include "./hid.h"
#include "./log.h"

#include "hid/pad.h"

void sdl_init() {
    SDL_Init(SDL_INIT_GAMEPAD);
}

void sdl_shutdown() {
    SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
}

// SDL3 event handling
void sdl_poll_events(void) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
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
