#include <stdint.h>

#define NUM_DEVICES 1
#define CELLS_PER_DEVICE 16
#define THERMISTORS_PER_DEVICE 3
#define TOTAL_CELLS (NUM_DEVICES * CELLS_PER_DEVICE)
#define TOTAL_THERMISTORS (NUM_DEVICES * THERMISTORS_PER_DEVICE)

uint8_t ReverseByteBits(uint8_t b);