#include <stdio.h>
#include <stdint.h>

#include "mmio.h"
#include "gr_heep.h"
#include "falcon_accel.h"

#define FALCON_Q   12289u
#define FALCON_Q0I 12287u

#define GMB_1 7888u
#define GMB_2 11060u
#define GMB_3 11208u
#define GMB_4 6960u
#define GMB_5 4342u
#define GMB_6 6275u
#define GMB_7 9759u

#define IGMB_1 4401u
#define IGMB_2 1081u
#define IGMB_3 1229u
#define IGMB_4 2530u
#define IGMB_5 6014u
#define IGMB_6 7947u
#define IGMB_7 5329u

#define NI_N8 8192u

#define BLOCK_SIZE 8u

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

static void ntt_butterfly_ref(
    uint32_t u,
    uint32_t x,
    uint32_t s,
    uint32_t *out_u,
    uint32_t *out_x
) {
    uint32_t v = montgomery_ref(x, s);

    *out_u = mod_add_ref(u, v);
    *out_x = mod_sub_ref(u, v);
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

static void mini_ntt8_ref(uint32_t a[BLOCK_SIZE]) {
    uint32_t s1[BLOCK_SIZE];
    uint32_t s2[BLOCK_SIZE];
    uint32_t s3[BLOCK_SIZE];

    ntt_butterfly_ref(a[0], a[4], GMB_1, &s1[0], &s1[4]);
    ntt_butterfly_ref(a[1], a[5], GMB_1, &s1[1], &s1[5]);
    ntt_butterfly_ref(a[2], a[6], GMB_1, &s1[2], &s1[6]);
    ntt_butterfly_ref(a[3], a[7], GMB_1, &s1[3], &s1[7]);

    ntt_butterfly_ref(s1[0], s1[2], GMB_2, &s2[0], &s2[2]);
    ntt_butterfly_ref(s1[1], s1[3], GMB_2, &s2[1], &s2[3]);

    ntt_butterfly_ref(s1[4], s1[6], GMB_3, &s2[4], &s2[6]);
    ntt_butterfly_ref(s1[5], s1[7], GMB_3, &s2[5], &s2[7]);

    ntt_butterfly_ref(s2[0], s2[1], GMB_4, &s3[0], &s3[1]);
    ntt_butterfly_ref(s2[2], s2[3], GMB_5, &s3[2], &s3[3]);
    ntt_butterfly_ref(s2[4], s2[5], GMB_6, &s3[4], &s3[5]);
    ntt_butterfly_ref(s2[6], s2[7], GMB_7, &s3[6], &s3[7]);

    for (uint32_t i = 0; i < BLOCK_SIZE; i++) {
        a[i] = s3[i];
    }
}

static void mini_intt8_ref(uint32_t a[BLOCK_SIZE]) {
    uint32_t s1[BLOCK_SIZE];
    uint32_t s2[BLOCK_SIZE];
    uint32_t s3[BLOCK_SIZE];

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

    for (uint32_t i = 0; i < BLOCK_SIZE; i++) {
        a[i] = montgomery_ref(s3[i], NI_N8);
    }
}

static uint32_t make_input_value(uint32_t index) {
    return ((index * 37u) + 11u) % FALCON_Q;
}

static void fill_block(uint32_t block[BLOCK_SIZE], uint32_t base_index) {
    for (uint32_t i = 0; i < BLOCK_SIZE; i++) {
        block[i] = make_input_value(base_index + i);
    }
}

static int compare_block(
    const uint32_t hw[BLOCK_SIZE],
    const uint32_t ref[BLOCK_SIZE]
) {
    for (uint32_t i = 0; i < BLOCK_SIZE; i++) {
        if (hw[i] != ref[i]) {
            printf("Mismatch at local index %u: hw=%u ref=%u\n", i, hw[i], ref[i]);
            return 0;
        }
    }

    return 1;
}

static int run_benchmark(
    const char *name,
    mmio_region_t accel,
    uint32_t total_len,
    int is_intt
) {
    uint32_t input[BLOCK_SIZE];
    uint32_t ref[BLOCK_SIZE];
    uint32_t hw[BLOCK_SIZE];

    uint32_t blocks = total_len / BLOCK_SIZE;
    uint32_t accel_cycles_total = 0;

    int pass = 1;

    falcon_accel_reset_counters();

    for (uint32_t b = 0; b < blocks; b++) {
        uint32_t base = b * BLOCK_SIZE;

        fill_block(input, base);

        for (uint32_t i = 0; i < BLOCK_SIZE; i++) {
            ref[i] = input[i];
        }

        if (is_intt) {
            mini_intt8_ref(ref);
        } else {
            mini_ntt8_ref(ref);
        }

        falcon_accel_clear_done(accel);
        falcon_accel_write_vector(accel, input, BLOCK_SIZE);

        falcon_accel_start(accel);
        falcon_accel_wait_done(accel);

        accel_cycles_total += falcon_accel_get_cycles(accel);

        falcon_accel_read_vector(accel, hw, BLOCK_SIZE);

        if (!compare_block(hw, ref)) {
            printf("%s FAIL at block %u\n", name, b);
            pass = 0;
            break;
        }
    }

    printf("==== %s benchmark ====\n", name);
    printf("Total coefficients = %u\n", total_len);
    printf("Blocks of 8 = %u\n", blocks);
    printf("Internal accelerator cycles total = %u\n", accel_cycles_total);
    printf("Internal accelerator cycles per block = %u\n", accel_cycles_total / blocks);
    printf("Internal accelerator cycles per coefficient x1000 = %u\n",
           (accel_cycles_total * 1000u) / total_len);
    printf("MMIO writes = %u\n", falcon_accel_get_mmio_writes());
    printf("MMIO reads = %u\n", falcon_accel_get_mmio_reads());
    printf("MMIO total accesses = %u\n", falcon_accel_get_mmio_total());

    if (pass) {
        printf("%s benchmark PASS\n", name);
        return 1;
    } else {
        printf("%s benchmark FAIL\n", name);
        return 0;
    }
}

int main(void) {
    mmio_region_t ntt_accel =
        mmio_region_from_addr((uintptr_t)FALCON_NTT_ACCEL_PERIPH_START_ADDRESS);

    mmio_region_t intt_accel =
        mmio_region_from_addr((uintptr_t)FALCON_INTT_ACCEL_PERIPH_START_ADDRESS);

    int pass = 1;

    printf("Falcon driver benchmark test\n");

    pass &= run_benchmark("NTT 512", ntt_accel, 512u, 0);
    pass &= run_benchmark("iNTT 512", intt_accel, 512u, 1);

    pass &= run_benchmark("NTT 1024", ntt_accel, 1024u, 0);
    pass &= run_benchmark("iNTT 1024", intt_accel, 1024u, 1);

    if (pass) {
        printf("Falcon driver benchmark test PASS\n");
        return 0;
    } else {
        printf("Falcon driver benchmark test FAIL\n");
        return 1;
    }
}
