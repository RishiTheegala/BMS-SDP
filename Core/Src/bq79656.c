#include "bq79656.h"
#include "packet.h"
#include "stm32f303x8.h"
#include "stm32f3xx_hal_cortex.h"
#include "util.h"
#include "gpio.h"
#include <math.h>
#include <stdint.h>
#include "stm32f3xx_hal.h"

#define ADC_RESOLUTION 190.73E-6F

#define GPIO_RESOLUTION 152.59E-6F
#define THERMISTOR_PULLUP 10E4
#define THERMISTOR_BETA 4000.0F
#define ROOM_TEMP 298.15F
#define THERMISTOR_ROOM_TEMP 10000.0F

static System_State_t current_state = STATE_INIT;
volatile static BQ_Data_t BQ_Data = {0};

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
    data[0] = 0x0F & (CELLS_PER_DEVICE - 6);
    SendCommandPacket(BROAD_WRITE, data, 1, ACTIVE_CELL, 0);

    // enable TSREF
    data[0] = 0x01;
    SendCommandPacket(BROAD_WRITE, data, 1, CONTROL2, 0);
    // set up all GPIOs as ADC + OTUT inputs
    data[0] = 0x0D;
    data[1] = 0x0D;
    data[2] = 0x0D;
    data[3] = 0x0D;
    SendCommandPacket(BROAD_WRITE, data, 4, GPIO_CONF1, 0);

    data[0] = 0x0E;
    SendCommandPacket(BROAD_WRITE, data, 1, ADC_CTRL1, 0);
    data[0] = 0x06;
    SendCommandPacket(BROAD_WRITE, data, 1, ADC_CTRL3, 0);
    HAL_Delay(10);

    data[0] = 0x0;
    SendCommandPacket(BROAD_WRITE, data, 1, FAULT_MSK1, 0);
    data[0] = 0x80;
    SendCommandPacket(BROAD_WRITE, data, 1, FAULT_MSK2, 0);

    // clear all faults
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
            BQ_Data.bms_fault = 0;
            break;

        case STATE_ACTIVE:
            if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) == GPIO_PIN_RESET) { // GPIO checking NFAULT pin is low
                current_state = STATE_FAULT;
                BQ_Data.bms_fault = 1;
                BQ_EnterSleep();
            }
            break;

        case STATE_FAULT:
            if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) == GPIO_PIN_SET) { // GPIO checking NFAULT pin is high
                current_state = STATE_ACTIVE;
                BQ_Data.bms_fault = 0;
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
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET); // check sdp pin
            BQ_ReadVoltages();
            BQ_ReadTemps();
            BQ_ReadCurrent();
            break;
        case STATE_FAULT:
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET); // TODO: Safety Daisy Chain
            BQ_ReadVoltages();
            BQ_ReadTemps();
            BQ_ReadCurrent();
            break;
        default:
            current_state = STATE_INIT;
            break;
    }

    BQ_ReadFaults();
    BQ_Update();
}

void BQ_AutoAddressing(void) {
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

	if(NUM_BQ_DEVICES > 1) {
		data[0] = 0x00;  
		SendCommandPacket(SINGLE_WRITE, data, 1, COMM_CTRL, 0);
		data[0] = 0x03;
		SendCommandPacket(SINGLE_WRITE, data, 1, COMM_CTRL, NUM_BQ_DEVICES - 1);
	}
	else{
		data[0] = 0x01;  
		SendCommandPacket(SINGLE_WRITE, data, 1, COMM_CTRL, 0);
	}

    DummyReadResponse(BROAD_READ, 0, OTP_ECC_TEST, 1);
    HAL_Delay(2);

    data[0] = 0xFF;
    SendCommandPacket(BROAD_WRITE, data, 1, FAULT_RST1, 0);
    SendCommandPacket(BROAD_WRITE, data, 1, FAULT_RST2, 0);
}

void BQ_ReadVoltages(void) { // TODO: Convert readings to voltage
    uint8_t data[1];
    data[0] = 0x80;  // CB_PAUSE, none of the other values are read until BAL_GO is set to 1
    SendCommandPacket(STACK_WRITE, data, 1, BAL_CTRL2, 0);

	HAL_NVIC_DisableIRQ(CAN_RX0_IRQn);
	for (uint8_t device = 0; device < NUM_BQ_DEVICES; device++) {
		ReadRegister(SINGLE_READ, device, (VCELL1_LO + 1) - (CELLS_PER_DEVICE * 2), CELLS_PER_DEVICE * 2);
		for (uint8_t cell = 0; cell < CELLS_PER_DEVICE; cell++) {
			int16_t rawRead = ((rx_buffers[0][cell * 2] << 8) | (rx_buffers[0][cell * 2 + 1]));
			BQ_Data.voltage[device * CELLS_PER_DEVICE + cell] = rawRead * ADC_RESOLUTION;
		}
	}
	HAL_NVIC_EnableIRQ(CAN_RX0_IRQn);

    data[0] = 0x0;  // CB_PAUSE=0 to resume, none of the other values are read until BAL_GO is set to 1
    SendCommandPacket(STACK_WRITE, data, 1, BAL_CTRL2, 0);
}

void BQ_ReadTemps(void)
{
    // read temps from battery
    ReadRegister(BROAD_READ, 0, GPIO1_HI - 1, THERMISTORS_PER_DEVICE * 2);

    // fill in kNumThermistors temperatures to array
    for (int i = 0; i < NUM_BQ_DEVICES; i++)
    {
        for (int j = 0; j < THERMISTORS_PER_DEVICE; j++)
        {
            int16_t temp = (rx_buffers[NUM_BQ_DEVICES - i - 1][(2 * j) + 1] << 8) | rx_buffers[NUM_BQ_DEVICES - i - 1][2 * j];
			float thermistorVoltage = temp * GPIO_RESOLUTION;
            BQ_Data.temp[(i * THERMISTORS_PER_DEVICE) + j] = (THERMISTOR_BETA * ROOM_TEMP) / 
				(THERMISTOR_BETA + (ROOM_TEMP * logf(thermistorVoltage / THERMISTOR_ROOM_TEMP)));
        }
    }
}

void BQ_ReadCurrent(void) {
    ReadRegister(SINGLE_READ, 1, CURRENT_HI, 3);
    int32_t curr = rx_buffers[0][0] << 16 | rx_buffers[0][1] << 8 | rx_buffers[0][2];
    BQ_Data.current = curr;
}

void BQ_ModuleBalancing(uint8_t time_thres) {
    uint8_t data[1];
    data[0] = time_thres;
    SendCommandPacket(BROAD_WRITE, data, 1, MB_TIMER_CTRL, 0);
    data[0] = 0x00;
    SendCommandPacket(BROAD_WRITE, data, 1, VMB_DONE_THRESH, 0);
    data[0] = 0x06;
    SendCommandPacket(BROAD_WRITE, data, 1, ADC_CTRL3, 0);
    HAL_Delay(5);
    data[0] = 0x02;
    SendCommandPacket(BROAD_WRITE, data, 1, BAL_CTRL2, 0);
}

void BQ_StopModuleBalancing(void) {
    uint8_t data[1];
    data[0] = 0x00;
    SendCommandPacket(BROAD_WRITE, data, 1, MB_TIMER_CTRL, 0);
    data[0] = 0x02;
    SendCommandPacket(BROAD_WRITE, data, 1, BAL_CTRL2, 0);
}

void BQ_HandleBalancing(uint8_t time_thres) {
    uint8_t data[CELLS_PER_DEVICE / 2];
    // data[0] = 0b00001000;
    // SendCommandPacket(STACK_WRITE, data, 1, FAULT_MSK1, 0);

    for (int i = 0; i < CELLS_PER_DEVICE / 2; i++) {
        data[i] = time_thres;
    }

    SendCommandPacket(BROAD_WRITE,
        data,
        CELLS_PER_DEVICE / 2,
        CB_CELL1_CTRL + 1 - CELLS_PER_DEVICE,
        0);  // can only do up to 8 in one command
    SendCommandPacket(BROAD_WRITE,
        data,
        CELLS_PER_DEVICE / 2,
        CB_CELL1_CTRL + (CELLS_PER_DEVICE / 2) + 1 - CELLS_PER_DEVICE,
        0);

    // set balancing end voltage to 4V (max)
    data[0] = 0x3F;
    SendCommandPacket(BROAD_WRITE, data, 1, VCB_DONE_THRESH, 0);

    data[0] = 0x05;  // OVUV_GO, OVUV_MODE round robin
    SendCommandPacket(BROAD_WRITE, data, 1, OVUV_CTRL, 0);


    // start balancing with FLTSTOP_EN to stop on fault, OTCB_EN to pause on overtemp, AUTO_BAL to automatically cycle
    // between even/odd
    data[0] = 0x33;
    SendCommandPacket(BROAD_WRITE, data, 1, BAL_CTRL2, 0);
}

void BQ_StopBalancing(void) {
    uint8_t data[CELLS_PER_DEVICE / 2];

    for (int i = 0; i < CELLS_PER_DEVICE / 2; i++) {
        data[i] = 0x4;
    }

    SendCommandPacket(BROAD_WRITE,
        data,
        CELLS_PER_DEVICE / 2,
        CB_CELL1_CTRL + 1 - CELLS_PER_DEVICE,
        0);  // can only do up to 8 in one command
    SendCommandPacket(BROAD_WRITE,
        data,
        CELLS_PER_DEVICE / 2,
        CB_CELL1_CTRL + (CELLS_PER_DEVICE / 2) + 1 - CELLS_PER_DEVICE,
        0);

    data[0] = 0x33;  // write BAL_GO to process registers
    SendCommandPacket(BROAD_WRITE, data, 1, BAL_CTRL2, 0);

    // clear OV faults
    data[0] = 0x08;
    SendCommandPacket(BROAD_WRITE, data, 1, FAULT_RST1, 0);
    // reset fault mask 1 to re-enable OV faults
    data[0] = 0x0;
    SendCommandPacket(BROAD_WRITE, data, 1, FAULT_MSK1, 0);

}

void BQ_RunOpenWireCheck(void)
{
    uint8_t data[1];
    // Before starting the open wire detection, the host ensures
    // The Main ADC is running in continuous mode
    // Configure the open wire detection threshold through DIAG_COMP_CTRL2[OW_THR3:0]
    data[0] = 0x01;  // 1*300mv+500mv=0.8v threshold
    SendCommandPacket(BROAD_WRITE, data, 1, DIAG_COMP_CTRL2, 0);

    // To start the open wire comparison
    // Turn on the VC pins current sink or source through DIAG_COMP_CTRL3[OW_SNK1:0]
    data[0] = 0x10;
    SendCommandPacket(BROAD_WRITE, data, 1, DIAG_COMP_CTRL3, 0);

    // Wait for dV/dt time to deplete capacitors
    HAL_Delay(3);  // depletes 0.47uf at 380ua minimum, 808V/s, will deplete to at most 1.776V

    // For VC open wire detection, select DIAG_COMP_CTRL3[COMP_ADC_SEL2:0] = OW VC check (0b010) and set COMP_ADC_GO=1
    data[0] = 0x15;  // leave current sinks on
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
            complete &= rx_buffers[i][0] & 0x08;
        }
    }

    // Turns of all current sinks and sources through DIAG_COMP_CTRL3[OW_SNK1:0]
    data[0] = 0x0;
    SendCommandPacket(BROAD_WRITE, data, 1, DIAG_COMP_CTRL3, 0);
}

void BQ_SetOVUVOTUT(void) {
    uint8_t data[1];
    data[0] = 0x05;
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

void BQ_ReadFaults() {
    ReadRegister(BROAD_READ, 0, FAULT_SUMMARY, 1);
    if (!rx_buffers[0][0]) return;
    for (int i = 0; i < NUM_DEVICES; i++) {
        if (rx_buffers[i][0] & 0b01000100) {
            for (int cell = 0; cell < CELLS_PER_DEVICE; cell++) {
                float voltage = BQ_Data.voltage[CELLS_PER_DEVICE*i + cell];
                if (voltage > OV_THRESH) {
                    BQ_Data.ovuvow_fault_status[CELLS_PER_DEVICE*i + cell] = 1;
                } else if (voltage < OW_THRESH) {
                    BQ_Data.ovuvow_fault_status[CELLS_PER_DEVICE*i + cell] = 3;
                } else if (voltage < UV_THRESH) {
                    BQ_Data.ovuvow_fault_status[CELLS_PER_DEVICE*i + cell] = 2;
                } else {
                    BQ_Data.ovuvow_fault_status[CELLS_PER_DEVICE*i + cell] = 0;
                }
            }
        }

        if (rx_buffers[i][0] & 0b00001000) {
            BQ_Data.otut_fault_status[i] = 1;
        } else {
            BQ_Data.otut_fault_status[i] = 0;
        }
    }
}

void BQ_EnterSleep(void) {
    uint8_t data[1];
    data[0] = 0x04;
    SendCommandPacket(BROAD_WRITE, data, 1, CONTROL1, 0);
}

void BQ_ExitSleep(void) {
    send_Wake(1000);
    HAL_Delay(10);
    DummyReadResponse(BROAD_READ, 0, OTP_ECC_TEST, 1);
}

int32_t BQ_GetCurrent(void) {
    return BQ_Data.current;
}

float BQ_GetVoltage(int cell) {
    return BQ_Data.voltage[cell];
}

float BQ_GetTemp(int thermistor) {
    return BQ_Data.temp[thermistor];
}

int BQ_GetOVUVOWFault(int cell) {
    return BQ_Data.ovuvow_fault_status[cell];
}

int BQ_GetOTUTFault(int device) {
    return BQ_Data.otut_fault_status[device];
}

int BQ_GetBMSFault(void) {
    return BQ_Data.bms_fault;
}