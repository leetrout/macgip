// hid_rumble: send the macgip rumble output report (ID 2: left trigger, right trigger, low, high;
// 0..100) through IOHIDDeviceSetReport. Build like hid_watch.c. Usage: hid_rumble lt rt low high
#include <IOKit/hid/IOHIDManager.h>
#include <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    uint8_t report[5] = { 2, 0, 0, 0, 0 };
    for (int i = 1; i < argc && i < 5; i++) report[i] = (uint8_t)atoi(argv[i]);
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
    IOReturn r = IOHIDDeviceSetReport(d, kIOHIDReportTypeOutput, 2, report, sizeof report);
    printf("SetReport(%u,%u,%u,%u): 0x%x\n", report[1], report[2], report[3], report[4], r);
    return r ? 3 : 0;
}
