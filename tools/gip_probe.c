// gip_probe: userspace proof of concept for a wired GIP gamepad (compatible with Xbox One and Series controllers) via libusb.
//
// Steps:
//   1. Open the pad by vendor and product ID and claim interface 0.
//   2. Read the interrupt IN endpoint in a thread. Hex-dump every packet with a timestamp.
//   3. Listen for a while (expect an announce packet, command 0x02).
//   4. Send the power-on packet. Listen.
//   5. If no input report (command 0x20) arrived, send rumble begin, then rumble end. Listen.
//   6. Decode input reports until the time limit.
//
// Only the known init and rumble packets are sent. Nothing else is written to the pad.
//
// Build: clang -O1 -o gip_probe gip_probe.c $(pkg-config --cflags --libs libusb-1.0) -lpthread
// Run:   ./gip_probe [seconds] [--no-init] [--force-rumble] [--led] [--raw]

#include <libusb.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#define VID 0x20d6
#define PID 0x2007
#define IFACE 0
#define EP_IN 0x82
#define EP_OUT 0x02

// Init sequence as data. Byte 2 of each packet is the GIP sequence number. The sender fills it in.
typedef struct { const char *name; uint8_t data[64]; int len; } packet_t;

static const packet_t PKT_POWER_ON   = { "power-on (05 20 seq 01 00)", {0x05, 0x20, 0x00, 0x01, 0x00}, 5 };
static const packet_t PKT_RUMBLE_ON  = { "PowerA rumble begin",  {0x09, 0x00, 0x00, 0x09, 0x00, 0x0F, 0x00, 0x00, 0x1D, 0x1D, 0xFF, 0x00, 0x00}, 13 };
// Guide LED on, brightness 0x14. SDL and xone send this during init. Off by default here; enable with --led.
static const packet_t PKT_LED_ON     = { "guide LED on (0a 20 seq 03 00 01 14)", {0x0A, 0x20, 0x00, 0x03, 0x00, 0x01, 0x14}, 7 };
static const packet_t PKT_RUMBLE_OFF = { "PowerA rumble end",    {0x09, 0x00, 0x00, 0x09, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, 13 };

static libusb_device_handle *h;
static atomic_int running = 1;
static atomic_int got_input = 0;
static atomic_int got_announce = 0;
static uint8_t seq = 0;
static struct timeval t0;
static int decode_input = 1;

static double now(void) {
    struct timeval t; gettimeofday(&t, NULL);
    return (t.tv_sec - t0.tv_sec) + (t.tv_usec - t0.tv_usec) / 1e6;
}

static void hexdump(const char *dir, const uint8_t *p, int n) {
    printf("%8.3f %s %3d:", now(), dir, n);
    for (int i = 0; i < n; i++) printf(" %02x", p[i]);
    printf("\n");
    fflush(stdout);
}

static int16_t s16(const uint8_t *p) { return (int16_t)(p[0] | (p[1] << 8)); }
static uint16_t u16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

static void decode(const uint8_t *d, int n) {
    if (n < 4) return;
    uint8_t cmd = d[0];
    if (cmd == 0x20 && n >= 18) {
        got_input = 1;
        if (!decode_input) return;
        uint8_t b4 = d[4], b5 = d[5];
        printf("         INPUT  %s%s%s%s%s%s  dpad:%s%s%s%s %s%s%s%s  LT=%4u RT=%4u  LX=%6d LY=%6d RX=%6d RY=%6d\n",
               (b4 & 0x10) ? "A" : "-", (b4 & 0x20) ? "B" : "-", (b4 & 0x40) ? "X" : "-", (b4 & 0x80) ? "Y" : "-",
               (b4 & 0x04) ? " MENU" : "", (b4 & 0x08) ? " VIEW" : "",
               (b5 & 0x01) ? "U" : "-", (b5 & 0x02) ? "D" : "-", (b5 & 0x04) ? "L" : "-", (b5 & 0x08) ? "R" : "-",
               (b5 & 0x10) ? "LB " : "", (b5 & 0x20) ? "RB " : "", (b5 & 0x40) ? "LS " : "", (b5 & 0x80) ? "RS " : "",
               u16(d + 6), u16(d + 8), s16(d + 10), s16(d + 12), s16(d + 14), s16(d + 16));
        fflush(stdout);
    } else if (cmd == 0x07) {
        printf("         GUIDE  %s\n", (n > 4 && (d[4] & 1)) ? "pressed" : "released");
        fflush(stdout);
    } else if (cmd == 0x02) {
        got_announce = 1;
        printf("         ANNOUNCE (cmd 0x02)\n");
        fflush(stdout);
    } else if (cmd == 0x03) {
        printf("         STATUS (cmd 0x03)\n");
        fflush(stdout);
    }
}

static void *reader(void *arg) {
    (void)arg;
    uint8_t buf[64];
    while (running) {
        int got = 0;
        int r = libusb_interrupt_transfer(h, EP_IN, buf, sizeof buf, &got, 200);
        if (r == 0 && got > 0) { hexdump("IN ", buf, got); decode(buf, got); }
        else if (r != LIBUSB_ERROR_TIMEOUT) { printf("%8.3f IN  error: %s\n", now(), libusb_error_name(r)); fflush(stdout); usleep(100000); }
    }
    return NULL;
}

static int send_packet(const packet_t *p) {
    uint8_t buf[64]; memcpy(buf, p->data, p->len);
    buf[2] = seq++;
    int sent = 0;
    int r = libusb_interrupt_transfer(h, EP_OUT, buf, p->len, &sent, 1000);
    printf("%8.3f --- send %s\n", now(), p->name);
    hexdump("OUT", buf, p->len);
    if (r) printf("%8.3f OUT error: %s\n", now(), libusb_error_name(r));
    fflush(stdout);
    return r;
}

static void listen_for(double secs) { double end = now() + secs; while (now() < end) usleep(10000); }

int main(int argc, char **argv) {
    double total = argc > 1 && argv[1][0] != '-' ? atof(argv[1]) : 20;
    int do_init = 1, force_rumble = 0, led = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--no-init")) do_init = 0;
        if (!strcmp(argv[i], "--force-rumble")) force_rumble = 1;
        if (!strcmp(argv[i], "--raw")) decode_input = 0;
        if (!strcmp(argv[i], "--led")) led = 1;
    }
    gettimeofday(&t0, NULL);
    int r = libusb_init(NULL);
    if (r) { fprintf(stderr, "libusb_init: %s\n", libusb_error_name(r)); return 1; }
    h = libusb_open_device_with_vid_pid(NULL, VID, PID);
    if (!h) { fprintf(stderr, "device %04x:%04x not found or open failed\n", VID, PID); return 1; }
    int cfg = -1;
    libusb_get_configuration(h, &cfg);
    printf("%8.3f opened %04x:%04x, current configuration=%d\n", now(), VID, PID, cfg);
    if (cfg != 1) {
        r = libusb_set_configuration(h, 1);
        printf("%8.3f set_configuration(1): %s\n", now(), r ? libusb_error_name(r) : "ok");
    }
    r = libusb_claim_interface(h, IFACE);
    printf("%8.3f claim_interface(%d): %s\n", now(), IFACE, r ? libusb_error_name(r) : "ok");
    if (r) return 1;

    pthread_t th; pthread_create(&th, NULL, reader, NULL);

    printf("%8.3f --- listening 2 s for unprompted traffic\n", now()); fflush(stdout);
    listen_for(2.0);

    if (do_init) {
        send_packet(&PKT_POWER_ON);
        listen_for(2.0);
        if (led) { send_packet(&PKT_LED_ON); listen_for(0.5); }
        if (!got_input || force_rumble) {
            printf("%8.3f --- no input report yet, trying PowerA rumble begin/end\n", now()); fflush(stdout);
            send_packet(&PKT_RUMBLE_ON);
            listen_for(0.3);
            send_packet(&PKT_RUMBLE_OFF);
            listen_for(2.0);
        }
    }
    printf("%8.3f --- got_announce=%d got_input=%d; decoding until t=%.0f s. Press buttons now.\n", now(), got_announce, got_input, total);
    fflush(stdout);
    while (now() < total) usleep(50000);

    running = 0; pthread_join(th, NULL);
    libusb_release_interface(h, IFACE);
    libusb_close(h);
    libusb_exit(NULL);
    printf("%8.3f done. got_announce=%d got_input=%d\n", now(), got_announce, got_input);
    return got_input ? 0 : 2;
}
