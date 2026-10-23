// Tests for the gip protocol core. Fixed vectors come from notes/protocol.md. The replay test
// reads a probe capture, parses every IN packet, and compares the result with the decoded
// INPUT line that gip_probe printed under it.
#include "gip.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;
static int replayed_total = 0;
static int checks = 0;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static size_t from_hex(const char *hex, uint8_t *out, size_t max)
{
    size_t n = 0;
    while (*hex && n < max) {
        while (*hex == ' ') hex++;
        if (!*hex) break;
        char *end;
        unsigned long v = strtoul(hex, &end, 16);
        if (end == hex) break;   // no hex digit, for example the newline at the end of a line
        out[n++] = (uint8_t)v;
        hex = end;
    }
    return n;
}

static void test_announce(void)
{
    uint8_t b[64];
    size_t n = from_hex("02 20 01 1c ab 9c 3c 12 c0 6d 00 00 d6 20 07 20 01 00 05 00 a7 00 4d 05 02 0e 01 00 01 00 01 00", b, 64);
    gip_packet p;
    CHECK(gip_parse(b, n, &p));
    CHECK(p.kind == GIP_PKT_ANNOUNCE);
    CHECK(p.sequence == 1 && p.length == 28);
    CHECK(p.u.announce.vendor_id == 0x20d6);
    CHECK(p.u.announce.product_id == 0x2007);
    CHECK(p.u.announce.address[0] == 0xab && p.u.announce.address[5] == 0x6d);
}

static void test_announce_pdp(void)
{
    // PDP Afterglow Wave 0e6f:0338, firmware 2.0.3.0, hardware 2.1.1.1. The real device
    // address is replaced with 01 02 03 04 05 06.
    uint8_t b[64];
    size_t n = from_hex("02 20 79 1c 01 02 03 04 05 06 00 00 6f 0e 38 03 02 00 00 00 03 00 00 00 02 00 01 00 01 00 01 00", b, 64);
    gip_packet p;
    CHECK(n == 32);
    CHECK(gip_parse(b, n, &p));
    CHECK(p.kind == GIP_PKT_ANNOUNCE);
    CHECK(p.sequence == 0x79 && p.length == 28);
    CHECK(p.u.announce.vendor_id == GIP_VENDOR_PDP);
    CHECK(p.u.announce.product_id == 0x0338);
    CHECK(p.u.announce.address[0] == 0x01 && p.u.announce.address[5] == 0x06);
}

static void test_status_guide_unknown(void)
{
    uint8_t b[64]; gip_packet p;
    size_t n = from_hex("03 20 01 04 80 00 00 00", b, 64);
    CHECK(gip_parse(b, n, &p) && p.kind == GIP_PKT_STATUS && p.u.status == 0x80);
    n = from_hex("07 20 02 02 01 5b", b, 64);
    CHECK(gip_parse(b, n, &p) && p.kind == GIP_PKT_GUIDE && p.u.guide_pressed);
    n = from_hex("07 20 03 02 00 5b", b, 64);
    CHECK(gip_parse(b, n, &p) && p.kind == GIP_PKT_GUIDE && !p.u.guide_pressed);
    n = from_hex("7f 00 00 00", b, 64);
    CHECK(gip_parse(b, n, &p) && p.kind == GIP_PKT_UNKNOWN);
    CHECK(!gip_parse(b, 3, &p));
    n = from_hex("20 00 01 20 00 00 00 00", b, 64);
    CHECK(!gip_parse(b, n, &p));   // input packet too short
}

static void test_input_rest(void)
{
    uint8_t b[64];
    size_t n = from_hex("20 00 01 20 00 00 00 00 00 00 40 07 b4 fe 46 04 4b fe 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00", b, 64);
    gip_packet p;
    CHECK(gip_parse(b, n, &p) && p.kind == GIP_PKT_INPUT);
    CHECK(p.length == 32);
    CHECK(p.u.input.buttons == 0);
    CHECK(p.u.input.left_trigger == 0 && p.u.input.right_trigger == 0);
    CHECK(p.u.input.left_x == 1856 && p.u.input.left_y == -332);
    CHECK(p.u.input.right_x == 1094 && p.u.input.right_y == -437);
}

static void test_input_bits(void)
{
    // Run 3: right stick click sets byte 5 bit 7.
    uint8_t b[64]; gip_packet p;
    size_t n = from_hex("20 00 5f 20 00 80 00 00 00 00 5f 04 c3 fd 35 f8 bf f0", b, 64);
    CHECK(gip_parse(b, n, &p) && p.u.input.buttons == GIP_BTN_RS);
    // Synthetic: every byte 4 and byte 5 bit maps to the named constant.
    n = from_hex("20 00 00 0e fc ff 00 00 00 00 00 00 00 00 00 00 00 00", b, 64);
    CHECK(gip_parse(b, n, &p));
    CHECK(p.u.input.buttons == (GIP_BTN_MENU | GIP_BTN_VIEW | GIP_BTN_A | GIP_BTN_B | GIP_BTN_X | GIP_BTN_Y
                                | GIP_BTN_DPAD_UP | GIP_BTN_DPAD_DOWN | GIP_BTN_DPAD_LEFT | GIP_BTN_DPAD_RIGHT
                                | GIP_BTN_LB | GIP_BTN_RB | GIP_BTN_LS | GIP_BTN_RS));
    // Full-scale triggers and sticks, 14-byte payload as a first-party pad sends.
    n = from_hex("20 00 00 0e 00 00 ff 03 ff 03 ff 7f 00 80 ff 7f 00 80", b, 64);
    CHECK(gip_parse(b, n, &p));
    CHECK(p.u.input.left_trigger == 1023 && p.u.input.right_trigger == 1023);
    CHECK(p.u.input.left_x == 32767 && p.u.input.left_y == -32768);
    CHECK(p.u.input.right_x == 32767 && p.u.input.right_y == -32768);
}

static void test_status_pdp(void)
{
    // PDP keep-alive, and the status packet the PDP pad sent after the LED packet
    // (captures/2026-10-22-pdp-probe-run2.txt).
    uint8_t b[64]; gip_packet p;
    size_t n = from_hex("03 20 01 04 80 00 00 00", b, 64);
    CHECK(gip_parse(b, n, &p) && p.kind == GIP_PKT_STATUS && p.u.status == 0x80);
    n = from_hex("03 20 02 04 80 01 00 00", b, 64);
    CHECK(gip_parse(b, n, &p) && p.kind == GIP_PKT_STATUS && p.u.status == 0x80);
}

static void test_input_pdp(void)
{
    // PDP Afterglow Wave layout: 50 bytes, payload 0x2e, share in bit 0 of byte 32 (len - 18).
    // Synthetic values: A, RB, half left trigger, full right trigger, share pressed.
    uint8_t b[64]; gip_packet p;
    size_t n = from_hex("20 00 12 2e 10 20 00 02 ff 03 ff 7f 00 80 34 12 cc ed"
                        " 00 00 00 00 00 00 00 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00 00 00"
                        " 00 00 00 00 00 00 00 00", b, 64);
    CHECK(n == 50);
    CHECK(gip_parse(b, n, &p) && p.kind == GIP_PKT_INPUT);
    CHECK(p.length == 0x2e);
    CHECK(p.u.input.buttons == (GIP_BTN_A | GIP_BTN_RB));
    CHECK(p.u.input.left_trigger == 512 && p.u.input.right_trigger == GIP_TRIGGER_MAX);
    CHECK(p.u.input.left_x == 32767 && p.u.input.left_y == -32768);
    CHECK(p.u.input.right_x == 0x1234 && p.u.input.right_y == -0x1234);
    CHECK(p.u.input.share);
    b[32] = 0x00;
    CHECK(gip_parse(b, n, &p) && !p.u.input.share);
    // A report shorter than 50 bytes has no share bit at len - 18.
    n = from_hex("20 00 00 0e 01 00 00 00 00 00 00 00 00 00 00 00 00 00", b, 64);
    CHECK(gip_parse(b, n, &p) && !p.u.input.share);
}

static void test_pdp_start(void)
{
    // PDP start sequence: power-on, guide LED on, security passed (SDL, xpad).
    gip_encoder e = { 0 };
    uint8_t out[GIP_MAX_PACKET];
    static const uint8_t power_on[] = { 0x05, 0x20, 0x01, 0x01, 0x00 };
    static const uint8_t led[] = { 0x0a, 0x20, 0x02, 0x03, 0x00, 0x01, 0x14 };
    static const uint8_t sec[] = { 0x06, 0x20, 0x03, 0x02, 0x01, 0x00 };
    CHECK(gip_encode_power_on(&e, out) == sizeof power_on && !memcmp(out, power_on, sizeof power_on));
    CHECK(gip_encode_led(&e, out, 0x14) == sizeof led && !memcmp(out, led, sizeof led));
    CHECK(gip_encode_security_passed(&e, out) == sizeof sec && !memcmp(out, sec, sizeof sec));
}

static void test_encoders(void)
{
    gip_encoder e = { 0 };
    uint8_t out[GIP_MAX_PACKET];
    static const uint8_t power_on[] = { 0x05, 0x20, 0x01, 0x01, 0x00 };
    static const uint8_t led[] = { 0x0a, 0x20, 0x02, 0x03, 0x00, 0x01, 0x14 };
    static const uint8_t sec[] = { 0x06, 0x20, 0x03, 0x02, 0x01, 0x00 };
    static const uint8_t rumble[] = { 0x09, 0x00, 0x04, 0x09, 0x00, 0x0f, 0x0a, 0x14, 0x64, 0x64, 0xff, 0x00, 0xeb };
    static const uint8_t stop[] = { 0x09, 0x00, 0x05, 0x09, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    CHECK(gip_encode_power_on(&e, out) == sizeof power_on && !memcmp(out, power_on, sizeof power_on));
    CHECK(gip_encode_led(&e, out, 0x14) == sizeof led && !memcmp(out, led, sizeof led));
    CHECK(gip_encode_security_passed(&e, out) == sizeof sec && !memcmp(out, sec, sizeof sec));
    CHECK(gip_encode_rumble(&e, out, 10, 20, 100, 200) == sizeof rumble && !memcmp(out, rumble, sizeof rumble));
    CHECK(gip_encode_rumble_stop(&e, out) == sizeof stop && !memcmp(out, stop, sizeof stop));
    // Sequence wraps from 255 to 1, never 0.
    e.sequence = 255;
    gip_encode_power_on(&e, out);
    CHECK(out[2] == 1);
}

// Replay a gip_probe capture. For each "IN" line followed by an "INPUT" line, the parsed
// values must match what the probe printed.
static void test_replay(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) { fprintf(stderr, "FAIL: cannot open %s\n", path); failures++; return; }
    char line[512], hex[512] = "";
    int packets = 0;
    while (fgets(line, sizeof line, f)) {
        const char *in = strstr(line, " IN ");
        const char *colon = in ? strchr(in, ':') : NULL;
        if (in && colon) {
            strncpy(hex, colon + 1, sizeof hex - 1);
            hex[sizeof hex - 1] = 0;
            continue;
        }
        const char *dec = strstr(line, "INPUT ");
        if (!dec || !hex[0]) continue;
        uint8_t b[64];
        size_t n = from_hex(hex, b, 64);
        hex[0] = 0;
        gip_packet p;
        if (!gip_parse(b, n, &p) || p.kind != GIP_PKT_INPUT) { CHECK(0); continue; }
        packets++;
        const gip_input *i = &p.u.input;
        // Face buttons: "A---" style field, then "dpad:UDLR", then optional "LB RB LS RS".
        const char *face = dec + 7;   // "INPUT" plus two spaces
        uint16_t want = 0;
        if (face[0] == 'A') want |= GIP_BTN_A;
        if (face[1] == 'B') want |= GIP_BTN_B;
        if (face[2] == 'X') want |= GIP_BTN_X;
        if (face[3] == 'Y') want |= GIP_BTN_Y;
        if (strstr(dec, " MENU")) want |= GIP_BTN_MENU;
        if (strstr(dec, " VIEW")) want |= GIP_BTN_VIEW;
        const char *dp = strstr(dec, "dpad:");
        if (dp) {
            if (dp[5] == 'U') want |= GIP_BTN_DPAD_UP;
            if (dp[6] == 'D') want |= GIP_BTN_DPAD_DOWN;
            if (dp[7] == 'L') want |= GIP_BTN_DPAD_LEFT;
            if (dp[8] == 'R') want |= GIP_BTN_DPAD_RIGHT;
        }
        if (strstr(dec, " LB ")) want |= GIP_BTN_LB;
        if (strstr(dec, " RB ")) want |= GIP_BTN_RB;
        if (strstr(dec, " LS ")) want |= GIP_BTN_LS;
        if (strstr(dec, " RS ")) want |= GIP_BTN_RS;
        bool want_share = strstr(dec, " SHARE ") != NULL;
        long lt = strtol(strstr(dec, "LT=") + 3, NULL, 10), rt = strtol(strstr(dec, "RT=") + 3, NULL, 10);
        long lx = strtol(strstr(dec, "LX=") + 3, NULL, 10), ly = strtol(strstr(dec, "LY=") + 3, NULL, 10);
        long rx = strtol(strstr(dec, "RX=") + 3, NULL, 10), ry = strtol(strstr(dec, "RY=") + 3, NULL, 10);
        if (i->buttons != want || i->left_trigger != lt || i->right_trigger != rt ||
            i->left_x != lx || i->left_y != ly || i->right_x != rx || i->right_y != ry ||
            i->share != want_share) {
            failures++;
            fprintf(stderr, "FAIL replay mismatch: %s", line);
        }
        checks++;
    }
    fclose(f);
    printf("replayed %d input packets from %s\n", packets, path);
    replayed_total += packets;
}

int main(int argc, char **argv)
{
    test_announce();
    test_announce_pdp();
    test_status_guide_unknown();
    test_status_pdp();
    test_input_rest();
    test_input_bits();
    test_input_pdp();
    test_pdp_start();
    test_encoders();
    for (int a = 1; a < argc; a++) test_replay(argv[a]);
    if (argc > 1) CHECK(replayed_total > 0);
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
