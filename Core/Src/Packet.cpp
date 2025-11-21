#include <stdio.h>
#include <stdint.h>
#include "UART.h"
#include "Packet.h"
#include <vector>

#define CHECK_POLY 0xC001

typedef struct {
    uint64_t type : 1;        // 1 bit for type (1 = command), equal 1
    uint64_t command : 3;
    uint64_t rvsd : 1;          // Reserved bit
    uint64_t length : 3;

} __attribute__((packed)) CMD_PACKET_INIT;

typedef struct {
    // device
    uint64_t rvsd : 2;          // Reserved bits, equal 0
    uint64_t devID : 6;
} __attribute__((packed)) DEVICE;

std::vector<uint8_t> rx_buffer;

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

    size_t full_len = length + (device != 0 ? 6 : 5);
    std::vector<uint8_t> full_packet(full_len);

    full_packet[0] = 0;
    full_packet[1] = 0;

    full_packet[0] = 0;
    full_packet[1] = 0;
    for (int i = 0; i < length; i++) {
        full_packet[i + 2] = ReverseByteBits((uint8_t)data[i]);
    }
    full_packet[length + 2] = ReverseByteBits(*((uint8_t*)&reg_lsb));
    full_packet[length + 3] = ReverseByteBits(*((uint8_t*)&reg_msb));

    uint16_t crc;
    if (device != NULL) {
        full_packet[length + 4] = ReverseByteBits(*((uint8_t*)&dev));
        full_packet[length + 5] = ReverseByteBits(*((uint8_t*)&packet));
        crc = calculate_crc(full_packet, length + 6);
    } else {
        full_packet[length + 4] = ReverseByteBits(*((uint8_t*)&packet));
        crc = calculate_crc(full_packet, length + 5);
    }

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

void DummyReadResponse(uint8_t cmd, uint8_t device, uint16_t reg, uint8_t length) {
    uint8_t data[1];
    data[0] = length;
    SendCommandPacket(cmd, data, 1, reg, device);
    UART_ClearRX();
}

void ReadRegister(uint8_t cmd, uint8_t device, uint16_t reg, uint8_t length) {
    uint8_t data[1];
    data[0] = length;
    SendCommandPacket(cmd, data, 1, reg, device);

    int numDevices = 1;
    if (device == NULL) {
        numDevices = NUM_DEVICES;
        if ((cmd & 2) && !(cmd & 4)) numDevices -= 1;
    }

    for (int i = 0; i < numDevices; i++) {
        GetPacket();
        if (check_crc(rx_buffer)) {
            printf("CRC Error on device %d\n", rx_buffer[1]);
        } else {
            for (int j = 0; j < length + 4; j++) {
                rx_buffers[i][j] = rx_buffer[j];
            }
        }
    }
}

void GetPacket() {
    int size = UART_GetByte();
    rx_buffer = std::vector<uint8_t>(size + 6);
    if (size) {
        rx_buffer[0] = size;
        for (int i = 1; i < size + 6; i++) {
            rx_buffer[i] = UART_GetByte();
        }
    }
}

int check_crc(std::vector<uint8_t> response) {
    int length = response[0];
    std::vector<uint8_t> check(6 + length);
    check[0] = 0;
    check[1] = 0;
    for (int i = 0; i < length + 4; i++) {
        check[i + 2] = ReverseByteBits(response[length + 3 - i]);
    }
    uint16_t crc = calculate_crc(check, 3 + length);
    uint16_t received_crc = (response[length + 2] << 8) | response[length + 3];
    return crc != received_crc;
}

uint16_t calculate_crc(std::vector<uint8_t> data, int length) {
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