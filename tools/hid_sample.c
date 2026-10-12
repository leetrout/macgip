// hid_sample: print the current value of every input element of the macgip gamepad once per
// second. No timing needed: hold a control and watch its value. Build like hid_watch.c.
#include <IOKit/hid/IOHIDManager.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    int secs = argc > 1 ? atoi(argv[1]) : 15;
    IOHIDManagerRef m = IOHIDManagerCreate(kCFAllocatorDefault, 0);
    int vid = 0x20d6, pid = 0x2007;
    CFNumberRef v = CFNumberCreate(0, kCFNumberIntType, &vid), p = CFNumberCreate(0, kCFNumberIntType, &pid);
    const void *keys[] = { CFSTR(kIOHIDVendorIDKey), CFSTR(kIOHIDProductIDKey) }, *vals[] = { v, p };
    CFDictionaryRef match = CFDictionaryCreate(0, keys, vals, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    IOHIDManagerSetDeviceMatching(m, match);
    IOHIDManagerScheduleWithRunLoop(m, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    if (IOHIDManagerOpen(m, kIOHIDOptionsTypeNone)) { puts("open failed"); return 1; }
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.5, false);
    CFSetRef devs = IOHIDManagerCopyDevices(m);
    if (!devs || CFSetGetCount(devs) == 0) { puts("no device"); return 2; }
    IOHIDDeviceRef d; CFSetGetValues(devs, (const void **)&d);
    CFArrayRef els = IOHIDDeviceCopyMatchingElements(d, NULL, 0);
    for (int t = 0; t < secs; t++) {
        printf("t=%2d:", t);
        for (CFIndex i = 0; i < CFArrayGetCount(els); i++) {
            IOHIDElementRef e = (IOHIDElementRef)CFArrayGetValueAtIndex(els, i);
            if (IOHIDElementGetType(e) > kIOHIDElementTypeInput_Button) continue;   // inputs only
            IOHIDValueRef val = NULL;
            if (IOHIDDeviceGetValue(d, e, &val) == kIOReturnSuccess && val)
                printf(" %02x/%02x=%ld", IOHIDElementGetUsagePage(e), IOHIDElementGetUsage(e), (long)IOHIDValueGetIntegerValue(val));
        }
        printf("\n"); fflush(stdout);
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    }
    return 0;
}
