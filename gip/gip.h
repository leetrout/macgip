// gip: protocol core for wired GIP gamepads (compatible with Xbox One and Series controllers).
//
// Plain C11, no OS dependencies. Parses the packets the controller sends and encodes the
// packets the host sends. Every layout here is verified on hardware; see notes/protocol.md.
#ifndef GIP_H
#define GIP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Command byte (offset 0) of each packet.
enum {
    GIP_CMD_ANNOUNCE = 0x02,   // controller announces itself until the host answers
    GIP_CMD_STATUS = 0x03,     // power status; also a heartbeat every 20 s
    GIP_CMD_POWER = 0x05,      // host: power mode
    GIP_CMD_SECURITY = 0x06,   // host: security check result
    GIP_CMD_GUIDE = 0x07,      // guide button state
    GIP_CMD_RUMBLE = 0x09,     // host: motor control
    GIP_CMD_LED = 0x0A,        // host: guide LED
    GIP_CMD_INPUT = 0x20,      // controller input report
};

// Flags byte (offset 1).
enum {
    GIP_FLAG_SYSTEM = 0x20,      // system message
    GIP_FLAG_NEEDS_ACK = 0x10,   // the sender wants an acknowledgement
};

// Button bits in gip_input.buttons. Bits 0..7 are packet byte 4, bits 8..15 are byte 5.
enum {
    GIP_BTN_MENU = 1u << 2,
    GIP_BTN_VIEW = 1u << 3,
    GIP_BTN_A = 1u << 4,
    GIP_BTN_B = 1u << 5,
    GIP_BTN_X = 1u << 6,
    GIP_BTN_Y = 1u << 7,
    GIP_BTN_DPAD_UP = 1u << 8,
    GIP_BTN_DPAD_DOWN = 1u << 9,
    GIP_BTN_DPAD_LEFT = 1u << 10,
    GIP_BTN_DPAD_RIGHT = 1u << 11,
    GIP_BTN_LB = 1u << 12,
    GIP_BTN_RB = 1u << 13,
    GIP_BTN_LS = 1u << 14,   // left stick click
    GIP_BTN_RS = 1u << 15,   // right stick click
};

#define GIP_MAX_PACKET 64
#define GIP_TRIGGER_MAX 1023

typedef enum {
    GIP_PKT_UNKNOWN = 0,
    GIP_PKT_ANNOUNCE,
    GIP_PKT_STATUS,
    GIP_PKT_INPUT,
    GIP_PKT_GUIDE,
} gip_packet_kind;

typedef struct {
    uint16_t buttons;          // GIP_BTN_* bits
    uint16_t left_trigger;     // 0..1023
    uint16_t right_trigger;    // 0..1023
    int16_t left_x, left_y;    // -32768..32767, Y positive is up
    int16_t right_x, right_y;
} gip_input;

typedef struct {
    uint8_t address[8];        // controller address or serial, as sent
    uint16_t vendor_id;
    uint16_t product_id;
} gip_announce;

typedef struct {
    gip_packet_kind kind;
    uint8_t command;           // raw byte 0
    uint8_t flags;             // raw byte 1
    uint8_t sequence;          // raw byte 2
    uint8_t length;            // raw byte 3, payload length
    union {
        gip_input input;
        gip_announce announce;
        bool guide_pressed;
        uint8_t status;        // raw byte 4 of a status packet
    } u;
} gip_packet;

// Parse one packet from the interrupt IN endpoint. Returns false if the buffer is too short
// for its command. An unknown command returns true with kind GIP_PKT_UNKNOWN.
bool gip_parse(const uint8_t *buf, size_t len, gip_packet *out);

// Sequence counter for outgoing packets. Zero-initialise it.
typedef struct {
    uint8_t sequence;
} gip_encoder;

// Each encoder writes one packet into out (at least GIP_MAX_PACKET bytes) and returns its
// length. Byte 2 is the next sequence number; the counter skips 0 after wrapping.
size_t gip_encode_power_on(gip_encoder *enc, uint8_t *out);
size_t gip_encode_led(gip_encoder *enc, uint8_t *out, uint8_t brightness);
size_t gip_encode_security_passed(gip_encoder *enc, uint8_t *out);
// Motor strengths are 0..100. Values above 100 are clamped.
size_t gip_encode_rumble(gip_encoder *enc, uint8_t *out, uint8_t left_trigger, uint8_t right_trigger,
                         uint8_t low_frequency, uint8_t high_frequency);
size_t gip_encode_rumble_stop(gip_encoder *enc, uint8_t *out);

#ifdef __cplusplus
}
#endif

#endif
