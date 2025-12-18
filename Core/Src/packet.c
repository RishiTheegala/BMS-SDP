#include <stdint.h>
#include <stdlib.h>
#include "UART.h"
#include "packet.h"
#include "util.h"
#include "stm32f3xx_hal.h"

// Define the rx_buffers array here (declared as extern in packet.h)
uint8_t rx_buffers[NUM_DEVICES][256];

#define CHECK_POLY 0xC001

typedef struct {
    uint64_t length : 3;
    uint64_t rvsd : 1;          // Reserved bit
    uint64_t command : 3;
    uint64_t type : 1;        // 1 bit for type (1 = command), equal 1

} __attribute__((packed)) CMD_PACKET_INIT;

typedef struct{
	UART_HandleTypeDef huart;
} Packet_Data;

static Packet_Data packetData;

void GetPacket();
uint16_t calculate_crc(uint8_t* data, int length);
int check_crc(uint8_t* response, int length);

void Packet_Init(UART_HandleTypeDef huart) {
	packetData.huart = huart;
}

uint8_t rx_buffer[256];

void SendCommandPacket(uint8_t cmd, uint8_t *data, int length, uint16_t reg, uint8_t device) {
    CMD_PACKET_INIT packet = {0};

    packet.type = 1; // Command
    packet.command = cmd; // No-op command
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

//	HAL_UART_Transmit(&packetData.huart,(uint8_t*) &packet, length, 0xFFFF);
    UART_Transmit((uint8_t*)&packet);
    if (cmd < 2) UART_Transmit((&device));
    UART_Transmit(&reg_lsb);
    UART_Transmit(&reg_msb);
    for (int i = 0; i < length; i++) {
        UART_Transmit(&data[i]);
    }
    UART_Transmit(&crc_lsb);
    UART_Transmit(&crc_msb);
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
    data[0] = length;
    SendCommandPacket(cmd, data, 1, reg, device);

    int numDevices = 1;
    if ((int)device < 0) {
        numDevices = NUM_DEVICES;
        if ((cmd & 2) && !(cmd & 4)) numDevices -= 1;
    }

    for (int i = 0; i < numDevices; i++) {
        GetPacket();
        if (check_crc(rx_buffer, length)) {
            printf("CRC Error on device %d\n", rx_buffer[1]);
        } else {
            for (int j = 0; j < length + 4; j++) {
                rx_buffers[i][j] = rx_buffer[j];
            }
        }
    }
}

void GetPacket() {
    int size = UART_GetByte() + 1;
    if (size) {
        rx_buffer[0] = size;
        for (int i = 1; i < size + 4; i++) {
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