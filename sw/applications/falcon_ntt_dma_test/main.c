#include <stdio.h>
#include <stdint.h>

#include "dma.h"
#include "hart.h"

#define FALCON_Q   12289u
#define FALCON_Q0I 12287u

#define NTT_SIZE 8u

static uint32_t input_data[NTT_SIZE];
static uint32_t output_data[NTT_SIZE];
static uint32_t ref_data[NTT_SIZE];

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

static void butterfly_ref(uint32_t *x0, uint32_t *x1, uint32_t s) {
    uint32_t u = *x0;
    uint32_t v = montgomery_ref(*x1, s);

    *x0 = mod_add_ref(u, v);
    *x1 = mod_sub_ref(u, v);
}

static void ntt8_ref(uint32_t a[NTT_SIZE]) {
    butterfly_ref(&a[0], &a[4], 7888u);
    butterfly_ref(&a[1], &a[5], 7888u);
    butterfly_ref(&a[2], &a[6], 7888u);
    butterfly_ref(&a[3], &a[7], 7888u);

    butterfly_ref(&a[0], &a[2], 11060u);
    butterfly_ref(&a[1], &a[3], 11060u);
    butterfly_ref(&a[4], &a[6], 11208u);
    butterfly_ref(&a[5], &a[7], 11208u);

    butterfly_ref(&a[0], &a[1], 6960u);
    butterfly_ref(&a[2], &a[3], 4342u);
    butterfly_ref(&a[4], &a[5], 6275u);
    butterfly_ref(&a[6], &a[7], 9759u);
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

static int run_dma_ntt_test(const uint32_t test_input[NTT_SIZE]) {
    int pass = 1;

    for (uint32_t i = 0; i < NTT_SIZE; i++) {
        input_data[i] = test_input[i];
        output_data[i] = 0u;
        ref_data[i] = test_input[i];
    }

    ntt8_ref(ref_data);

    dma_init(NULL);

    static dma_target_t src_target = {0};
    static dma_target_t dst_target = {0};
    static dma_trans_t trans = {0};

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
    trans.channel = 0;
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
        printf("NTT8 DMA Test PASS\n");
    } else {
        printf("NTT8 DMA Test FAIL\n");
    }

    return pass;
}

int main(void) {
    int pass = 1;

    const uint32_t test0[NTT_SIZE] = {
        1, 2, 3, 4, 5, 6, 7, 8
    };

    const uint32_t test1[NTT_SIZE] = {
        100, 200, 300, 400, 500, 600, 700, 800
    };

    const uint32_t test2[NTT_SIZE] = {
        10000, 5000, 3000, 7000, 12288, 1, 4091, 7888
    };

    const uint32_t test3[NTT_SIZE] = {
        12288, 12287, 1, 2, 4091, 7888, 11060, 11208
    };

    printf("Falcon NTT8 DMA accelerator test\n");

#ifdef DMA_HW_FIFO_MODE
    printf("DMA_HW_FIFO_MODE enabled\n");
#else
    printf("DMA_HW_FIFO_MODE disabled\n");
#endif

    pass &= run_dma_ntt_test(test0);
    pass &= run_dma_ntt_test(test1);
    pass &= run_dma_ntt_test(test2);
    pass &= run_dma_ntt_test(test3);

    if (pass) {
        printf("Falcon NTT8 DMA accelerator test PASS\n");
        return 0;
    } else {
        printf("Falcon NTT8 DMA accelerator test FAIL\n");
        return 1;
    }
}
