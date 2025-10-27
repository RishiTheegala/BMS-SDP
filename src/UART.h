#pragma once

#include <stdint.h>

#define CHECK_POLY 0xC001

typedef struct __attribute__((packed)) {
    uint64_t type : 1;        // 1 bit for type (1 = command), equal 1
    uint64_t command : 3;
    uint64_t rvsd : 1;          // Reserved bit
    uint64_t length : 3;

} CMD_INIT;

typedef struct __attribute__((packed)) {
    uint64_t rvsd : 2;          // Reserved bits, equal 0
    uint64_t ID : 6;

} DEVICE_ADDR;

typedef struct __attribute__((packed)) {
    uint64_t msb : 8;
} REG_ADDR_H;

typedef struct __attribute__((packed)) {
    uint64_t lsb : 8;
} REG_ADDR_L;


void UART_Init();
void UART_Transmit(uint8_t data, uint8_t device, uint8_t reg);
uint8_t UART_Receive();