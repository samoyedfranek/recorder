#include "h/getRadioImage.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "h/stb_image_write.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/time.h>
#include <sys/select.h>

#define WIDTH 128
#define HEIGHT 64
#define FRAME_SIZE 1024

#define HEADER0 0xAA
#define HEADER1 0x55
#define TYPE_SCREENSHOT 0x01
#define TYPE_DIFF 0x02

static uint8_t framebuffer[FRAME_SIZE];
static int serial_fd = -1;
static uint64_t last_time = 0;
static uint8_t last_frame[FRAME_SIZE];

// --- Time helper ---
static uint64_t millis()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

// --- Serial functions ---
static int open_serial(const char *portname)
{
    int fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) return -1;

    struct termios tty;
    memset(&tty, 0, sizeof tty);
    if (tcgetattr(fd, &tty) != 0) { close(fd); return -1; }

    cfsetospeed(&tty, B38400);
    cfsetispeed(&tty, B38400);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_iflag &= ~IGNBRK;
    tty.c_lflag = 0;
    tty.c_oflag = 0;
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1;
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(PARENB | PARODD | CSTOPB);

    if (tcsetattr(fd, TCSANOW, &tty) != 0) { close(fd); return -1; }
    return fd;
}

static void send_keepalive()
{
    if (serial_fd >= 0)
    {
        uint8_t frame[4] = {0x55, 0xAA, 0x00, 0x00};
        write(serial_fd, frame, 4);
    }
}

// --- Apply diff frames ---
static void apply_diff(uint8_t *diff, int size)
{
    for (int i = 0; i + 9 <= size; i += 9)
    {
        int block_index = diff[i];
        if (block_index >= 128) break;
        memcpy(&framebuffer[block_index * 8], &diff[i + 1], 8);
    }
}

// --- Read frames efficiently ---
static int read_frame()
{
    if (serial_fd < 0) return 0;
    uint8_t buffer[2048];
    int n = read(serial_fd, buffer, sizeof(buffer));
    if (n <= 0) return 0;

    for (int i = 0; i < n - 4; i++)
    {
        if (buffer[i] == HEADER0 && buffer[i+1] == HEADER1)
        {
            uint8_t t = buffer[i+2];
            int size = (buffer[i+3] << 8) | buffer[i+4];
            if (i + 5 + size > n) break;

            if (t == TYPE_SCREENSHOT && size == FRAME_SIZE)
            {
                memcpy(framebuffer, &buffer[i+5], FRAME_SIZE);
                return 1;
            }
            else if (t == TYPE_DIFF && size % 9 == 0)
            {
                apply_diff(&buffer[i+5], size);
                return 1;
            }
        }
    }
    return 0;
}

// --- Framebuffer to RGB ---
static void framebuffer_to_rgb(uint8_t *rgb)
{
    for (int i = 0; i < FRAME_SIZE; i++)
    {
        uint8_t b = framebuffer[i];
        for (int bit = 0; bit < 8; bit++)
        {
            int idx = (i*8 + bit) * 3;
            int on = (b >> bit) & 1;
            rgb[idx + 0] = 0;
            rgb[idx + 1] = on ? 255 : 0;
            rgb[idx + 2] = 0;
        }
    }
}

// --- Save PNG only if changed ---
static void save_frame_as_png()
{
    if (memcmp(framebuffer, last_frame, FRAME_SIZE) == 0) return;

    uint8_t rgb[WIDTH * HEIGHT * 3];
    framebuffer_to_rgb(rgb);
    stbi_write_png("frame.png", WIDTH, HEIGHT, 3, rgb, WIDTH*3);

    memcpy(last_frame, framebuffer, FRAME_SIZE);
}

// --- Public API ---
int radio_init(const char *port)
{
    serial_fd = open_serial(port);
    if (serial_fd < 0) return -1;

    // wait for first frame
    while (!read_frame()) send_keepalive();
    last_time = millis();
    memcpy(last_frame, framebuffer, FRAME_SIZE);
    return 0;
}

void radio_update(void)
{
    if (serial_fd < 0) return;

    // wait for data with select (low CPU)
    fd_set fds;
    struct timeval tv = {0, 500}; // 0.5 ms
    FD_ZERO(&fds);
    FD_SET(serial_fd, &fds);
    int ret = select(serial_fd + 1, &fds, NULL, NULL, &tv);
    if (ret > 0) read_frame();

    send_keepalive();

    // save frame every 100ms if changed
    uint64_t now = millis();
    if (now - last_time >= 100)
    {
        save_frame_as_png();
        last_time = now;
    }
}

void radio_close(void)
{
    if (serial_fd >= 0)
    {
        close(serial_fd);
        serial_fd = -1;
    }
}
