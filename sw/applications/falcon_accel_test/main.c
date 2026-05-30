#include <stdio.h>
#include <stdint.h>
#include "mmio.h"
#include "gr_heep.h"

#define FALCON_ACCEL_CTRL_OFFSET      0x00
#define FALCON_ACCEL_STATUS_OFFSET    0x04
#define FALCON_ACCEL_DATA_IN_OFFSET   0x08
#define FALCON_ACCEL_DATA_OUT_OFFSET  0x0C

#define FALCON_ACCEL_CTRL_START       0x1
#define FALCON_ACCEL_CTRL_CLEAR_DONE  0x2

int main(void) {
    mmio_region_t falcon_accel =
        mmio_region_from_addr((uintptr_t)FALCON_ACCEL_PERIPH_START_ADDRESS);

    printf("Falcon accelerator minimal test\n");

    mmio_region_write32(falcon_accel, FALCON_ACCEL_DATA_IN_OFFSET, 42);
    mmio_region_write32(falcon_accel, FALCON_ACCEL_CTRL_OFFSET, FALCON_ACCEL_CTRL_START);

    while ((mmio_region_read32(falcon_accel, FALCON_ACCEL_STATUS_OFFSET) & 0x1) == 0) {
    }

    uint32_t result = mmio_region_read32(falcon_accel, FALCON_ACCEL_DATA_OUT_OFFSET);

    printf("DATA_OUT = %u\n", result);

    if (result == 43) {
        printf("Falcon accelerator test PASS\n");
        return 0;
    } else {
        printf("Falcon accelerator test FAIL\n");
        return 1;
    }
}
