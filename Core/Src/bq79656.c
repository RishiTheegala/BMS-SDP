#include "bq79656.h"
#include "packet.h"
#include "util.h"
#include "gpio.h"
#include "stm32f3xx_hal.h"

typedef enum {
    STATE_INIT,
    STATE_ACTIVE,
    STATE_FAULT
} system_state_t;

typedef struct {
    int32_t current;
    int16_t voltage[TOTAL_CELLS];
    int32_t temp[TOTAL_THERMISTORS];
    int fault_status;
    int fault_sum;
    int fault_dev_id;
} bq_data_t;

static bq_data_t BQ_Data;

const static int NUM_BQ_DEVICES = NUM_DEVICES;
static system_state_t current_state = STATE_INIT;

void BQ_AutoAddressing();
void BQ_ReadVoltages();
void BQ_ReadCurrent();
void BQ_ReadTemps();
void BQ_EnterSleep();
void BQ_ExitSleep();

void BQ_Init() {
    BQ_AutoAddressing();

    uint8_t data_arr_[4];

    data_arr_[0] = 0b00001010;  // disable short comm timeout, long timeout action shutdown, long comm timeout 2s
    SendCommandPacket(BROAD_WRITE, data_arr_, 1, COMM_TIMEOUT_CONF, 0);

    // set active cells for OV/UV
    uint8_t series_per_segment = 16; // TODO: set correctly
    data_arr_[0] = 0b00001111 & (series_per_segment - 6);
    SendCommandPacket(BROAD_WRITE, data_arr_, 1, ACTIVE_CELL, 0);

    // enable TSREF
    data_arr_[0] = 0b00000001;
    SendCommandPacket(BROAD_WRITE, data_arr_, 1, CONTROL2, 0);
    // set up all GPIOs as ADC + OTUT inputs
    data_arr_[0] = 0b00001001;
    data_arr_[1] = 0b00001001;
    data_arr_[2] = 0b00001001;
    data_arr_[3] = 0b00001001;
    SendCommandPacket(BROAD_WRITE, data_arr_, 4, GPIO_CONF1, 0);

    data_arr_[0] = 0b00001110;
    SendCommandPacket(BROAD_WRITE, data_arr_, 1, ADC_CTRL1, 0);
    HAL_Delay(10);

    data_arr_[0] = 0b00000000;
    SendCommandPacket(BROAD_WRITE, data_arr_, 1, FAULT_MSK1, 0);
    data_arr_[0] = 0b01000000;
    SendCommandPacket(BROAD_WRITE, data_arr_, 1, FAULT_MSK2, 0);

    // clear all faults
    data_arr_[0] = 0xFF;
    SendCommandPacket(BROAD_WRITE, data_arr_, 1, FAULT_RST1, 0);
    SendCommandPacket(BROAD_WRITE, data_arr_, 1, FAULT_RST2, 0);
}

void BQ_Main() {
    switch (current_state) {
        case STATE_INIT:
            BQ_Init();
            current_state = STATE_ACTIVE;
            break;
        case STATE_ACTIVE:
            BQ_ReadVoltages();
            BQ_ReadTemps();
            BQ_ReadCurrent();
            break;
        case STATE_FAULT:
            // TODO: Safety Daisy Chain

            //if () { // GPIO checking NFAULT pin is low
                current_state = STATE_ACTIVE;
                send_Wake(1);
                HAL_Delay(10);
                DummyReadResponse(BROAD_READ, 0, OTP_ECC_TEST, 1);
            //}
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

    // data[0] = 0x02;
    // SendCommandPacket(BROAD_WRITE, data, 1, COMM_CTRL, 0);
    // data[0] = 0x00;  
    // SendCommandPacket(SINGLE_WRITE, data, 1, COMM_CTRL, 0);
    // data[0] = 0x03;
    // SendCommandPacket(SINGLE_WRITE, data, 1, COMM_CTRL, NUM_BQ_DEVICES - 1);
    DummyReadResponse(BROAD_READ, 0, OTP_ECC_TEST, 1);

    data[0] = 0xFF;
    SendCommandPacket(BROAD_WRITE, data, 1, FAULT_RST1, 0);
    SendCommandPacket(BROAD_WRITE, data, 1, FAULT_RST2, 0);
}

void BQ_SetFaultState() {
    current_state = STATE_FAULT;
}

void BQ_ReadVoltages() { // TODO: read all cells
    uint8_t data[1];
    data[0] = 0b01000000;  // CB_PAUSE, none of the other values are read until BAL_GO is set to 1
    SendCommandPacket(STACK_WRITE, data, 1, BAL_CTRL2, 0);

    ReadRegister(BROAD_READ, 0, VCELL16_HI, 2); // Read all cell voltages
    // for (uint8_t device = 0; device < NUM_BQ_DEVICES; device++) {
    //     for (uint8_t cell = 0; cell < 16; cell++) {
            int16_t voltage = (rx_buffers[0][4] << 8) | rx_buffers[0][5];
            BQ_Data.voltage[0] = voltage;
    //     }
    // }

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

void BQ_HandleBalancing() {
    uint8_t data_arr_[CELLS_PER_DEVICE / 2];
    // data_arr_[0] = 0b00001000;
    // SendCommandPacket(STACK_WRITE, data_arr_, 1, FAULT_MSK1, 0);

    for (int i = 0; i < CELLS_PER_DEVICE / 2; i++) {
        data_arr_[i] = 0x4;
    }

    SendCommandPacket(STACK_WRITE,
        data_arr_,
        CELLS_PER_DEVICE / 2,
        CB_CELL1_CTRL + 1 - CELLS_PER_DEVICE,
        0);  // can only do up to 8 in one command
    SendCommandPacket(STACK_WRITE,
        data_arr_,
        CELLS_PER_DEVICE / 2,
        CB_CELL1_CTRL + (CELLS_PER_DEVICE / 2) + 1 - CELLS_PER_DEVICE,
        0);

    // set balancing end voltage to 4V (max)
    data_arr_[0] = 0x3F;
    SendCommandPacket(STACK_WRITE, data_arr_, 1, VCB_DONE_THRESH, 0);

    data_arr_[0] = 0b00000101;  // OVUV_GO, OVUV_MODE round robin
    SendCommandPacket(STACK_WRITE, data_arr_, 1, OVUV_CTRL, 0);


    // start balancing with FLTSTOP_EN to stop on fault, OTCB_EN to pause on overtemp, AUTO_BAL to automatically cycle
    // between even/odd
    data_arr_[0] = 0b00110011;
    SendCommandPacket(STACK_WRITE, data_arr_, 1, BAL_CTRL2, 0);
}

void BQ_StopBalancing() {
    uint8_t data_arr_[CELLS_PER_DEVICE / 2];

    for (int i = 0; i < CELLS_PER_DEVICE / 2; i++) {
        data_arr_[i] = 0x4;
    }

    SendCommandPacket(STACK_WRITE,
        data_arr_,
        CELLS_PER_DEVICE / 2,
        CB_CELL1_CTRL + 1 - CELLS_PER_DEVICE,
        0);  // can only do up to 8 in one command
    SendCommandPacket(STACK_WRITE,
        data_arr_,
        CELLS_PER_DEVICE / 2,
        CB_CELL1_CTRL + (CELLS_PER_DEVICE / 2) + 1 - CELLS_PER_DEVICE,
        0);

    data_arr_[0] = 0b00110011;  // write BAL_GO to process registers
    SendCommandPacket(STACK_WRITE, data_arr_, 1, BAL_CTRL2, 0);

    // clear OV faults
    data_arr_[0] = 0b00001000;
    SendCommandPacket(STACK_WRITE, data_arr_, 1, FAULT_RST1, 0);
    // reset fault mask 1 to re-enable OV faults
    data_arr_[0] = 0b00000000;
    SendCommandPacket(STACK_WRITE, data_arr_, 1, FAULT_MSK1, 0);

}

void BQ_SetProtectors(float ov_thresh, float uv_thresh, float ot_thresh, float ut_thresh) {
    uint8_t data_arr_[1];
    uint8_t ov_offset = (ov_thresh - 4.175f) / 0.025f;
    data_arr_[0] = 0b00111111 & (ov_offset + 0x22);
    SendCommandPacket(STACK_WRITE, data_arr_, 1, OV_THRESH, 0);

    uint8_t uv_offset = (uv_thresh - 1.2f) / 0.050f;
    data_arr_[0] = 0b00111111 & uv_offset;
    SendCommandPacket(STACK_WRITE, data_arr_, 1, UV_THRESH, 0);

    uint8_t ut_offset = ((TEMP_TO_VOL(ut_thresh) / 5.0f) * (100 / 2)) - 66;
    uint8_t ot_offset = ((TEMP_TO_VOL(ot_thresh) / 5.0f) * 100) - 10;
    data_arr_[0] = (0b11100000 & (ut_offset << 5)) | (0b00011111 & ot_offset);
    SendCommandPacket(STACK_WRITE, data_arr_, 1, OTUT_THRESH, 0);
    delay(5);
}

int32_t BQ_GetCurrent() {
    return BQ_Data.current;
}

int16_t BQ_GetVoltage(int cell) {
    return BQ_Data.voltage[cell];
}