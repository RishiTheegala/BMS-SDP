#pragma once
#include <stdint.h>

#define CHECK_POLY 0xC001

typedef struct __attribute__((packed)) {
    uint64_t type : 1;        // 1 bit for type (1 = command), equal 1
    uint64_t command : 3;
    uint64_t rvsd : 1;          // Reserved bit
    uint64_t length : 3;

} CMD_PACKET_INIT;

typedef struct __attribute__((packed)) {
    // device
    uint64_t rvsd : 2;          // Reserved bits, equal 0
    uint64_t devID : 6;
} DEVICE;

void SendCommandPacket(uint8_t *data, int length, uint16_t reg, uint8_t device);
uint16_t calculate_crc(uint64_t data, int length);