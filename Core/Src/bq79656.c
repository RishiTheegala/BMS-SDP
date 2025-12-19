#include "bq79656.h"
#include "packet.h"
#include "util.h"

typedef enum {
    STATE_INIT,
    STATE_ACTIVE,
    STATE_FAULT,
    STATE_SHUTDOWN
} system_state_t;

typedef struct {
    int32_t current;
} bq_data_t;

static bq_data_t BQ_Data;

const static int NUM_BQ_DEVICES = NUM_DEVICES;
static system_state_t current_state = STATE_INIT;

void BQ_ReadVoltages();
void BQ_ReadCurrent();
void BQ_ReadTemps();

void BQ_SetFaultState() {
    current_state = STATE_FAULT;
}

void BQ_ClearFaults() {
    uint8_t data[1];
    data[0] = 0xFF;
    SendCommandPacket(BROAD_WRITE, data, 1, FAULT_RST1, 0);
    SendCommandPacket(BROAD_WRITE, data, 1, FAULT_RST2, 0);
}

void BQ_Thread() {
    switch (current_state) {
        case STATE_INIT:
            BQ_AutoAddressing();
            current_state = STATE_ACTIVE;
            break;
        case STATE_ACTIVE:
            BQ_ReadVoltages();
            BQ_ReadTemps();
            break;
        case STATE_FAULT:
            ReadRegister(STACK_READ, 0, FAULT_SUMMARY, 1);
            int fault_detected = 0;
            for (uint8_t device = 1; device < NUM_BQ_DEVICES; device++) {
                fault_detected |= (rx_buffers[device][0] & 0x01);
            }
            if (!fault_detected) {
                current_state = STATE_ACTIVE;
            }
            break;
        case STATE_SHUTDOWN:
            // Implement shutdown procedures
            break;
        default:
            current_state = STATE_INIT;
            break;
    }
}

void BQ_AutoAddressing() {
    uint8_t data[256];

    data[0] = 0;
    SendCommandPacket(BROAD_WRITE, data, 1, OTP_ECC_TEST, 0);

    data[0] = 1;
    SendCommandPacket(BROAD_WRITE, data, 1, CONTROL1, 0);

    for (uint8_t device = 0; device < NUM_BQ_DEVICES; device++) {
        data[0] = device; // Assign address sequentially
        SendCommandPacket(BROAD_WRITE, data, 1, DIR0_ADDR, 0);
    }

    data[0] = 0x02;
    SendCommandPacket(BROAD_WRITE, data, 1, COMM_CTRL, 0);
    data[0] = 0x00;  
    SendCommandPacket(SINGLE_WRITE, data, 1, COMM_CTRL, 0);
    data[0] = 0x03;
    SendCommandPacket(SINGLE_WRITE, data, 1, COMM_CTRL, NUM_BQ_DEVICES - 1);
    DummyReadResponse(BROAD_READ, 0, OTP_ECC_TEST, 1);

    data[0] = 0xFF;
    SendCommandPacket(BROAD_WRITE, data, 1, FAULT_RST1, 0);
    SendCommandPacket(BROAD_WRITE, data, 1, FAULT_RST2, 0);
}

void BQ_ReadVoltages() {
    uint8_t data[1];
    data[0] = 0b01000000;  // CB_PAUSE, none of the other values are read until BAL_GO is set to 1
    SendCommandPacket(STACK_WRITE, data, 1, BAL_CTRL2, 0);


    data[0] = 0b00000000;  // CB_PAUSE=0 to resume, none of the other values are read until BAL_GO is set to 1
    SendCommandPacket(STACK_WRITE, data, 1, BAL_CTRL2, 0);
}

void BQ_ReadCurrent() {
    ReadRegister(SINGLE_READ, 1, CURRENT_HI, 3);
    int32_t curr;
    ((uint8_t *)&curr)[2] = rx_buffers[0][4];
    ((uint8_t *)&curr)[1] = rx_buffers[0][5];
    ((uint8_t *)&curr)[0] = rx_buffers[0][6];
    curr = curr << 8;
    curr = curr >> 8;
    BQ_Data.current = curr;
}

int32_t BQ_GetCurrent() {
    return BQ_Data.current;
}