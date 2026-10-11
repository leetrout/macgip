// sdl_test: does SDL3 see the pad? Run with:
//   SDL_HIDAPI_LIBUSB=1 SDL_HIDAPI_LIBUSB_WHITELIST=0 SDL_JOYSTICK_HIDAPI_GIP=1 ./sdl_test [seconds]
// Build: clang -o sdl_test sdl_test.c $(pkg-config --cflags --libs sdl3)
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    double secs = argc > 1 ? atof(argv[1]) : 8;
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
    if (!SDL_Init(SDL_INIT_GAMEPAD)) { printf("SDL_Init failed: %s\n", SDL_GetError()); return 1; }
    printf("SDL %d\n", SDL_GetVersion());
    // Raw HID layer view: what does SDL's hidapi (IOKit and libusb backends) enumerate?
    for (SDL_hid_device_info *d = SDL_hid_enumerate(0, 0), *first = d; d; d = d->next) {
        printf(" hid: %04x:%04x bus=%d iface=%d class=%d/%d/%d usage=%04x/%04x path=%s product=%ls\n", d->vendor_id, d->product_id,
               d->bus_type, d->interface_number, d->interface_class, d->interface_subclass, d->interface_protocol,
               d->usage_page, d->usage, d->path, d->product_string ? d->product_string : L"");
        if (!d->next) { SDL_hid_free_enumeration(first); break; }
    }
    int n = 0; SDL_JoystickID *ids = SDL_GetJoysticks(&n);
    printf("joysticks: %d\n", n);
    SDL_Gamepad *gp = NULL;
    for (int i = 0; i < n; i++) {
        printf(" [%u] name=\"%s\" vid=0x%04x pid=0x%04x type=%d gamepad=%d path=%s\n", ids[i],
               SDL_GetJoystickNameForID(ids[i]), SDL_GetJoystickVendorForID(ids[i]), SDL_GetJoystickProductForID(ids[i]),
               SDL_GetJoystickTypeForID(ids[i]), SDL_IsGamepad(ids[i]), SDL_GetJoystickPathForID(ids[i]));
        if (!gp && SDL_IsGamepad(ids[i])) { gp = SDL_OpenGamepad(ids[i]); printf("  opened gamepad: %s (err: %s)\n", gp ? SDL_GetGamepadName(gp) : "NULL", SDL_GetError()); }
    }
    SDL_free(ids);
    Uint64 end = SDL_GetTicks() + (Uint64)(secs * 1000);
    SDL_Event e;
    while (SDL_GetTicks() < end) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_GAMEPAD_ADDED) { printf("gamepad added id=%u\n", e.gdevice.which); if (!gp) { gp = SDL_OpenGamepad(e.gdevice.which); printf("  opened: %s\n", gp ? SDL_GetGamepadName(gp) : SDL_GetError()); } }
            else if (e.type == SDL_EVENT_GAMEPAD_REMOVED) printf("gamepad removed id=%u\n", e.gdevice.which);
            else if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || e.type == SDL_EVENT_GAMEPAD_BUTTON_UP) printf("button %s %s\n", SDL_GetGamepadStringForButton(e.gbutton.button), e.gbutton.down ? "down" : "up");
            else if (e.type == SDL_EVENT_GAMEPAD_AXIS_MOTION) printf("axis %s = %d\n", SDL_GetGamepadStringForAxis(e.gaxis.axis), e.gaxis.value);
        }
        SDL_Delay(10);
    }
    if (gp) { printf("rumble test (0.5 s low): %s\n", SDL_RumbleGamepad(gp, 0x4000, 0x4000, 500) ? "ok" : SDL_GetError()); SDL_Delay(700); SDL_CloseGamepad(gp); }
    SDL_Quit();
    return gp ? 0 : 2;
}
