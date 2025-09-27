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
    tty.c_cc[VTIME] = 5;
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(PARENB | PARODD);
    tty.c_cflag &= ~CSTOPB;

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

static void apply_diff(uint8_t *diff, int size)
{
    for (int i = 0; i + 9 <= size; i += 9)
    {
        int block_index = diff[i];
        if (block_index >= 128) break;
        memcpy(&framebuffer[block_index * 8], &diff[i + 1], 8);
    }
}

static int read_frame()
{
    if (serial_fd < 0) return 0;
    uint8_t b;
    while (1)
    {
        int n = read(serial_fd, &b, 1);
        if (n <= 0) return 0;

        if (b == HEADER0)
        {
            uint8_t b2;
            if (read(serial_fd, &b2, 1) <= 0) return 0;
            if (b2 == HEADER1)
            {
                uint8_t t;
                uint8_t size_bytes[2];
                if (read(serial_fd, &t, 1) <= 0) return 0;
                if (read(serial_fd, size_bytes, 2) <= 0) return 0;

                int size = (size_bytes[0] << 8) | size_bytes[1];
                if (t == TYPE_SCREENSHOT && size == FRAME_SIZE)
                {
                    if (read(serial_fd, framebuffer, FRAME_SIZE) <= 0) return 0;
                    return 1;
                }
                else if (t == TYPE_DIFF && size % 9 == 0)
                {
                    uint8_t diff[size];
                    if (read(serial_fd, diff, size) <= 0) return 0;
                    apply_diff(diff, size);
                    return 1;
                }
            }
        }
    }
}

static void framebuffer_to_rgb(uint8_t *rgb)
{
    int bit_index = 0;
    for (int y = 0; y < HEIGHT; y++)
    {
        for (int x = 0; x < WIDTH; x++)
        {
            int byte_idx = bit_index / 8;
            int bit_pos = bit_index % 8;
            int on = framebuffer[byte_idx] & (1 << bit_pos);
            int idx = (y * WIDTH + x) * 3;
            rgb[idx + 0] = 0;
            rgb[idx + 1] = on ? 255 : 0;
            rgb[idx + 2] = 0;
            bit_index++;
        }
    }
}

static void save_frame_as_png()
{
    uint8_t rgb[WIDTH * HEIGHT * 3];
    framebuffer_to_rgb(rgb);
    stbi_write_png("frame.png", WIDTH, HEIGHT, 3, rgb, WIDTH * 3);
}

static uint64_t millis()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

// --- Public API ---
int radio_init(const char *port)
{
    serial_fd = open_serial(port);
    if (serial_fd < 0) return -1;

    // wait for first full frame
    while (!read_frame()) send_keepalive();
    last_time = millis();
    return 0;
}

void radio_update(void)
{
    if (serial_fd < 0) return;

    uint64_t now = millis();
    if (now - last_time >= 100)
    {
        save_frame_as_png();
        last_time = now;
    }

    read_frame();
    send_keepalive();
}

void radio_close(void)
{
    if (serial_fd >= 0)
    {
        close(serial_fd);
        serial_fd = -1;
    }
}
