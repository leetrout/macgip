#include <os/log.h>
#include <DriverKit/IOLib.h>
#include <USBDriverKit/IOUSBHostDevice.h>
#include "MacgipUSBDevice.h"

#define LOG(fmt, ...) os_log(OS_LOG_DEFAULT, "macgip usb: " fmt, ##__VA_ARGS__)

kern_return_t IMPL(MacgipUSBDevice, Start)
{
    kern_return_t ret = Start(provider, SUPERDISPATCH);
    if (ret != kIOReturnSuccess) {
        return ret;
    }
    IOUSBHostDevice *device = OSDynamicCast(IOUSBHostDevice, provider);
    if (!device) {
        LOG("provider is not an IOUSBHostDevice");
        Stop(provider, SUPERDISPATCH);
        return kIOReturnBadArgument;
    }
    ret = device->Open(this, 0, 0);
    if (ret != kIOReturnSuccess) {
        LOG("Open failed: 0x%x", ret);
        Stop(provider, SUPERDISPATCH);
        return ret;
    }
    // The pad has one configuration. Selecting it publishes the three GIP interfaces.
    ret = device->SetConfiguration(1, true);
    if (ret != kIOReturnSuccess) {
        LOG("SetConfiguration(1) failed: 0x%x", ret);
        device->Close(this, 0);
        Stop(provider, SUPERDISPATCH);
        return ret;
    }
    LOG("configuration 1 selected");
    RegisterService();
    return kIOReturnSuccess;
}

kern_return_t IMPL(MacgipUSBDevice, Stop)
{
    IOUSBHostDevice *device = OSDynamicCast(IOUSBHostDevice, provider);
    if (device) {
        device->Close(this, 0);
    }
    LOG("stopped");
    return Stop(provider, SUPERDISPATCH);
}
