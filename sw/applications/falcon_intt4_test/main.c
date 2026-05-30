#include <stdio.h>
#include <stdint.h>

#include "mmio.h"
#include "gr_heep.h"

#define ACCEL_CTRL_OFFSET      0x00
#define ACCEL_STATUS_OFFSET    0x04
#define ACCEL_DATA0_OFFSET     0x08
#define ACCEL_DATA1_OFFSET     0x0C
#define ACCEL_DATA2_OFFSET     0x10
#define ACCEL_DATA3_OFFSET     0x14

#define ACCEL_CTRL_START       0x1
#define ACCEL_CTRL_CLEAR_DONE  0x2

#define ACCEL_STATUS_DONE      0x1

#define FALCON_Q   12289u
#define FALCON_Q0I 12287u

#define IGMB_1 4401u
#define IGMB_2 1081u
#define IGMB_3 1229u

#define NI_N4 1023u

static uint32_t mod_add_ref(uint32_t u, uint32_t v) {
    uint32_t tmp = u + v;

    if (tmp >= FALCON_Q) {
        tmp -= FALCON_Q;
    }

    return tmp;
}

static uint32_t mod_sub_ref(uint32_t u, uint32_t v) {
    if (u >= v) {
        return u - v;
    } else {
        return u + FALCON_Q - v;
    }
}

static uint32_t montgomery_ref(uint32_t x, uint32_t y) {
    uint32_t z = x * y;
    uint32_t w = ((z * FALCON_Q0I) & 0xFFFFu) * FALCON_Q;
    uint32_t t = (z + w) >> 16;

    if (t >= FALCON_Q) {
        t -= FALCON_Q;
    }

    return t;
}

static void intt_butterfly_ref(
    uint32_t u,
    uint32_t v,
    uint32_t s,
    uint32_t *out_u,
    uint32_t *out_v
) {
    uint32_t w = mod_sub_ref(u, v);

    *out_u = mod_add_ref(u, v);
    *out_v = montgomery_ref(w, s);
}

static void mini_intt4_ref(uint32_t a[4]) {
    uint32_t s1_a0;
    uint32_t s1_a1;
    uint32_t s1_a2;
    uint32_t s1_a3;

    uint32_t s2_a0;
    uint32_t s2_a1;
    uint32_t s2_a2;
    uint32_t s2_a3;

    // Stage 1: butterflies (a0,a1), (a2,a3)
    intt_butterfly_ref(a[0], a[1], IGMB_2, &s1_a0, &s1_a1);
    intt_butterfly_ref(a[2], a[3], IGMB_3, &s1_a2, &s1_a3);

    // Stage 2: butterflies (a0,a2), (a1,a3)
    intt_butterfly_ref(s1_a0, s1_a2, IGMB_1, &s2_a0, &s2_a2);
    intt_butterfly_ref(s1_a1, s1_a3, IGMB_1, &s2_a1, &s2_a3);

    // Final scaling by ni for n = 4.
    a[0] = montgomery_ref(s2_a0, NI_N4);
    a[1] = montgomery_ref(s2_a1, NI_N4);
    a[2] = montgomery_ref(s2_a2, NI_N4);
    a[3] = montgomery_ref(s2_a3, NI_N4);
}

static void accel_clear_done(mmio_region_t accel) {
    mmio_region_write32(accel, ACCEL_CTRL_OFFSET, ACCEL_CTRL_CLEAR_DONE);
}

static void accel_start(mmio_region_t accel) {
    mmio_region_write32(accel, ACCEL_CTRL_OFFSET, ACCEL_CTRL_START);
}

static uint32_t accel_wait_done(mmio_region_t accel) {
    uint32_t status;

    do {
        status = mmio_region_read32(accel, ACCEL_STATUS_OFFSET);
    } while ((status & ACCEL_STATUS_DONE) == 0);

    return status;
}

static int run_intt4_test(mmio_region_t intt_accel, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    uint32_t ref[4] = {a0, a1, a2, a3};
    uint32_t hw[4];
    uint32_t status;

    mini_intt4_ref(ref);

    accel_clear_done(intt_accel);

    mmio_region_write32(intt_accel, ACCEL_DATA0_OFFSET, a0);
    mmio_region_write32(intt_accel, ACCEL_DATA1_OFFSET, a1);
    mmio_region_write32(intt_accel, ACCEL_DATA2_OFFSET, a2);
    mmio_region_write32(intt_accel, ACCEL_DATA3_OFFSET, a3);

    accel_start(intt_accel);
    status = accel_wait_done(intt_accel);

    hw[0] = mmio_region_read32(intt_accel, ACCEL_DATA0_OFFSET);
    hw[1] = mmio_region_read32(intt_accel, ACCEL_DATA1_OFFSET);
    hw[2] = mmio_region_read32(intt_accel, ACCEL_DATA2_OFFSET);
    hw[3] = mmio_region_read32(intt_accel, ACCEL_DATA3_OFFSET);

    printf("Input:  [%u, %u, %u, %u]\n", a0, a1, a2, a3);
    printf("STATUS = 0x%08x\n", status);

    int pass = 1;

    for (int i = 0; i < 4; i++) {
        printf("a[%d] hw=%u ref=%u\n", i, hw[i], ref[i]);
        if (hw[i] != ref[i]) {
            pass = 0;
        }
    }

    if (pass) {
        printf("iNTT4 Test PASS\n");
        return 1;
    } else {
        printf("iNTT4 Test FAIL\n");
        return 0;
    }
}

int main(void) {
    mmio_region_t intt_accel =
        mmio_region_from_addr((uintptr_t)FALCON_INTT_ACCEL_PERIPH_START_ADDRESS);

    int pass = 1;

    printf("Falcon mini iNTT4 accelerator test\n");

    pass &= run_intt4_test(intt_accel, 1, 2, 3, 4);
    pass &= run_intt4_test(intt_accel, 100, 200, 300, 400);
    pass &= run_intt4_test(intt_accel, 10000, 5000, 3000, 7000);
    pass &= run_intt4_test(intt_accel, 12288, 1, 4091, 7888);

    if (pass) {
        printf("Falcon mini iNTT4 accelerator test PASS\n");
        return 0;
    } else {
        printf("Falcon mini iNTT4 accelerator test FAIL\n");
        return 1;
    }
}
