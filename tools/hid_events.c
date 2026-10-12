// hid_events: print HID event system events from the macgip gamepad, by event type.
// Uses the IOHIDEventSystemClient simple-client API (private, exported by IOKit).
// Build: clang -o hid_events hid_events.c -framework IOKit -framework CoreFoundation
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct __IOHIDEventSystemClient *IOHIDEventSystemClientRef;
typedef struct __IOHIDEvent *IOHIDEventRef;
typedef struct __IOHIDServiceClient *IOHIDServiceClientRef;
typedef void (*IOHIDEventSystemClientEventCallback)(void *target, void *refcon, IOHIDServiceClientRef sender, IOHIDEventRef event);
extern IOHIDEventSystemClientRef IOHIDEventSystemClientCreateSimpleClient(CFAllocatorRef);
extern void IOHIDEventSystemClientSetMatching(IOHIDEventSystemClientRef, CFDictionaryRef);
extern void IOHIDEventSystemClientRegisterEventCallback(IOHIDEventSystemClientRef, IOHIDEventSystemClientEventCallback, void *, void *);
extern void IOHIDEventSystemClientScheduleWithRunLoop(IOHIDEventSystemClientRef, CFRunLoopRef, CFStringRef);
extern uint32_t IOHIDEventGetType(IOHIDEventRef);
extern CFIndex IOHIDEventGetIntegerValue(IOHIDEventRef, uint32_t field);
extern double IOHIDEventGetFloatValue(IOHIDEventRef, uint32_t field);
extern CFArrayRef IOHIDEventGetChildren(IOHIDEventRef);

static int counts[64];
static void cb(void *t, void *r, IOHIDServiceClientRef s, IOHIDEventRef e)
{
    uint32_t type = IOHIDEventGetType(e);
    if (type < 64) counts[type]++;
    static int printed = 0;
    if (printed < 40) {
        printed++;
        // Field ids: (type << 16) | index. Print the first four integer fields and float fields.
        printf("event type %u:", type);
        for (int i = 0; i < 6; i++) printf(" i%d=%ld", i, (long)IOHIDEventGetIntegerValue(e, (type << 16) | i));
        for (int i = 0; i < 6; i++) printf(" f%d=%.2f", i, IOHIDEventGetFloatValue(e, (type << 16) | i));
        CFArrayRef ch = IOHIDEventGetChildren(e);
        if (ch) printf(" children=%ld", (long)CFArrayGetCount(ch));
        printf("\n"); fflush(stdout);
    }
}
int main(int argc, char **argv)
{
    double secs = argc > 1 ? atof(argv[1]) : 15;
    IOHIDEventSystemClientRef c = IOHIDEventSystemClientCreateSimpleClient(kCFAllocatorDefault);
    if (!c) { puts("no client"); return 1; }
    int vid = 0x20d6; CFNumberRef v = CFNumberCreate(0, kCFNumberIntType, &vid);
    const void *k[] = { CFSTR("VendorID") }, *val[] = { v };
    IOHIDEventSystemClientSetMatching(c, CFDictionaryCreate(0, k, val, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
    IOHIDEventSystemClientRegisterEventCallback(c, cb, NULL, NULL);
    IOHIDEventSystemClientScheduleWithRunLoop(c, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, secs, false);
    printf("--- event counts by type (3=keyboard, 36=game controller):\n");
    for (int i = 0; i < 64; i++) if (counts[i]) printf("  type %d: %d\n", i, counts[i]);
    return 0;
}
