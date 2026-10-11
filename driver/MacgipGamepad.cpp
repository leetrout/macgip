// MacgipGamepad: GIP interface 0 as a HID gamepad.
//
// Flow: handleStart opens the interface, sends power-on and LED, and arms one interrupt IN
// read. ReadComplete parses the packet with the gip core, turns input into a HID report, and
// re-arms the read at once. The pad resets itself if the host stops reading, so the read is
// always pending. setReport turns the rumble output report into a GIP rumble packet.
#include <os/log.h>
#include <string.h>
#include <DriverKit/IOLib.h>
#include <DriverKit/IOBufferMemoryDescriptor.h>
#include <DriverKit/IOMemoryMap.h>
#include <DriverKit/OSCollections.h>
#include <USBDriverKit/USBDriverKit.h>
#include <USBDriverKit/IOUSBHostInterface.h>
#include <USBDriverKit/IOUSBHostPipe.h>
#include <HIDDriverKit/IOHIDDeviceKeys.h>
#include <HIDDriverKit/IOHIDUsageTables.h>
#include "gip.h"
#include "MacgipGamepad.h"

#define LOG(fmt, ...) os_log(OS_LOG_DEFAULT, "macgip hid: " fmt, ##__VA_ARGS__)

static const uint8_t kEndpointIn = 0x82;
static const uint8_t kEndpointOut = 0x02;
static const uint32_t kPacketSize = 64;
static const uint8_t kInputReportID = 1;
static const uint8_t kRumbleReportID = 2;
static const uint32_t kInputReportSize = 16;   // report ID + 15 data bytes
static const uint8_t kGuideLEDBrightness = 0x14;

// HID report descriptor. Own layout, not copied from any vendor's descriptor. Axes and triggers
// use the usages the Game Controller framework expects for a gamepad: X/Y and Z/Rz for the
// sticks, Brake/Accelerator for the triggers, a hat switch for the d-pad, 10 buttons, and
// Consumer AC Home for the guide button. Report 2 is a 4-byte rumble output report.
static const uint8_t kReportDescriptor[] = {
    0x05, 0x01,                   // Usage Page (Generic Desktop)
    0x09, 0x05,                   // Usage (Game Pad)
    0xA1, 0x01,                   // Collection (Application)
    0x85, kInputReportID,         //   Report ID (1)
    0x09, 0x01,                   //   Usage (Pointer)
    0xA1, 0x00,                   //   Collection (Physical)
    0x09, 0x30,                   //     Usage (X)
    0x09, 0x31,                   //     Usage (Y)
    0x15, 0x00,                   //     Logical Minimum (0)
    0x27, 0xFF, 0xFF, 0x00, 0x00, //     Logical Maximum (65535)
    0x95, 0x02,                   //     Report Count (2)
    0x75, 0x10,                   //     Report Size (16)
    0x81, 0x02,                   //     Input (Data, Var, Abs)
    0xC0,                         //   End Collection
    0x09, 0x01,                   //   Usage (Pointer)
    0xA1, 0x00,                   //   Collection (Physical)
    0x09, 0x32,                   //     Usage (Z)
    0x09, 0x35,                   //     Usage (Rz)
    0x15, 0x00,                   //     Logical Minimum (0)
    0x27, 0xFF, 0xFF, 0x00, 0x00, //     Logical Maximum (65535)
    0x95, 0x02,                   //     Report Count (2)
    0x75, 0x10,                   //     Report Size (16)
    0x81, 0x02,                   //     Input (Data, Var, Abs)
    0xC0,                         //   End Collection
    0x05, 0x02,                   //   Usage Page (Simulation Controls)
    0x09, 0xC5,                   //   Usage (Brake)
    0x15, 0x00,                   //   Logical Minimum (0)
    0x26, 0xFF, 0x03,             //   Logical Maximum (1023)
    0x95, 0x01,                   //   Report Count (1)
    0x75, 0x10,                   //   Report Size (16)
    0x81, 0x02,                   //   Input (Data, Var, Abs)
    0x09, 0xC4,                   //   Usage (Accelerator)
    0x15, 0x00,                   //   Logical Minimum (0)
    0x26, 0xFF, 0x03,             //   Logical Maximum (1023)
    0x95, 0x01,                   //   Report Count (1)
    0x75, 0x10,                   //   Report Size (16)
    0x81, 0x02,                   //   Input (Data, Var, Abs)
    0x05, 0x01,                   //   Usage Page (Generic Desktop)
    0x09, 0x39,                   //   Usage (Hat switch)
    0x15, 0x01,                   //   Logical Minimum (1)
    0x25, 0x08,                   //   Logical Maximum (8)
    0x35, 0x00,                   //   Physical Minimum (0)
    0x46, 0x3B, 0x01,             //   Physical Maximum (315)
    0x66, 0x14, 0x00,             //   Unit (degrees)
    0x75, 0x04,                   //   Report Size (4)
    0x95, 0x01,                   //   Report Count (1)
    0x81, 0x42,                   //   Input (Data, Var, Abs, Null State)
    0x75, 0x04,                   //   Report Size (4)
    0x95, 0x01,                   //   Report Count (1)
    0x15, 0x00,                   //   Logical Minimum (0)
    0x25, 0x00,                   //   Logical Maximum (0)
    0x35, 0x00,                   //   Physical Minimum (0)
    0x45, 0x00,                   //   Physical Maximum (0)
    0x65, 0x00,                   //   Unit (none)
    0x81, 0x03,                   //   Input (Const) padding
    0x05, 0x09,                   //   Usage Page (Button)
    0x19, 0x01,                   //   Usage Minimum (1)
    0x29, 0x0A,                   //   Usage Maximum (10)
    0x15, 0x00,                   //   Logical Minimum (0)
    0x25, 0x01,                   //   Logical Maximum (1)
    0x75, 0x01,                   //   Report Size (1)
    0x95, 0x0A,                   //   Report Count (10)
    0x81, 0x02,                   //   Input (Data, Var, Abs)
    0x05, 0x0C,                   //   Usage Page (Consumer)
    0x0A, 0x23, 0x02,             //   Usage (AC Home)
    0x15, 0x00,                   //   Logical Minimum (0)
    0x25, 0x01,                   //   Logical Maximum (1)
    0x75, 0x01,                   //   Report Size (1)
    0x95, 0x01,                   //   Report Count (1)
    0x81, 0x02,                   //   Input (Data, Var, Abs)
    0x75, 0x01,                   //   Report Size (1)
    0x95, 0x05,                   //   Report Count (5)
    0x81, 0x03,                   //   Input (Const) padding
    0x06, 0x00, 0xFF,             //   Usage Page (Vendor 0xFF00)
    0x85, kRumbleReportID,        //   Report ID (2)
    0x09, 0x01,                   //   Usage (1)
    0x15, 0x00,                   //   Logical Minimum (0)
    0x25, 0x64,                   //   Logical Maximum (100)
    0x75, 0x08,                   //   Report Size (8)
    0x95, 0x04,                   //   Report Count (4)
    0x91, 0x02,                   //   Output (Data, Var, Abs)
    0xC0,                         // End Collection
};

struct MacgipGamepad_IVars {
    IOUSBHostInterface *interface;
    IOUSBHostPipe *inPipe;
    IOUSBHostPipe *outPipe;
    IOBufferMemoryDescriptor *inBuffer;
    IOBufferMemoryDescriptor *outBuffer;
    IOBufferMemoryDescriptor *reportBuffer;
    uint8_t *inBytes;
    uint8_t *outBytes;
    uint8_t *reportBytes;
    OSAction *readAction;
    OSAction *writeAction;
    gip_encoder encoder;
    bool writeBusy;
    bool rumblePending;
    uint8_t rumble[4];
    bool guidePressed;
    bool stopping;
};

bool MacgipGamepad::init()
{
    if (!super::init()) {
        return false;
    }
    ivars = IONewZero(MacgipGamepad_IVars, 1);
    return ivars != nullptr;
}

void MacgipGamepad::free()
{
    if (ivars) {
        OSSafeReleaseNULL(ivars->readAction);
        OSSafeReleaseNULL(ivars->writeAction);
        OSSafeReleaseNULL(ivars->inBuffer);
        OSSafeReleaseNULL(ivars->outBuffer);
        OSSafeReleaseNULL(ivars->reportBuffer);
        OSSafeReleaseNULL(ivars->inPipe);
        OSSafeReleaseNULL(ivars->outPipe);
        OSSafeReleaseNULL(ivars->interface);
    }
    IOSafeDeleteNULL(ivars, MacgipGamepad_IVars, 1);
    super::free();
}

static uint8_t *bufferBytes(IOBufferMemoryDescriptor *buffer)
{
    IOAddressSegment range = {};
    if (buffer->GetAddressRange(&range) != kIOReturnSuccess) {
        return nullptr;
    }
    return reinterpret_cast<uint8_t *>(range.address);
}

// Synchronous write used only during start, before the read loop runs. Interrupt pipes do not
// support a completion timeout, so it is always 0.
static kern_return_t writeSync(MacgipGamepad_IVars *iv, size_t length)
{
    uint32_t sent = 0;
    return iv->outPipe->IO(iv->outBuffer, static_cast<uint32_t>(length), &sent, 0);
}

bool MacgipGamepad::handleStart(IOService *provider)
{
    kern_return_t ret;
    MacgipGamepad_IVars *iv = ivars;

    iv->interface = OSDynamicCast(IOUSBHostInterface, provider);
    if (!iv->interface) {
        LOG("provider is not an IOUSBHostInterface");
        return false;
    }
    iv->interface->retain();

    ret = iv->interface->Open(this, 0, 0);
    if (ret != kIOReturnSuccess) {
        LOG("interface Open failed: 0x%x", ret);
        return false;
    }
    ret = iv->interface->CopyPipe(kEndpointIn, &iv->inPipe);
    if (ret != kIOReturnSuccess) {
        LOG("CopyPipe IN failed: 0x%x", ret);
        return false;
    }
    ret = iv->interface->CopyPipe(kEndpointOut, &iv->outPipe);
    if (ret != kIOReturnSuccess) {
        LOG("CopyPipe OUT failed: 0x%x", ret);
        return false;
    }
    if (IOBufferMemoryDescriptor::Create(kIOMemoryDirectionIn, kPacketSize, 0, &iv->inBuffer) != kIOReturnSuccess ||
        IOBufferMemoryDescriptor::Create(kIOMemoryDirectionOut, kPacketSize, 0, &iv->outBuffer) != kIOReturnSuccess ||
        IOBufferMemoryDescriptor::Create(kIOMemoryDirectionOut, kInputReportSize, 0, &iv->reportBuffer) != kIOReturnSuccess) {
        LOG("buffer allocation failed");
        return false;
    }
    iv->inBytes = bufferBytes(iv->inBuffer);
    iv->outBytes = bufferBytes(iv->outBuffer);
    iv->reportBytes = bufferBytes(iv->reportBuffer);
    if (!iv->inBytes || !iv->outBytes || !iv->reportBytes) {
        LOG("buffer mapping failed");
        return false;
    }
    iv->inBuffer->SetLength(kPacketSize);
    iv->reportBuffer->SetLength(kInputReportSize);

    if (CreateActionReadComplete(0, &iv->readAction) != kIOReturnSuccess ||
        CreateActionWriteComplete(0, &iv->writeAction) != kIOReturnSuccess) {
        LOG("action creation failed");
        return false;
    }

    // Init sequence, verified on hardware: power-on, then the guide LED.
    size_t n = gip_encode_power_on(&iv->encoder, iv->outBytes);
    iv->outBuffer->SetLength(static_cast<uint64_t>(n));
    ret = writeSync(iv, n);
    if (ret != kIOReturnSuccess) {
        LOG("power-on write failed: 0x%x", ret);
        return false;
    }
    n = gip_encode_led(&iv->encoder, iv->outBytes, kGuideLEDBrightness);
    iv->outBuffer->SetLength(static_cast<uint64_t>(n));
    ret = writeSync(iv, n);
    if (ret != kIOReturnSuccess) {
        LOG("LED write failed: 0x%x", ret);
    }

    ret = iv->inPipe->AsyncIO(iv->inBuffer, kPacketSize, iv->readAction, 0);
    if (ret != kIOReturnSuccess) {
        LOG("first AsyncIO failed: 0x%x", ret);
        return false;
    }
    LOG("started");
    return true;
}

kern_return_t IMPL(MacgipGamepad, Stop)
{
    MacgipGamepad_IVars *iv = ivars;
    iv->stopping = true;
    if (iv->inPipe) {
        iv->inPipe->Abort(kIOUSBAbortSynchronous, kIOReturnAborted, nullptr);
    }
    if (iv->outPipe) {
        iv->outPipe->Abort(kIOUSBAbortSynchronous, kIOReturnAborted, nullptr);
    }
    if (iv->interface) {
        iv->interface->Close(this, 0);
    }
    LOG("stopped");
    return Stop(provider, SUPERDISPATCH);
}

OSDictionary *MacgipGamepad::newDeviceDescription(void)
{
    OSDictionary *d = OSDictionary::withCapacity(8);
    if (!d) {
        return nullptr;
    }
    // The USB IDs pass through: this HID device is that controller. See issue #11.
    OSDictionarySetUInt64Value(d, kIOHIDVendorIDKey, 0x20d6);
    OSDictionarySetUInt64Value(d, kIOHIDProductIDKey, 0x2007);
    OSDictionarySetUInt64Value(d, kIOHIDVersionNumberKey, 0x0167);
    OSDictionarySetStringValue(d, kIOHIDTransportKey, "USB");
    OSDictionarySetStringValue(d, kIOHIDManufacturerKey, "PowerA");
    OSDictionarySetStringValue(d, kIOHIDProductKey, "Wired Controller (compatible with Xbox) via macgip");
    OSDictionarySetUInt64Value(d, kIOHIDPrimaryUsagePageKey, kHIDPage_GenericDesktop);
    OSDictionarySetUInt64Value(d, kIOHIDPrimaryUsageKey, kHIDUsage_GD_GamePad);
    OSDictionarySetUInt64Value(d, kIOHIDReportIntervalKey, 4000);
    return d;
}

OSData *MacgipGamepad::newReportDescriptor(void)
{
    return OSData::withBytes(kReportDescriptor, sizeof kReportDescriptor);
}

static uint8_t hatFromButtons(uint16_t b)
{
    bool up = b & GIP_BTN_DPAD_UP, down = b & GIP_BTN_DPAD_DOWN;
    bool left = b & GIP_BTN_DPAD_LEFT, right = b & GIP_BTN_DPAD_RIGHT;
    if (up && right) return 2;
    if (down && right) return 4;
    if (down && left) return 6;
    if (up && left) return 8;
    if (up) return 1;
    if (right) return 3;
    if (down) return 5;
    if (left) return 7;
    return 0;   // null state
}

static void put16(uint8_t *p, uint16_t v) { p[0] = static_cast<uint8_t>(v); p[1] = static_cast<uint8_t>(v >> 8); }

// Build the HID input report from the parsed GIP input and the cached guide state.
static void buildReport(uint8_t *r, const gip_input *in, bool guide)
{
    r[0] = kInputReportID;
    put16(r + 1, static_cast<uint16_t>(in->left_x + 32768));
    put16(r + 3, static_cast<uint16_t>(32767 - in->left_y));    // HID Y grows downward
    put16(r + 5, static_cast<uint16_t>(in->right_x + 32768));
    put16(r + 7, static_cast<uint16_t>(32767 - in->right_y));
    put16(r + 9, in->left_trigger);
    put16(r + 11, in->right_trigger);
    r[13] = hatFromButtons(in->buttons);
    uint16_t btn = 0;
    if (in->buttons & GIP_BTN_A) btn |= 1u << 0;
    if (in->buttons & GIP_BTN_B) btn |= 1u << 1;
    if (in->buttons & GIP_BTN_X) btn |= 1u << 2;
    if (in->buttons & GIP_BTN_Y) btn |= 1u << 3;
    if (in->buttons & GIP_BTN_LB) btn |= 1u << 4;
    if (in->buttons & GIP_BTN_RB) btn |= 1u << 5;
    if (in->buttons & GIP_BTN_VIEW) btn |= 1u << 6;
    if (in->buttons & GIP_BTN_MENU) btn |= 1u << 7;
    if (in->buttons & GIP_BTN_LS) btn |= 1u << 8;
    if (in->buttons & GIP_BTN_RS) btn |= 1u << 9;
    if (guide) btn |= 1u << 10;
    put16(r + 14, btn);
}

void IMPL(MacgipGamepad, ReadComplete)
{
    MacgipGamepad_IVars *iv = ivars;
    if (iv->stopping || status == kIOReturnAborted) {
        return;
    }
    if (status == kIOReturnSuccess && actualByteCount >= 4) {
        gip_packet p;
        if (gip_parse(iv->inBytes, actualByteCount, &p)) {
            static gip_input lastInput;   // one device per driver instance is fine for now
            bool send = false;
            if (p.kind == GIP_PKT_INPUT) {
                lastInput = p.u.input;
                send = true;
            } else if (p.kind == GIP_PKT_GUIDE) {
                iv->guidePressed = p.u.guide_pressed;
                send = true;
            }
            if (send) {
                buildReport(iv->reportBytes, &lastInput, iv->guidePressed);
                handleReport(completionTimestamp, iv->reportBuffer, kInputReportSize, kIOHIDReportTypeInput, 0);
            }
        }
    } else if (status != kIOReturnSuccess) {
        LOG("read error 0x%x", status);
    }
    kern_return_t ret = iv->inPipe->AsyncIO(iv->inBuffer, kPacketSize, iv->readAction, 0);
    if (ret != kIOReturnSuccess) {
        LOG("re-arm AsyncIO failed: 0x%x", ret);
    }
}

static void sendRumble(MacgipGamepad_IVars *iv)
{
    size_t n = gip_encode_rumble(&iv->encoder, iv->outBytes, iv->rumble[0], iv->rumble[1], iv->rumble[2], iv->rumble[3]);
    iv->outBuffer->SetLength(static_cast<uint64_t>(n));
    iv->writeBusy = true;
    iv->rumblePending = false;
    if (iv->outPipe->AsyncIO(iv->outBuffer, static_cast<uint32_t>(n), iv->writeAction, 0) != kIOReturnSuccess) {
        iv->writeBusy = false;
    }
}

void IMPL(MacgipGamepad, WriteComplete)
{
    MacgipGamepad_IVars *iv = ivars;
    iv->writeBusy = false;
    if (status != kIOReturnSuccess && status != kIOReturnAborted) {
        LOG("write error 0x%x", status);
    }
    if (iv->rumblePending && !iv->stopping) {
        sendRumble(iv);
    }
}

kern_return_t MacgipGamepad::setReport(IOMemoryDescriptor *report, IOHIDReportType reportType,
                                       IOOptionBits options, uint32_t completionTimeout, OSAction *action)
{
    MacgipGamepad_IVars *iv = ivars;
    kern_return_t ret = kIOReturnUnsupported;
    uint32_t handled = 0;
    IOMemoryMap *map = nullptr;

    if (reportType == kIOHIDReportTypeOutput && report && report->CreateMapping(0, 0, 0, 0, 0, &map) == kIOReturnSuccess && map) {
        const uint8_t *bytes = reinterpret_cast<const uint8_t *>(map->GetAddress());
        uint64_t len = map->GetLength();
        if (len >= 5 && bytes[0] == kRumbleReportID) {
            memcpy(iv->rumble, bytes + 1, 4);
            if (iv->writeBusy) {
                iv->rumblePending = true;
            } else {
                sendRumble(iv);
            }
            handled = 5;
            ret = kIOReturnSuccess;
        }
        OSSafeReleaseNULL(map);
    }
    if (action) {
        CompleteReport(action, ret, handled);
    }
    return ret;
}
