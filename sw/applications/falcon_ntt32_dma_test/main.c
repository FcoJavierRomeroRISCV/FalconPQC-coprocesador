#include <stdio.h>
#include <stdint.h>

#include "dma.h"
#include "hart.h"

#define FALCON_Q   12289u
#define FALCON_Q0I 12287u

#define NTT_SIZE 32u
#define CH_NTT   0u

static uint32_t input_data[NTT_SIZE];
static uint32_t output_data[NTT_SIZE];
static uint32_t ref_data[NTT_SIZE];

static const uint32_t GMb[32] = {
    4091u, 7888u, 11060u, 11208u, 6960u, 4342u, 6275u, 9759u,
    1591u, 6399u, 9477u, 5266u, 586u, 5825u, 7538u, 9710u,
    1134u, 6407u, 1711u, 965u, 7099u, 7674u, 3743u, 6442u,
    10414u, 8100u, 1885u, 1688u, 1364u, 10329u, 10164u, 9180u
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

static void butterfly_ref(uint32_t *x0, uint32_t *x1, uint32_t s) {
    uint32_t u = *x0;
    uint32_t v = montgomery_ref(*x1, s);

    *x0 = mod_add_ref(u, v);
    *x1 = mod_sub_ref(u, v);
}

static void ntt32_ref(uint32_t a[NTT_SIZE]) {
    uint32_t t = NTT_SIZE;

    for (uint32_t m = 1; m < NTT_SIZE; m <<= 1) {
        uint32_t ht = t >> 1;

        for (uint32_t i = 0; i < m; i++) {
            uint32_t j1 = i * t;
            uint32_t j2 = j1 + ht;
            uint32_t s = GMb[m + i];

            for (uint32_t j = j1; j < j2; j++) {
                butterfly_ref(&a[j], &a[j + ht], s);
            }
        }

        t = ht;
    }
}

static void print_vector(const char *name, uint32_t v[NTT_SIZE]) {
    printf("%s[", name);

    for (uint32_t i = 0; i < NTT_SIZE; i++) {
        printf("%u", v[i]);
        if (i != NTT_SIZE - 1u) {
            printf(", ");
        }
    }

    printf("]\n");
}

static int run_dma_ntt32_test(const uint32_t test_input[NTT_SIZE]) {
    int pass = 1;

    for (uint32_t i = 0; i < NTT_SIZE; i++) {
        input_data[i] = test_input[i];
        output_data[i] = 0u;
        ref_data[i] = test_input[i];
    }

    ntt32_ref(ref_data);

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
    trans.size_d1_du = NTT_SIZE;
    trans.dim = DMA_DIM_CONF_1D;
    trans.src_type = DMA_DATA_TYPE_WORD;
    trans.dst_type = DMA_DATA_TYPE_WORD;
    trans.mode = DMA_TRANS_MODE_SINGLE;
    trans.end = DMA_TRANS_END_INTR_WAIT;
    trans.channel = CH_NTT;
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

    for (uint32_t i = 0; i < NTT_SIZE; i++) {
        printf("a[%u] hw=%u ref=%u\n", i, output_data[i], ref_data[i]);

        if (output_data[i] != ref_data[i]) {
            pass = 0;
        }
    }

    if (pass) {
        printf("NTT32 DMA Test PASS\n");
    } else {
        printf("NTT32 DMA Test FAIL\n");
    }

    return pass;
}

int main(void) {
    int pass = 1;

    const uint32_t test0[NTT_SIZE] = {
        0, 1, 2, 3, 4, 5, 6, 7,
        8, 9, 10, 11, 12, 13, 14, 15,
        16, 17, 18, 19, 20, 21, 22, 23,
        24, 25, 26, 27, 28, 29, 30, 31
    };

    const uint32_t test1[NTT_SIZE] = {
        1, 2, 3, 4, 5, 6, 7, 8,
        100, 200, 300, 400, 500, 600, 700, 800,
        900, 1000, 1100, 1200, 1300, 1400, 1500, 1600,
        1700, 1800, 1900, 2000, 2100, 2200, 2300, 2400
    };

    const uint32_t test2[NTT_SIZE] = {
        10000, 5000, 3000, 7000,
        12288, 1, 4091, 7888,
        11060, 11208, 6960, 4342,
        6275, 9759, 1591, 6399,
        9477, 5266, 586, 5825,
        7538, 9710, 1134, 6407,
        1711, 965, 7099, 7674,
        3743, 6442, 10414, 8100
    };

    const uint32_t test3[NTT_SIZE] = {
        12288, 12287, 1, 2,
        4091, 7888, 11060, 11208,
        6960, 4342, 6275, 9759,
        1591, 6399, 9477, 5266,
        586, 5825, 7538, 9710,
        1134, 6407, 1711, 965,
        7099, 7674, 3743, 6442,
        10414, 8100, 1885, 1688
    };

    printf("Falcon NTT32 DMA accelerator test\n");

#ifdef DMA_HW_FIFO_MODE
    printf("DMA_HW_FIFO_MODE enabled\n");
#else
    printf("DMA_HW_FIFO_MODE disabled\n");
#endif

    dma_init(NULL);

    pass &= run_dma_ntt32_test(test0);
    pass &= run_dma_ntt32_test(test1);
    pass &= run_dma_ntt32_test(test2);
    pass &= run_dma_ntt32_test(test3);

    if (pass) {
        printf("Falcon NTT32 DMA accelerator test PASS\n");
        return 0;
    } else {
        printf("Falcon NTT32 DMA accelerator test FAIL\n");
        return 1;
    }
}
