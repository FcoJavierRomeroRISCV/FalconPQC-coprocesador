#include <stdio.h>
#include <stdint.h>

#include "dma.h"
#include "hart.h"

#define FALCON_Q   12289u
#define FALCON_Q0I 12287u

#define INTT_SIZE 16u
#define CH_INTT   0u

static uint32_t input_data[INTT_SIZE];
static uint32_t output_data[INTT_SIZE];
static uint32_t ref_data[INTT_SIZE];

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
    }
    return u + FALCON_Q - v;
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

static void intt_butterfly_ref(uint32_t *x0, uint32_t *x1, uint32_t s) {
    uint32_t u = *x0;
    uint32_t v = *x1;

    *x0 = mod_add_ref(u, v);
    *x1 = montgomery_ref(mod_sub_ref(u, v), s);
}

static void intt16_ref(uint32_t a[INTT_SIZE]) {
    intt_butterfly_ref(&a[0],  &a[1],  2579u);
    intt_butterfly_ref(&a[2],  &a[3],  4751u);
    intt_butterfly_ref(&a[4],  &a[5],  6464u);
    intt_butterfly_ref(&a[6],  &a[7],  11703u);
    intt_butterfly_ref(&a[8],  &a[9],  7023u);
    intt_butterfly_ref(&a[10], &a[11], 2812u);
    intt_butterfly_ref(&a[12], &a[13], 5890u);
    intt_butterfly_ref(&a[14], &a[15], 10698u);

    intt_butterfly_ref(&a[0],  &a[2],  2530u);
    intt_butterfly_ref(&a[1],  &a[3],  2530u);
    intt_butterfly_ref(&a[4],  &a[6],  6014u);
    intt_butterfly_ref(&a[5],  &a[7],  6014u);
    intt_butterfly_ref(&a[8],  &a[10], 7947u);
    intt_butterfly_ref(&a[9],  &a[11], 7947u);
    intt_butterfly_ref(&a[12], &a[14], 5329u);
    intt_butterfly_ref(&a[13], &a[15], 5329u);

    intt_butterfly_ref(&a[0],  &a[4],  1081u);
    intt_butterfly_ref(&a[1],  &a[5],  1081u);
    intt_butterfly_ref(&a[2],  &a[6],  1081u);
    intt_butterfly_ref(&a[3],  &a[7],  1081u);
    intt_butterfly_ref(&a[8],  &a[12], 1229u);
    intt_butterfly_ref(&a[9],  &a[13], 1229u);
    intt_butterfly_ref(&a[10], &a[14], 1229u);
    intt_butterfly_ref(&a[11], &a[15], 1229u);

    intt_butterfly_ref(&a[0], &a[8],  4401u);
    intt_butterfly_ref(&a[1], &a[9],  4401u);
    intt_butterfly_ref(&a[2], &a[10], 4401u);
    intt_butterfly_ref(&a[3], &a[11], 4401u);
    intt_butterfly_ref(&a[4], &a[12], 4401u);
    intt_butterfly_ref(&a[5], &a[13], 4401u);
    intt_butterfly_ref(&a[6], &a[14], 4401u);
    intt_butterfly_ref(&a[7], &a[15], 4401u);

    for (uint32_t i = 0; i < INTT_SIZE; i++) {
        a[i] = montgomery_ref(a[i], 4096u);
    }
}

static void print_vector(const char *name, uint32_t v[INTT_SIZE]) {
    printf("%s[", name);

    for (uint32_t i = 0; i < INTT_SIZE; i++) {
        printf("%u", v[i]);
        if (i != INTT_SIZE - 1u) {
            printf(", ");
        }
    }

    printf("]\n");
}

static int run_dma_intt16_test(const uint32_t test_input[INTT_SIZE]) {
    int pass = 1;

    for (uint32_t i = 0; i < INTT_SIZE; i++) {
        input_data[i] = test_input[i];
        output_data[i] = 0u;
        ref_data[i] = test_input[i];
    }

    intt16_ref(ref_data);

    static dma_target_t src_target;
    static dma_target_t dst_target;
    static dma_trans_t trans;

    src_target = (dma_target_t){0};
    dst_target = (dma_target_t){0};
    trans = (dma_trans_t){0};

    src_target.ptr = (uint8_t *)input_data;
    src_target.inc_d1_du = 1;
    src_target.type = DMA_DATA_TYPE_WORD;
    src_target.trig = DMA_TRIG_MEMORY;

    dst_target.ptr = (uint8_t *)output_data;
    dst_target.inc_d1_du = 1;
    dst_target.type = DMA_DATA_TYPE_WORD;
    dst_target.trig = DMA_TRIG_MEMORY;

    trans.src = &src_target;
    trans.dst = &dst_target;
    trans.size_d1_du = INTT_SIZE;
    trans.dim = DMA_DIM_CONF_1D;
    trans.src_type = DMA_DATA_TYPE_WORD;
    trans.dst_type = DMA_DATA_TYPE_WORD;
    trans.mode = DMA_TRANS_MODE_SINGLE;
    trans.end = DMA_TRANS_END_INTR_WAIT;
    trans.channel = CH_INTT;
    trans.hw_fifo_en = 1;

    dma_config_flags_t res = dma_validate_transaction(
        &trans,
        DMA_ENABLE_REALIGN,
        DMA_PERFORM_CHECKS_INTEGRITY
    );

    if (res & DMA_CONFIG_CRITICAL_ERROR) {
        printf("DMA validation failed\n");
        return 0;
    }

    res = dma_load_transaction(&trans);

    if (res & DMA_CONFIG_CRITICAL_ERROR) {
        printf("DMA load failed\n");
        return 0;
    }

    res = dma_launch(&trans);

    if (res & DMA_CONFIG_CRITICAL_ERROR) {
        printf("DMA launch failed\n");
        return 0;
    }

    print_vector("Input:  ", input_data);
    print_vector("HW out: ", output_data);
    print_vector("REF:    ", ref_data);

    for (uint32_t i = 0; i < INTT_SIZE; i++) {
        printf("a[%u] hw=%u ref=%u\n", i, output_data[i], ref_data[i]);

        if (output_data[i] != ref_data[i]) {
            pass = 0;
        }
    }

    if (pass) {
        printf("iNTT16 DMA Test PASS\n");
    } else {
        printf("iNTT16 DMA Test FAIL\n");
    }

    return pass;
}

int main(void) {
    int pass = 1;

    const uint32_t test0[INTT_SIZE] = {
        0, 1, 2, 3, 4, 5, 6, 7,
        8, 9, 10, 11, 12, 13, 14, 15
    };

    const uint32_t test1[INTT_SIZE] = {
        1, 2, 3, 4, 5, 6, 7, 8,
        100, 200, 300, 400, 500, 600, 700, 800
    };

    const uint32_t test2[INTT_SIZE] = {
        10000, 5000, 3000, 7000,
        12288, 1, 4091, 7888,
        11060, 11208, 6960, 4342,
        6275, 9759, 1591, 6399
    };

    const uint32_t test3[INTT_SIZE] = {
        12288, 12287, 1, 2,
        4091, 7888, 11060, 11208,
        6960, 4342, 6275, 9759,
        1591, 6399, 9477, 5266
    };

    printf("Falcon iNTT16 DMA accelerator test\n");

#ifdef DMA_HW_FIFO_MODE
    printf("DMA_HW_FIFO_MODE enabled\n");
#else
    printf("DMA_HW_FIFO_MODE disabled\n");
#endif

    dma_init(NULL);

    pass &= run_dma_intt16_test(test0);
    pass &= run_dma_intt16_test(test1);
    pass &= run_dma_intt16_test(test2);
    pass &= run_dma_intt16_test(test3);

    if (pass) {
        printf("Falcon iNTT16 DMA accelerator test PASS\n");
        return 0;
    } else {
        printf("Falcon iNTT16 DMA accelerator test FAIL\n");
        return 1;
    }
}
