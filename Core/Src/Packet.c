#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "UART.h"
#include "Packet.h"
#include "stm32f3xx_hal_uart.h"
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

typedef struct{
	UART_HandleTypeDef huart;
} Packet_Data;

static uint16_t calculate_crc(uint8_t* data, int length);
static Packet_Data packetData;

void Packet_Init(UART_HandleTypeDef huart) {
	packetData.huart = huart;
}

void SendCommandPacket(uint8_t cmd, uint8_t *data, int length, uint16_t reg, uint8_t device) {
    CMD_PACKET_INIT packet = {0};
    DEVICE dev = {0};

    packet.type = 1; // Command
    packet.command = cmd; // No-op command
    packet.length = length;

    dev.devID = device;
    dev.rvsd = 0;

    uint8_t reg_lsb = reg & 0xFF;
    uint8_t reg_msb = (reg >> 8) & 0xFF;

    uint8_t* full_packet;

    if (device != 0) full_packet = (uint8_t*)malloc(length + 6);
    else full_packet = (uint8_t*)malloc(length + 5);

    full_packet[0] = 0;
    full_packet[1] = 0;
    for (int i = 0; i < length; i++) {
        full_packet[i + 2] = ReverseByteBits((uint8_t)data[i]);
    }
    full_packet[length + 2] = ReverseByteBits(*((uint8_t*)&reg_lsb));
    full_packet[length + 3] = ReverseByteBits(*((uint8_t*)&reg_msb));

    uint16_t crc;
    if (device != 0) {
        full_packet[length + 4] = ReverseByteBits(*((uint8_t*)&dev));
        full_packet[length + 5] = ReverseByteBits(*((uint8_t*)&packet));
        crc = calculate_crc(full_packet, length + 6);
    } else {
        full_packet[length + 4] = ReverseByteBits(*((uint8_t*)&packet));
        crc = calculate_crc(full_packet, length + 5);
    }

    free(full_packet);

    uint8_t crc_lsb = crc & 0xFF;
    uint8_t crc_msb = (crc >> 8) & 0xFF;

//	HAL_UART_Transmit(&packetData.huart,(uint8_t*) &packet, length, 0xFFFF);
    UART_Transmit((uint8_t*)&packet);
    if (device != 0) UART_Transmit((uint8_t*)&dev);
    UART_Transmit(&reg_lsb);
    UART_Transmit(&reg_msb);
    for (int i = 0; i < length - 5; i++) {
        UART_Transmit(&data[i]);
    }
    UART_Transmit(&crc_lsb);
    UART_Transmit(&crc_msb);
}

uint8_t* ReadResponse() {
    uint8_t* response = UART_GetRxData();
    int length = response[0];
    uint8_t* check = (uint8_t*)malloc(3 + length);
    check[0] = 0;
    check[1] = 0;
    memcpy(check + 2, response, 1 + length);
    uint16_t crc = calculate_crc(check, 3 + length);
    free(check);
    uint16_t received_crc = (response[length + 2] << 8) | response[length + 3];
    if (crc != received_crc) {
        return NULL;
    }
    return response;
}

uint16_t calculate_crc(uint8_t* data, int length) {
    uint32_t polynomial = 0b11000000000000101;  // Polynomial from document
    
    int bit = length * 8;
    
    int local_bit = (bit - 1) % 8;
    int byte = (bit - 1)/8;
    
    data[byte] ^= 0xFF;
    data[byte - 1] ^= 0xFF;

    while (bit > 16) {
        for (int i = 0; i < 17; i++) {
            int bit_index = bit - i;
            int byte_index = (bit_index - 1) / 8;
            int bit_pos = (bit_index - 1) % 8;
            int mask_bit = (polynomial >> (16 - i)) & 1;

            data[byte_index] ^= (mask_bit << bit_pos);
        }
        
        while (!(data[byte] & (1ULL << local_bit))) {
            bit--;
            local_bit = (bit - 1) % 8;
            byte = (bit - 1)/8;
        }
    }
    
    uint16_t crc = ReverseByteBits(data[1]) << 8 | ReverseByteBits(data[0]);

    return crc;
}
