#include "bq79656.h"
#include "packet.h"
#include "util.h"
#include "gpio.h"
#include "stm32f3xx_hal.h"

typedef enum {
    STATE_INIT,
    STATE_ACTIVE,
    STATE_FAULT,
    STATE_SLEEP
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
void BQ_SetProtectors(float ov_thresh, float uv_thresh, float ot_thresh, float ut_thresh);
void BQ_RunOpenWireCheck();

void BQ_Init() {
    BQ_AutoAddressing();

    uint8_t data[4];

    // data[0] = 0b00001010;  // disable short comm timeout, long timeout action shutdown, long comm timeout 2s
    // SendCommandPacket(BROAD_WRITE, data, 1, COMM_TIMEOUT_CONF, 0);

    // set active cells for OV/UV
    data[0] = 0b00001111 & (CELLS_PER_DEVICE - 6);
    SendCommandPacket(BROAD_WRITE, data, 1, ACTIVE_CELL, 0);

    // enable TSREF
    data[0] = 0b00000001;
    SendCommandPacket(BROAD_WRITE, data, 1, CONTROL2, 0);
    // set up all GPIOs as ADC + OTUT inputs
    data[0] = 0b00001001;
    data[1] = 0b00001001;
    data[2] = 0b00001001;
    data[3] = 0b00001001;
    SendCommandPacket(BROAD_WRITE, data, 4, GPIO_CONF1, 0);

    data[0] = 0b00001110;
    SendCommandPacket(BROAD_WRITE, data, 1, ADC_CTRL1, 0);
    data[0] = 0x06;
    SendCommandPacket(BROAD_WRITE, data, 1, ADC_CTRL3, 0);
    HAL_Delay(10);

    data[0] = 0b00000000;
    SendCommandPacket(BROAD_WRITE, data, 1, FAULT_MSK1, 0);
    data[0] = 0b01000000;
    SendCommandPacket(BROAD_WRITE, data, 1, FAULT_MSK2, 0);

    // // clear all faults
    data[0] = 0xFF;
    SendCommandPacket(BROAD_WRITE, data, 1, FAULT_RST1, 0);
    SendCommandPacket(BROAD_WRITE, data, 1, FAULT_RST2, 0);

    BQ_SetProtectors(2.7, 1.2, 0, 0);
    BQ_RunOpenWireCheck();
}

void BQ_Update() {
    switch (current_state)
    {
        case STATE_INIT:
            current_state = STATE_ACTIVE;
            break;

        case STATE_ACTIVE:
            if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) == GPIO_PIN_RESET) { // GPIO checking NFAULT pin is high
                current_state = STATE_FAULT;
            }
            break;

        case STATE_FAULT:
            current_state = STATE_SLEEP;
            break;

        case STATE_SLEEP:
            if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) == GPIO_PIN_SET) { // GPIO checking NFAULT pin is low
                current_state = STATE_ACTIVE;
                BQ_ExitSleep();
            }
            break;
        default:
            break;
    }
}

void BQ_Main() {
    switch (current_state) {
        case STATE_INIT:
            BQ_Init();
            break;
        case STATE_ACTIVE:
            // TODO: Safety Daisy Chain

            BQ_ReadVoltages();
            BQ_ReadTemps();
            BQ_ReadCurrent();
            break;
        case STATE_FAULT:
            // TODO: Safety Daisy Chain

            BQ_EnterSleep();
            break;
        case STATE_SLEEP:
        
            break;
        default:
            current_state = STATE_INIT;
            break;
    }

    BQ_Update();
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

    // data[0] = 0x00;  
    // SendCommandPacket(SINGLE_WRITE, data, 1, COMM_CTRL, 0);
    // data[0] = 0x03;
    // SendCommandPacket(SINGLE_WRITE, data, 1, COMM_CTRL, NUM_BQ_DEVICES - 1);
    data[0] = 0x01;  
    SendCommandPacket(SINGLE_WRITE, data, 1, COMM_CTRL, 0);

    DummyReadResponse(BROAD_READ, 0, OTP_ECC_TEST, 1);
    HAL_Delay(2);

    data[0] = 0xFF;
    SendCommandPacket(BROAD_WRITE, data, 1, FAULT_RST1, 0);
    SendCommandPacket(BROAD_WRITE, data, 1, FAULT_RST2, 0);
}

void BQ_ReadVoltages() { // TODO: Convert readings to voltage
    uint8_t data[1];
    data[0] = 0b01000000;  // CB_PAUSE, none of the other values are read until BAL_GO is set to 1
    SendCommandPacket(STACK_WRITE, data, 1, BAL_CTRL2, 0);

    for (uint8_t device = 0; device < NUM_BQ_DEVICES; device++) {
        for (uint8_t cell = 0; cell < CELLS_PER_DEVICE; cell++) {
            ReadRegister(SINGLE_READ, device, VCELL16_HI + cell * 2, 2);
            int16_t voltage = (rx_buffers[device][0] << 8) | rx_buffers[device][1];
            BQ_Data.voltage[device * CELLS_PER_DEVICE + cell] = voltage * BQ_V_LSB_ADC;  // convert to volts
        }
    }

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

void BQ_ModuleBalancing() {
    uint8_t data[1];
    data[0] = 0x01;
    SendCommandPacket(BROAD_WRITE, data, 1, MB_TIMER_CTRL, 0);
    data[0] = 0x00;
    SendCommandPacket(BROAD_WRITE, data, 1, VMB_DONE_THRESH, 0);
    data[0] = 0x06;
    SendCommandPacket(BROAD_WRITE, data, 1, ADC_CTRL3, 0);
    HAL_Delay(5);
    data[0] = 0x02;
    SendCommandPacket(BROAD_WRITE, data, 1, BAL_CTRL2, 0);
}

void BQ_HandleBalancing() {
    uint8_t data[CELLS_PER_DEVICE / 2];
    // data[0] = 0b00001000;
    // SendCommandPacket(STACK_WRITE, data, 1, FAULT_MSK1, 0);

    for (int i = 0; i < CELLS_PER_DEVICE / 2; i++) {
        data[i] = 0x4;
    }

    SendCommandPacket(STACK_WRITE,
        data,
        CELLS_PER_DEVICE / 2,
        CB_CELL1_CTRL + 1 - CELLS_PER_DEVICE,
        0);  // can only do up to 8 in one command
    SendCommandPacket(STACK_WRITE,
        data,
        CELLS_PER_DEVICE / 2,
        CB_CELL1_CTRL + (CELLS_PER_DEVICE / 2) + 1 - CELLS_PER_DEVICE,
        0);

    // set balancing end voltage to 4V (max)
    data[0] = 0x3F;
    SendCommandPacket(STACK_WRITE, data, 1, VCB_DONE_THRESH, 0);

    data[0] = 0b00000101;  // OVUV_GO, OVUV_MODE round robin
    SendCommandPacket(STACK_WRITE, data, 1, OVUV_CTRL, 0);


    // start balancing with FLTSTOP_EN to stop on fault, OTCB_EN to pause on overtemp, AUTO_BAL to automatically cycle
    // between even/odd
    data[0] = 0b00110011;
    SendCommandPacket(STACK_WRITE, data, 1, BAL_CTRL2, 0);
}

void BQ_StopBalancing() {
    uint8_t data[CELLS_PER_DEVICE / 2];

    for (int i = 0; i < CELLS_PER_DEVICE / 2; i++) {
        data[i] = 0x4;
    }

    SendCommandPacket(STACK_WRITE,
        data,
        CELLS_PER_DEVICE / 2,
        CB_CELL1_CTRL + 1 - CELLS_PER_DEVICE,
        0);  // can only do up to 8 in one command
    SendCommandPacket(STACK_WRITE,
        data,
        CELLS_PER_DEVICE / 2,
        CB_CELL1_CTRL + (CELLS_PER_DEVICE / 2) + 1 - CELLS_PER_DEVICE,
        0);

    data[0] = 0b00110011;  // write BAL_GO to process registers
    SendCommandPacket(STACK_WRITE, data, 1, BAL_CTRL2, 0);

    // clear OV faults
    data[0] = 0b00001000;
    SendCommandPacket(STACK_WRITE, data, 1, FAULT_RST1, 0);
    // reset fault mask 1 to re-enable OV faults
    data[0] = 0b00000000;
    SendCommandPacket(STACK_WRITE, data, 1, FAULT_MSK1, 0);

}

void BQ_RunOpenWireCheck()
{
    uint8_t data[1];
    // Before starting the open wire detection, the host ensures
    // The Main ADC is running in continuous mode
    // Configure the open wire detection threshold through DIAG_COMP_CTRL2[OW_THR3:0]
    data[0] = 0x0 | 6;  // 6*300mv+500mv=2.3v threshold
    SendCommandPacket(BROAD_WRITE, data, 1, DIAG_COMP_CTRL2, 0);

    // To start the open wire comparison
    // Turn on the VC pins current sink or source through DIAG_COMP_CTRL3[OW_SNK1:0]
    data[0] = 0b00010000;
    SendCommandPacket(BROAD_WRITE, data, 1, DIAG_COMP_CTRL3, 0);

    // Wait for dV/dt time to deplete capacitors
    HAL_Delay(3);  // depletes 0.47uf at 380ua minimum, 808V/s, will deplete to at most 1.776V

    // For VC open wire detection, select DIAG_COMP_CTRL3[COMP_ADC_SEL2:0] = OW VC check (0b010) and set COMP_ADC_GO=1
    data[0] = 0b00010101;  // leave current sinks on
    SendCommandPacket(BROAD_WRITE, data, 1, DIAG_COMP_CTRL3, 0);

    // Device runs comparisons
    // Wait for comparison completed, ADC_STAT2[DRDY_VCOW]=1
    uint8_t complete = 0;
    while (!complete)
    {
        ReadRegister(BROAD_READ, 0, ADC_STAT2, 1);
        complete = 0xFF;
        for (int i = 0; i < NUM_BQ_DEVICES; i++)
        {
            complete &= rx_buffers[i][0] & 0b00001000;
        }
    }

    // Turns of all current sinks and sources through DIAG_COMP_CTRL3[OW_SNK1:0]
    data[0] = 0b00000000;
    SendCommandPacket(BROAD_WRITE, data, 1, DIAG_COMP_CTRL3, 0);

    // Checks the FAULT_COMP_VCOW1/2 registers for comparison result
    // Just check fault summary
    // May not be needed, can return void and let nfault trigger interrupt
    // ReadRegister(BROAD_READ, 0, FAULT_SUMMARY, 1);
    // int ow_fault = 0;
    // for (int i = 0; i < NUM_BQ_DEVICES; i++)
    // {
    //     ow_fault |= rx_buffers[NUM_BQ_DEVICES - i - 1][0] & 0b01000000;
    // }
    // return ow_fault;
}

void BQ_SetOVUVOTUT() {
    uint8_t data[1];
    data[0] = 0b00000101;
    SendCommandPacket(BROAD_WRITE, data, 1, OVUV_CTRL, 0);
    // SendCommandPacket(BROAD_WRITE, data, 1, OTUT_CTRL, 0);
}

void BQ_SetProtectors(float ov_thresh, float uv_thresh, float ot_thresh, float ut_thresh) {
    uint8_t data[1];
    uint8_t ov_offset = (ov_thresh - 4.175f) / 0.025f;
    data[0] = 0b00111111 & (ov_offset + 0x22);
    data[0] = 0x02;
    SendCommandPacket(BROAD_WRITE, data, 1, OV_THRESH, 0);

    uint8_t uv_offset = (uv_thresh - 1.2f) / 0.050f;
    data[0] = 0b00111111 & uv_offset;
    SendCommandPacket(BROAD_WRITE, data, 1, UV_THRESH, 0);

    // uint8_t ut_offset = ((TEMP_TO_VOL(ut_thresh) / 5.0f) * (100 / 2)) - 66;
    // uint8_t ot_offset = ((TEMP_TO_VOL(ot_thresh) / 5.0f) * 100) - 10;
    // data[0] = (0b11100000 & (ut_offset << 5)) | (0b00011111 & ot_offset);
    // SendCommandPacket(BROAD_WRITE, data, 1, OTUT_THRESH, 0);
    HAL_Delay(5);
    BQ_SetOVUVOTUT();
}

void BQ_EnterSleep() {
    uint8_t data[1];
    data[0] = 0x04;
    SendCommandPacket(BROAD_WRITE, data, 1, CONTROL1, 0);
}

void BQ_ExitSleep() {
    send_Wake(1000);
    HAL_Delay(10);
    DummyReadResponse(BROAD_READ, 0, OTP_ECC_TEST, 1);
}

int32_t BQ_GetCurrent() {
    return BQ_Data.current;
}

int16_t BQ_GetVoltage(int cell) {
    return BQ_Data.voltage[cell];
}