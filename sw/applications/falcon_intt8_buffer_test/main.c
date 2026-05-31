#include <stdio.h>
#include <stdint.h>

#include "mmio.h"
#include "gr_heep.h"
#include "falcon_accel.h"

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

    intt_butterfly_ref(a[0], a[1], IGMB_4, &s1[0], &s1[1]);
    intt_butterfly_ref(a[2], a[3], IGMB_5, &s1[2], &s1[3]);
    intt_butterfly_ref(a[4], a[5], IGMB_6, &s1[4], &s1[5]);
    intt_butterfly_ref(a[6], a[7], IGMB_7, &s1[6], &s1[7]);

    intt_butterfly_ref(s1[0], s1[2], IGMB_2, &s2[0], &s2[2]);
    intt_butterfly_ref(s1[1], s1[3], IGMB_2, &s2[1], &s2[3]);

    intt_butterfly_ref(s1[4], s1[6], IGMB_3, &s2[4], &s2[6]);
    intt_butterfly_ref(s1[5], s1[7], IGMB_3, &s2[5], &s2[7]);

    intt_butterfly_ref(s2[0], s2[4], IGMB_1, &s3[0], &s3[4]);
    intt_butterfly_ref(s2[1], s2[5], IGMB_1, &s3[1], &s3[5]);
    intt_butterfly_ref(s2[2], s2[6], IGMB_1, &s3[2], &s3[6]);
    intt_butterfly_ref(s2[3], s2[7], IGMB_1, &s3[3], &s3[7]);

    for (int i = 0; i < INTT8_SIZE; i++) {
        a[i] = montgomery_ref(s3[i], NI_N8);
    }
}

static int run_intt8_buffer_test(mmio_region_t intt_accel, const uint32_t input[INTT8_SIZE]) {
    uint32_t ref[INTT8_SIZE];
    uint32_t hw[INTT8_SIZE];

    uint32_t status;
    uint32_t size;
    uint32_t cycles;

    int pass = 1;

    size = falcon_accel_get_size(intt_accel);

    for (int i = 0; i < INTT8_SIZE; i++) {
        ref[i] = input[i];
    }

    mini_intt8_ref(ref);

    falcon_accel_clear_done(intt_accel);
    falcon_accel_write_vector(intt_accel, input, INTT8_SIZE);

    falcon_accel_start(intt_accel);
    status = falcon_accel_wait_done(intt_accel);

    cycles = falcon_accel_get_cycles(intt_accel);

    falcon_accel_read_vector(intt_accel, hw, INTT8_SIZE);

    printf("Buffer size = %u\n", size);

    printf("Input:  [");
    for (int i = 0; i < INTT8_SIZE; i++) {
        printf("%u", input[i]);
        if (i != INTT8_SIZE - 1) {
            printf(", ");
        }
    }
    printf("]\n");

    printf("STATUS = 0x%08x\n", status);
    printf("ACCEL cycles = %u\n", cycles);

    for (int i = 0; i < INTT8_SIZE; i++) {
        printf("a[%d] hw=%u ref=%u\n", i, hw[i], ref[i]);
        if (hw[i] != ref[i]) {
            pass = 0;
        }
    }

    if (pass) {
        printf("iNTT8 driver Test PASS\n");
        return 1;
    } else {
        printf("iNTT8 driver Test FAIL\n");
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

    printf("Falcon iNTT8 driver test\n");

    pass &= run_intt8_buffer_test(intt_accel, test0);
    pass &= run_intt8_buffer_test(intt_accel, test1);
    pass &= run_intt8_buffer_test(intt_accel, test2);
    pass &= run_intt8_buffer_test(intt_accel, test3);

    if (pass) {
        printf("Falcon iNTT8 driver test PASS\n");
        return 0;
    } else {
        printf("Falcon iNTT8 driver test FAIL\n");
        return 1;
    }
}
