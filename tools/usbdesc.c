// Dump device + configuration descriptors for a USB device via IOUSBLib (no root, no exclusive open).
#include <stdio.h>
#include <stdlib.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>
#include <CoreFoundation/CoreFoundation.h>

static void hexdump(const uint8_t *p, size_t n) {
    for (size_t i = 0; i < n; i++) { printf("%02x%s", p[i], (i % 16 == 15 || i + 1 == n) ? "\n" : " "); }
}

int main(int argc, char **argv) {
    int vid = (int)strtol(argv[1], 0, 0), pid = (int)strtol(argv[2], 0, 0);
    CFMutableDictionaryRef m = IOServiceMatching(kIOUSBDeviceClassName);
    CFNumberRef v = CFNumberCreate(0, kCFNumberIntType, &vid), p = CFNumberCreate(0, kCFNumberIntType, &pid);
    CFDictionarySetValue(m, CFSTR(kUSBVendorID), v); CFDictionarySetValue(m, CFSTR(kUSBProductID), p);
    io_iterator_t it; if (IOServiceGetMatchingServices(kIOMainPortDefault, m, &it) != KERN_SUCCESS) { puts("match failed"); return 1; }
    io_service_t svc; int found = 0;
    while ((svc = IOIteratorNext(it))) {
        found = 1;
        IOCFPlugInInterface **plug = 0; SInt32 score;
        kern_return_t kr = IOCreatePlugInInterfaceForService(svc, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &plug, &score);
        if (kr) { printf("plugin kr=0x%x\n", kr); continue; }
        IOUSBDeviceInterface650 **dev = 0;
        (*plug)->QueryInterface(plug, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID650), (LPVOID *)&dev);
        (*plug)->Release(plug);
        UInt8 ncfg = 0; (*dev)->GetNumberOfConfigurations(dev, &ncfg);
        UInt8 cur = 0; kr = (*dev)->GetConfiguration(dev, &cur);
        printf("numConfigurations=%u currentConfiguration=%u (kr=0x%x)\n", ncfg, cur, kr);
        UInt16 spd=0; (*dev)->GetDeviceSpeed(dev, (UInt8*)&spd); printf("speed code=%u\n", spd & 0xff);
        for (UInt8 i = 0; i < ncfg; i++) {
            IOUSBConfigurationDescriptorPtr cd = 0;
            kr = (*dev)->GetConfigurationDescriptorPtr(dev, i, &cd);
            if (kr || !cd) { printf("cfg %u: kr=0x%x\n", i, kr); continue; }
            UInt16 total = USBToHostWord(cd->wTotalLength);
            printf("--- configuration descriptor %u: wTotalLength=%u bNumInterfaces=%u bConfigurationValue=%u bmAttributes=0x%02x bMaxPower=%u (x2mA)\n",
                   i, total, cd->bNumInterfaces, cd->bConfigurationValue, cd->bmAttributes, cd->MaxPower);
            hexdump((const uint8_t *)cd, total);
            // walk descriptors
            const uint8_t *q = (const uint8_t *)cd, *end = q + total;
            while (q + 2 <= end && q[0] >= 2) {
                uint8_t len = q[0], type = q[1];
                if (type == 4 && len >= 9) printf("  INTERFACE bInterfaceNumber=%u bAlternateSetting=%u bNumEndpoints=%u class=0x%02x subclass=0x%02x protocol=0x%02x iInterface=%u\n", q[2], q[3], q[4], q[5], q[6], q[7], q[8]);
                else if (type == 5 && len >= 7) printf("    ENDPOINT addr=0x%02x (%s %u) attr=0x%02x (%s) wMaxPacketSize=%u bInterval=%u\n", q[2], (q[2]&0x80)?"IN":"OUT", q[2]&0xf, q[3], (const char*[]){"control","iso","bulk","interrupt"}[q[3]&3], q[4] | (q[5]<<8), q[6]);
                else if (type != 2) printf("  descriptor type=0x%02x len=%u\n", type, len);
                q += len;
            }
        }
        (*dev)->Release(dev);
        IOObjectRelease(svc);
    }
    if (!found) puts("device not found");
    return 0;
}
