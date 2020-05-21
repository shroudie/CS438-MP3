#ifndef _COMMON_H_
#define _COMMON_H_

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdbool.h>

#define MTU_PAYLOAD 1472
#define INFO_SIZE 8

typedef uint32_t sequence;
typedef uint64_t file_size;

constexpr sequence  _SEND_WINDOW() { return 256; }
constexpr sequence  _RECV_WINDOW() { return 256; }
constexpr file_size  _FRAME_SIZE() { return ((6000 / MTU_PAYLOAD) * MTU_PAYLOAD - INFO_SIZE); }

#define SEND_WINDOW _SEND_WINDOW()
#define RECV_WINDOW _RECV_WINDOW()
#define FRAME_SIZE   _FRAME_SIZE()

// #define DEBUG

typedef struct packet {
    sequence seq;
    uint16_t len;
    uint16_t fin = 0;
    char buf[FRAME_SIZE];
} packet;

#endif