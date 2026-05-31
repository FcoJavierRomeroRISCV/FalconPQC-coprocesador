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
#define ACCEL_DATA4_OFFSET     0x18
#define ACCEL_DATA5_OFFSET     0x1C
#define ACCEL_DATA6_OFFSET     0x20
#define ACCEL_DATA7_OFFSET     0x24

#define ACCEL_CTRL_START       0x1
#define ACCEL_CTRL_CLEAR_DONE  0x2

#define ACCEL_STATUS_DONE      0x1

#define FALCON_Q   12289u
#define FALCON_Q0I 12287u

#define IGMB_1 4401u
#define IGMB_2 1081u
#define IGMB_3 1229u
#define IGMB_4 2530u
#define IGMB_5 6014u
#define IGMB_6 7947u
#define IGMB_7 5329u

#define NI_N8 8192u

#define INTT8_SIZE 8

static const uint32_t data_offsets[INTT8_SIZE] = {
    ACCEL_DATA0_OFFSET,
    ACCEL_DATA1_OFFSET,
    ACCEL_DATA2_OFFSET,
    ACCEL_DATA3_OFFSET,
    ACCEL_DATA4_OFFSET,
    ACCEL_DATA5_OFFSET,
    ACCEL_DATA6_OFFSET,
    ACCEL_DATA7_OFFSET,
};

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

static void mini_intt8_ref(uint32_t a[INTT8_SIZE]) {
    uint32_t s1[INTT8_SIZE];
    uint32_t s2[INTT8_SIZE];
    uint32_t s3[INTT8_SIZE];

    // Stage 1: t = 1, m = 8, hm = 4
    intt_butterfly_ref(a[0], a[1], IGMB_4, &s1[0], &s1[1]);
    intt_butterfly_ref(a[2], a[3], IGMB_5, &s1[2], &s1[3]);
    intt_butterfly_ref(a[4], a[5], IGMB_6, &s1[4], &s1[5]);
    intt_butterfly_ref(a[6], a[7], IGMB_7, &s1[6], &s1[7]);

    // Stage 2: t = 2, m = 4, hm = 2
    intt_butterfly_ref(s1[0], s1[2], IGMB_2, &s2[0], &s2[2]);
    intt_butterfly_ref(s1[1], s1[3], IGMB_2, &s2[1], &s2[3]);

    intt_butterfly_ref(s1[4], s1[6], IGMB_3, &s2[4], &s2[6]);
    intt_butterfly_ref(s1[5], s1[7], IGMB_3, &s2[5], &s2[7]);

    // Stage 3: t = 4, m = 2, hm = 1
    intt_butterfly_ref(s2[0], s2[4], IGMB_1, &s3[0], &s3[4]);
    intt_butterfly_ref(s2[1], s2[5], IGMB_1, &s3[1], &s3[5]);
    intt_butterfly_ref(s2[2], s2[6], IGMB_1, &s3[2], &s3[6]);
    intt_butterfly_ref(s2[3], s2[7], IGMB_1, &s3[3], &s3[7]);

    // Final scaling by ni for n = 8.
    for (int i = 0; i < INTT8_SIZE; i++) {
        a[i] = montgomery_ref(s3[i], NI_N8);
    }
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

static int run_intt8_test(mmio_region_t intt_accel, const uint32_t input[INTT8_SIZE]) {
    uint32_t ref[INTT8_SIZE];
    uint32_t hw[INTT8_SIZE];
    uint32_t status;
    int pass = 1;

    for (int i = 0; i < INTT8_SIZE; i++) {
        ref[i] = input[i];
    }

    mini_intt8_ref(ref);

    accel_clear_done(intt_accel);

    for (int i = 0; i < INTT8_SIZE; i++) {
        mmio_region_write32(intt_accel, data_offsets[i], input[i]);
    }

    accel_start(intt_accel);
    status = accel_wait_done(intt_accel);

    for (int i = 0; i < INTT8_SIZE; i++) {
        hw[i] = mmio_region_read32(intt_accel, data_offsets[i]);
    }

    printf("Input:  [");
    for (int i = 0; i < INTT8_SIZE; i++) {
        printf("%u", input[i]);
        if (i != INTT8_SIZE - 1) {
            printf(", ");
        }
    }
    printf("]\n");

    printf("STATUS = 0x%08x\n", status);

    for (int i = 0; i < INTT8_SIZE; i++) {
        printf("a[%d] hw=%u ref=%u\n", i, hw[i], ref[i]);
        if (hw[i] != ref[i]) {
            pass = 0;
        }
    }

    if (pass) {
        printf("iNTT8 Test PASS\n");
        return 1;
    } else {
        printf("iNTT8 Test FAIL\n");
        return 0;
    }
}

int main(void) {
    mmio_region_t intt_accel =
        mmio_region_from_addr((uintptr_t)FALCON_INTT_ACCEL_PERIPH_START_ADDRESS);

    int pass = 1;

    const uint32_t test0[INTT8_SIZE] = {
        1, 2, 3, 4, 5, 6, 7, 8
    };

    const uint32_t test1[INTT8_SIZE] = {
        100, 200, 300, 400, 500, 600, 700, 800
    };

    const uint32_t test2[INTT8_SIZE] = {
        10000, 5000, 3000, 7000, 12288, 1, 4091, 7888
    };

    const uint32_t test3[INTT8_SIZE] = {
        12288, 12287, 1, 2, 4091, 7888, 11060, 11208
    };

    printf("Falcon mini iNTT8 accelerator test\n");

    pass &= run_intt8_test(intt_accel, test0);
    pass &= run_intt8_test(intt_accel, test1);
    pass &= run_intt8_test(intt_accel, test2);
    pass &= run_intt8_test(intt_accel, test3);

    if (pass) {
        printf("Falcon mini iNTT8 accelerator test PASS\n");
        return 0;
    } else {
        printf("Falcon mini iNTT8 accelerator test FAIL\n");
        return 1;
    }
}
