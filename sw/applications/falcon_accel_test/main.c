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

#define FALCON_Q   12289u
#define FALCON_Q0I 12287u

static uint32_t montgomery_ref(uint32_t x, uint32_t y) {
    uint32_t z = x * y;
    uint32_t w = ((z * FALCON_Q0I) & 0xFFFFu) * FALCON_Q;
    uint32_t t = (z + w) >> 16;

    if (t >= FALCON_Q) {
        t -= FALCON_Q;
    }

    return t;
}

static int run_test(mmio_region_t falcon_accel, uint32_t x, uint32_t y) {
    uint32_t status;
    uint32_t hw;
    uint32_t ref = montgomery_ref(x, y);

    mmio_region_write32(
        falcon_accel,
        FALCON_ACCEL_CTRL_OFFSET,
        FALCON_ACCEL_CTRL_CLEAR_DONE
    );

    mmio_region_write32(falcon_accel, FALCON_ACCEL_DATA0_OFFSET, x);
    mmio_region_write32(falcon_accel, FALCON_ACCEL_DATA1_OFFSET, y);

    mmio_region_write32(
        falcon_accel,
        FALCON_ACCEL_CTRL_OFFSET,
        FALCON_ACCEL_CTRL_START
    );

    do {
        status = mmio_region_read32(
            falcon_accel,
            FALCON_ACCEL_STATUS_OFFSET
        );
    } while ((status & FALCON_ACCEL_STATUS_DONE) == 0);

    hw = mmio_region_read32(falcon_accel, FALCON_ACCEL_DATA0_OFFSET);

    printf("x=%u y=%u\n", x, y);
    printf("STATUS = 0x%08x\n", status);
    printf("MONT hw=%u ref=%u\n", hw, ref);

    if (hw == ref) {
        printf("Test PASS\n");
        return 1;
    } else {
        printf("Test FAIL\n");
        return 0;
    }
}

int main(void) {
    mmio_region_t falcon_accel =
        mmio_region_from_addr((uintptr_t)FALCON_ACCEL_PERIPH_START_ADDRESS);

    int pass = 1;

    printf("Falcon Montgomery multiplication test\n");

    pass &= run_test(falcon_accel, 1, 1);
    pass &= run_test(falcon_accel, 2, 3);
    pass &= run_test(falcon_accel, 1000, 2000);
    pass &= run_test(falcon_accel, 12288, 12288);
    pass &= run_test(falcon_accel, 4091, 4091);

    if (pass) {
        printf("Falcon Montgomery multiplication test PASS\n");
        return 0;
    } else {
        printf("Falcon Montgomery multiplication test FAIL\n");
        return 1;
    }
}