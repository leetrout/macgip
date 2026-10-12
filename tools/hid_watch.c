// hid_watch: print HID element values from the macgip gamepad through IOHIDManager.
// This is what any app sees. Build: clang -o hid_watch hid_watch.c -framework IOKit -framework CoreFoundation
#include <IOKit/hid/IOHIDManager.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <stdlib.h>

static int events = 0;
static void onValue(void *ctx, IOReturn r, void *sender, IOHIDValueRef v)
{
    IOHIDElementRef e = IOHIDValueGetElement(v);
    printf("page 0x%02x usage 0x%02x = %ld\n", IOHIDElementGetUsagePage(e), IOHIDElementGetUsage(e), (long)IOHIDValueGetIntegerValue(v));
    fflush(stdout);
    events++;
}
static void onMatch(void *ctx, IOReturn r, void *sender, IOHIDDeviceRef d)
{
    CFStringRef p = IOHIDDeviceGetProperty(d, CFSTR(kIOHIDProductKey));
    char buf[128] = "?"; if (p) CFStringGetCString(p, buf, sizeof buf, kCFStringEncodingUTF8);
    printf("matched device: %s\n", buf); fflush(stdout);
}
int main(int argc, char **argv)
{
    double secs = argc > 1 ? atof(argv[1]) : 20;
    IOHIDManagerRef m = IOHIDManagerCreate(kCFAllocatorDefault, 0);
    int vid = 0x20d6, pid = 0x2007;
    CFNumberRef v = CFNumberCreate(0, kCFNumberIntType, &vid), p = CFNumberCreate(0, kCFNumberIntType, &pid);
    const void *keys[] = { CFSTR(kIOHIDVendorIDKey), CFSTR(kIOHIDProductIDKey) }, *vals[] = { v, p };
    CFDictionaryRef match = CFDictionaryCreate(0, keys, vals, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    IOHIDManagerSetDeviceMatching(m, match);
    IOHIDManagerRegisterDeviceMatchingCallback(m, onMatch, NULL);
    IOHIDManagerRegisterInputValueCallback(m, onValue, NULL);
    IOHIDManagerScheduleWithRunLoop(m, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    IOReturn r = IOHIDManagerOpen(m, kIOHIDOptionsTypeNone);
    printf("IOHIDManagerOpen: 0x%x\n", r); fflush(stdout);
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, secs, false);
    printf("done, %d value events\n", events);
    return events ? 0 : 2;
}
