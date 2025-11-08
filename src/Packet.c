#include <stdio.h>
#include "UART.h"
#include "Packet.h"
#include "util.h"

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

void SendCommandPacket(uint8_t cmd, uint8_t *data, int length, uint16_t reg, uint8_t device) {
    struct CMD_PACKET_INIT packet = {0};
    struct DEVICE dev = {0};

    packet.type = 1; // Command
    packet.command = cmd; // No-op command
    packet.length = length;

    dev.devID = device;
    dev.rvsd = 0;

    uint8_t reg_lsb = reg & 0xFF;
    uint8_t reg_msb = (reg >> 8) & 0xFF;

    uint64_t full_packet = 0;
    if (device != NULL) {
        full_packet |= reverse_byte_bits(*((uint8_t*)&packet)) << (24 + (8 * length));
        full_packet |= reverse_byte_bits(*((uint8_t*)&dev)) << (16 + (8 * length));
        full_packet |= reverse_byte_bits(((uint8_t)reg_msb) << (8 + (8 * length))) | reverse_byte_bits(((uint8_t)reg_lsb) << (8 * length));
    } else {
        full_packet |= reverse_byte_bits(*((uint8_t*)&packet)) << (16 + (8 * length));
        full_packet |= reverse_byte_bits(((uint8_t)reg_msb) << (8 + (8 * length))) | reverse_byte_bits(((uint8_t)reg_lsb) << (8 * length));
    }

    for (int i = 0; i < length; i++) {
        full_packet |= (reverse_byte_bits((uint8_t)data[i]) << (8 * i));
    }

    uint16_t crc = calculate_crc(full_packet, length + 4);
    uint8_t crc_lsb = crc & 0xFF;
    uint8_t crc_msb = (crc >> 8) & 0xFF;

    UART_Transmit((uint8_t*)&packet);
    if (device != NULL) UART_Transmit((uint8_t*)&dev);
    UART_Transmit(&reg_lsb);
    UART_Transmit(&reg_msb);
    for (int i = 0; i < length - 5; i++) {
        UART_Transmit(&data[i]);
    }
    UART_Transmit(&crc_lsb);
    UART_Transmit(&crc_msb);
}

uint16_t calculate_crc(uint64_t data, int length) {
    uint64_t polynomial = 0b11000000000000101;  // Polynomial from document
    int bit = length * 8 + 16;

    data ^= 0xFFFF << ((length-2)*8);
    data <<= 16;

    while (bit > 16) {
        while (!(data & (1ULL << (bit - 1)))) {
            bit--;
        }
        data ^= polynomial << bit - 17;
    }

    uint16_t crc = reverse_byte_bits((data >> 8) & 0xFF) << 8 | reverse_byte_bits(data & 0xFF);

    return crc;  // Final XOR per spec
}