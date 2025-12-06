#include "bq79656.h"
#include "Packet.h"

const static int NUM_BQ_DEVICES = 3;

void BQ_Init(void) {
}

void BQ_AutoAdressing(void) {
    uint8_t data[MAX_DATA_LENGTH];

    for (uint8_t device = 0; device < NUM_BQ_DEVICES; device++) {
        data[0] = device; // Assign address sequentially
        //SendCommandPacket(, data, 1, , NULL); // Example command and register
    }
}
