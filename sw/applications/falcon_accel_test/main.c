#include <stdio.h>
#include <stdint.h>

#include "mmio.h"
#include "gr_heep.h"

#define FALCON_ACCEL_CTRL_OFFSET      0x00
#define FALCON_ACCEL_STATUS_OFFSET    0x04
#define FALCON_ACCEL_DATA0_OFFSET     0x08
#define FALCON_ACCEL_DATA1_OFFSET     0x0C
#define FALCON_ACCEL_DATA2_OFFSET     0x10
#define FALCON_ACCEL_DATA3_OFFSET     0x14

#define FALCON_ACCEL_CTRL_START       0x1
#define FALCON_ACCEL_CTRL_CLEAR_DONE  0x2

#define FALCON_ACCEL_STATUS_DONE      0x1
#define FALCON_ACCEL_STATUS_BUSY      0x2

#define NUM_WORDS 4

static const uint32_t data_offsets[NUM_WORDS] = {
    FALCON_ACCEL_DATA0_OFFSET,
    FALCON_ACCEL_DATA1_OFFSET,
    FALCON_ACCEL_DATA2_OFFSET,
    FALCON_ACCEL_DATA3_OFFSET,
};

int main(void) {
    mmio_region_t falcon_accel =
        mmio_region_from_addr((uintptr_t)FALCON_ACCEL_PERIPH_START_ADDRESS);

    uint32_t input[NUM_WORDS] = {10, 20, 30, 40};
    uint32_t output[NUM_WORDS];

    printf("Falcon accelerator small buffer test\n");

    mmio_region_write32(
        falcon_accel,
        FALCON_ACCEL_CTRL_OFFSET,
        FALCON_ACCEL_CTRL_CLEAR_DONE
    );

    for (int i = 0; i < NUM_WORDS; i++) {
        mmio_region_write32(
            falcon_accel,
            data_offsets[i],
            input[i]
        );
    }

    mmio_region_write32(
        falcon_accel,
        FALCON_ACCEL_CTRL_OFFSET,
        FALCON_ACCEL_CTRL_START
    );

    uint32_t status;
    do {
        status = mmio_region_read32(
            falcon_accel,
            FALCON_ACCEL_STATUS_OFFSET
        );
    } while ((status & FALCON_ACCEL_STATUS_DONE) == 0);

    printf("STATUS = 0x%08x\n", status);

    int pass = 1;

    for (int i = 0; i < NUM_WORDS; i++) {
        output[i] = mmio_region_read32(
            falcon_accel,
            data_offsets[i]
        );

        printf("DATA[%d] in=%u out=%u\n", i, input[i], output[i]);

        if (output[i] != input[i] + 1) {
            pass = 0;
        }
    }

    if (pass) {
        printf("Falcon accelerator small buffer test PASS\n");
        return 0;
    } else {
        printf("Falcon accelerator small buffer test FAIL\n");
        return 1;
    }
}