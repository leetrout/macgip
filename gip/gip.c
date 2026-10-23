// gip: protocol core implementation. See gip.h and notes/protocol.md.
#include "gip.h"

#include <string.h>

static uint16_t rd_u16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static int16_t rd_s16(const uint8_t *p) { return (int16_t)rd_u16(p); }

bool gip_parse(const uint8_t *buf, size_t len, gip_packet *out)
{
    if (len < 4 || !out) {
        return false;
    }
    memset(out, 0, sizeof *out);
    out->command = buf[0];
    out->flags = buf[1];
    out->sequence = buf[2];
    out->length = buf[3];

    switch (buf[0]) {
    case GIP_CMD_INPUT:
        // 4-byte header, 2 bytes buttons, 2 x u16 triggers, 4 x s16 sticks. Payload lengths
        // of 14 (first-party), 32 (the PowerA unit, zero padded) and 46 (the PDP Afterglow
        // Wave) are seen.
        if (len < 18) {
            return false;
        }
        out->kind = GIP_PKT_INPUT;
        out->u.input.buttons = (uint16_t)(buf[4] | (buf[5] << 8));
        out->u.input.left_trigger = rd_u16(buf + 6);
        out->u.input.right_trigger = rd_u16(buf + 8);
        out->u.input.left_x = rd_s16(buf + 10);
        out->u.input.left_y = rd_s16(buf + 12);
        out->u.input.right_x = rd_s16(buf + 14);
        out->u.input.right_y = rd_s16(buf + 16);
        // A report of 50 bytes or more has the share button in bit 0 of byte len - 18. SDL and
        // xpad agree on this position for these lengths. Shorter reports use other positions.
        if (len >= 50) {
            out->u.input.share = (buf[len - 18] & 0x01) != 0;
        }
        return true;

    case GIP_CMD_GUIDE:
        if (len < 5) {
            return false;
        }
        out->kind = GIP_PKT_GUIDE;
        out->u.guide_pressed = (buf[4] & 0x01) != 0;
        return true;

    case GIP_CMD_ANNOUNCE:
        if (len < 16) {
            return false;
        }
        out->kind = GIP_PKT_ANNOUNCE;
        memcpy(out->u.announce.address, buf + 4, 8);
        out->u.announce.vendor_id = rd_u16(buf + 12);
        out->u.announce.product_id = rd_u16(buf + 14);
        return true;

    case GIP_CMD_STATUS:
        if (len < 5) {
            return false;
        }
        out->kind = GIP_PKT_STATUS;
        out->u.status = buf[4];
        return true;

    default:
        out->kind = GIP_PKT_UNKNOWN;
        return true;
    }
}

static uint8_t next_sequence(gip_encoder *enc)
{
    enc->sequence++;
    if (enc->sequence == 0) {
        enc->sequence = 1;
    }
    return enc->sequence;
}

static size_t emit(gip_encoder *enc, uint8_t *out, const uint8_t *tmpl, size_t n)
{
    memcpy(out, tmpl, n);
    out[2] = next_sequence(enc);
    return n;
}

size_t gip_encode_power_on(gip_encoder *enc, uint8_t *out)
{
    static const uint8_t t[] = { GIP_CMD_POWER, GIP_FLAG_SYSTEM, 0x00, 0x01, 0x00 };
    return emit(enc, out, t, sizeof t);
}

size_t gip_encode_led(gip_encoder *enc, uint8_t *out, uint8_t brightness)
{
    static const uint8_t t[] = { GIP_CMD_LED, GIP_FLAG_SYSTEM, 0x00, 0x03, 0x00, 0x01, 0x14 };
    size_t n = emit(enc, out, t, sizeof t);
    out[6] = brightness;
    return n;
}

size_t gip_encode_security_passed(gip_encoder *enc, uint8_t *out)
{
    static const uint8_t t[] = { GIP_CMD_SECURITY, GIP_FLAG_SYSTEM, 0x00, 0x02, 0x01, 0x00 };
    return emit(enc, out, t, sizeof t);
}

static uint8_t clamp100(uint8_t v) { return v > 100 ? 100 : v; }

size_t gip_encode_rumble(gip_encoder *enc, uint8_t *out, uint8_t left_trigger, uint8_t right_trigger,
                         uint8_t low_frequency, uint8_t high_frequency)
{
    // Layout from SDL's wired Xbox One driver (zlib). Byte 5 enables all four motors.
    // Bytes 6..9: left trigger, right trigger, low-frequency, high-frequency, 0..100.
    // Byte 10 is the duration, 0xFF for "until changed"; bytes 11..12 delay and repeat.
    static const uint8_t t[] = { GIP_CMD_RUMBLE, 0x00, 0x00, 0x09, 0x00, 0x0F,
                                 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0xEB };
    size_t n = emit(enc, out, t, sizeof t);
    out[6] = clamp100(left_trigger);
    out[7] = clamp100(right_trigger);
    out[8] = clamp100(low_frequency);
    out[9] = clamp100(high_frequency);
    return n;
}

size_t gip_encode_rumble_stop(gip_encoder *enc, uint8_t *out)
{
    // The PowerA "rumble end" packet from xpad: all strengths zero, duration zero.
    static const uint8_t t[] = { GIP_CMD_RUMBLE, 0x00, 0x00, 0x09, 0x00, 0x0F,
                                 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    return emit(enc, out, t, sizeof t);
}
