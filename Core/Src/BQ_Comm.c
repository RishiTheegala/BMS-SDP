#include "BQ_Comm.h"
#include "Packet.h"
#include "UART.h"
#include "util.h"

const static int NUM_BQ_DEVICES = NUM_DEVICES;

void BQ_Init(void) {
    UART_Init();
    BQ_AutoAdressing();
}

void BQ_AutoAdressing(void) {
    uint8_t data[256];

    data[0] = 0;
    SendCommandPacket(RequestType::BROAD_WRITE, data, 1, Register::OTP_ECC_TEST, NULL);

    data[0] = 1;
    SendCommandPacket(RequestType::BROAD_WRITE, data, 1, Register::CONTROL1, NULL);

    for (uint8_t device = 0; device < NUM_BQ_DEVICES; device++) {
        data[0] = device; // Assign address sequentially
        SendCommandPacket(RequestType::BROAD_WRITE, data, 1, Register::DIR0_ADDR, NULL);
    }

    data[0] = 0x02;
    SendCommandPacket(RequestType::BROAD_WRITE, data, 1, Register::COMM_CTRL, NULL);
    data[0] = 0x00;  
    SendCommandPacket(RequestType::SINGLE_WRITE, data, 1, Register::COMM_CTRL, 0);
    data[0] = 0x03;
    SendCommandPacket(RequestType::SINGLE_WRITE, data, 1, Register::COMM_CTRL, NUM_BQ_DEVICES - 1);

    DummyReadResponse(RequestType::BROAD_READ, NULL, Register::OTP_ECC_TEST, 1);

    data[0] = 0xFF;
    SendCommandPacket(RequestType::BROAD_WRITE, data, 1, Register::FAULT_RST1, NULL);
    SendCommandPacket(RequestType::BROAD_WRITE, data, 1, Register::FAULT_RST2, NULL);
}