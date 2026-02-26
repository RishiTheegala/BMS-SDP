#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "UART.h"
#include "packet.h"
#include "util.h"
#include "stm32f3xx_hal.h"

// Define the rx_buffers array here (declared as extern in packet.h)
uint8_t rx_buffers[NUM_BQ_DEVICES][256];

#define CHECK_POLY 0xC001

typedef struct {
    uint64_t length : 3;
    uint64_t rvsd : 1;          // Reserved bit
    uint64_t command : 3;
    uint64_t type : 1;        // 1 bit for type (1 = command), equal 1
} CommandPacket_t;

typedef struct{
	UART_HandleTypeDef *huart;
} Packet_Data;

static Packet_Data packetData;

void GetPacket();
uint16_t calculate_crc(uint8_t* data, int length);
int check_crc(uint8_t* response, int length);

void Packet_Init(UART_HandleTypeDef *huart) {
	packetData.huart = huart;
}

uint8_t rx_buffer[256];

void SendCommandPacket(uint8_t cmd, uint8_t *data, int length, uint16_t reg, uint8_t device) {
    CommandPacket_t packet = {0};

    packet.type = 1;
    packet.command = cmd; // op code
    packet.length = length - 1;

    uint8_t reg_lsb = reg & 0xFF;
    uint8_t reg_msb = (reg >> 8) & 0xFF;

    uint8_t full_packet[100];

    full_packet[0] = 0;
    full_packet[1] = 0;
    for (int i = 0; i < length; i++) {
        full_packet[i + 2] = ReverseByteBits((uint8_t)data[i]);
    }
    full_packet[length + 2] = ReverseByteBits(*((uint8_t*)&reg_lsb));
    full_packet[length + 3] = ReverseByteBits(*((uint8_t*)&reg_msb));

    uint16_t crc;
    if (cmd < 2) {
        full_packet[length + 4] = ReverseByteBits(device);
        full_packet[length + 5] = ReverseByteBits(*((uint8_t*)&packet));
        crc = calculate_crc(full_packet, length + 6);
    } else {
        full_packet[length + 4] = ReverseByteBits(*((uint8_t*)&packet));
        crc = calculate_crc(full_packet, length + 5);
    }

    uint8_t crc_lsb = crc & 0xFF;
    uint8_t crc_msb = (crc >> 8) & 0xFF;

    static uint8_t send_packet[100];
    send_packet[0] = ((uint8_t*)&packet)[0];
    if (cmd < 2) send_packet[1] = device;
    send_packet[cmd < 2 ? 2 : 1] = reg_msb;
    send_packet[cmd < 2 ? 3 : 2] = reg_lsb;
    for (int i = length - 1; i >= 0; i--) {
        send_packet[(cmd < 2 ? 4 : 3) + (length - 1 - i)] = data[i];
    }
    send_packet[(cmd < 2 ? 4 : 3) + length] = crc_msb;
    send_packet[(cmd < 2 ? 5 : 4) + length] = crc_lsb;

    HAL_UART_Transmit(packetData.huart, send_packet, (cmd < 2 ? 6 : 5) + length, HAL_MAX_DELAY);
    
    HAL_Delay(4);
}

void DummyReadResponse(uint8_t cmd, uint8_t device, uint16_t reg, uint8_t length) {
    uint8_t data[1];
    data[0] = length - 1;
    SendCommandPacket(cmd, data, 1, reg, device);
    UART_ClearRX();
}

void ReadRegister(uint8_t cmd, uint8_t device, uint16_t reg, uint8_t length) {
    uint8_t data[1];
    data[0] = length - 1;
    SendCommandPacket(cmd, data, 1, reg, device);

    int numDevices = 1;
    if (cmd > 1) {
        numDevices = NUM_BQ_DEVICES;
        if ((cmd & 2) && !(cmd & 4)) numDevices -= 1;
    }

    HAL_Delay(4);

    for (int i = 0; i < numDevices; i++) {
        GetPacket();
        for (int j = 0; j < length; j++) {
            rx_buffers[i][j] = rx_buffer[j + 4];
        }
        // if (check_crc(rx_buffer, length)) {
        //     rx_buffers[i][0] = -1;
        // } else {
        //     for (int j = 0; j < length + 4; j++) {
        //         rx_buffers[i][j] = rx_buffer[j];
        //     }
        // }
    }
}

void GetPacket() {
    int size = UART_GetByte() + 1;
    if (size) {
        rx_buffer[0] = size;
        for (int i = 1; i < size + 6; i++) {
            rx_buffer[i] = UART_GetByte();
        }
    }
}

int check_crc(uint8_t* response, int length) {
    uint8_t check[100];
    check[0] = 0;
    check[1] = 0;
    for (int i = 0; i < length + 4; i++) {
        check[i + 2] = ReverseByteBits(response[length + 3 - i]);
    }
    uint16_t crc = calculate_crc(check, 6 + length);
    uint16_t received_crc = (response[length + 2] << 8) | response[length + 3];
    return crc != received_crc;
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
