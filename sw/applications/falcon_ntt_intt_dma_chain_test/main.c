#include <stdio.h>
#include <stdint.h>

#include "dma.h"
#include "hart.h"

#define CH_NTT   0u
#define CH_INTT  1u

#define VEC_SIZE 8u

static uint32_t input_data[VEC_SIZE];
static uint32_t ntt_data[VEC_SIZE];
static uint32_t final_data[VEC_SIZE];

static void print_vector(const char *name, uint32_t v[VEC_SIZE]) {
    printf("%s[", name);

    for (uint32_t i = 0; i < VEC_SIZE; i++) {
        printf("%u", v[i]);

        if (i != VEC_SIZE - 1u) {
            printf(", ");
        }
    }

    printf("]\n");
}

static int run_dma_transfer(
    uint32_t channel,
    uint32_t *src,
    uint32_t *dst,
    uint32_t size
) {
    static dma_target_t src_target;
    static dma_target_t dst_target;
    static dma_trans_t trans;

    src_target = (dma_target_t){0};
    dst_target = (dma_target_t){0};
    trans = (dma_trans_t){0};

    src_target.ptr = (uint8_t *)src;
    src_target.inc_d1_du = 1;
    src_target.type = DMA_DATA_TYPE_WORD;
    src_target.trig = DMA_TRIG_MEMORY;

    dst_target.ptr = (uint8_t *)dst;
    dst_target.inc_d1_du = 1;
    dst_target.type = DMA_DATA_TYPE_WORD;
    dst_target.trig = DMA_TRIG_MEMORY;

    trans.src = &src_target;
    trans.dst = &dst_target;
    trans.size_d1_du = size;
    trans.dim = DMA_DIM_CONF_1D;
    trans.src_type = DMA_DATA_TYPE_WORD;
    trans.dst_type = DMA_DATA_TYPE_WORD;
    trans.mode = DMA_TRANS_MODE_SINGLE;
    trans.end = DMA_TRANS_END_INTR_WAIT;
    trans.channel = channel;
    trans.hw_fifo_en = 1;

    dma_config_flags_t res = dma_validate_transaction(
        &trans,
        DMA_ENABLE_REALIGN,
        DMA_PERFORM_CHECKS_INTEGRITY
    );

    if (res & DMA_CONFIG_CRITICAL_ERROR) {
        printf("DMA validation failed on channel %u, res=0x%08x\n", channel, res);
        return 0;
    }

    res = dma_load_transaction(&trans);

    if (res & DMA_CONFIG_CRITICAL_ERROR) {
        printf("DMA load failed on channel %u, res=0x%08x\n", channel, res);
        return 0;
    }

    res = dma_launch(&trans);

    if (res & DMA_CONFIG_CRITICAL_ERROR) {
        printf("DMA launch failed on channel %u, res=0x%08x\n", channel, res);
        return 0;
    }

    return 1;
}

static int run_chain_test(const uint32_t test_input[VEC_SIZE]) {
    int pass = 1;

    for (uint32_t i = 0; i < VEC_SIZE; i++) {
        input_data[i] = test_input[i];
        ntt_data[i] = 0u;
        final_data[i] = 0u;
    }

    printf("Running chain test\n");
    print_vector("Input:  ", input_data);

    if (!run_dma_transfer(CH_NTT, input_data, ntt_data, VEC_SIZE)) {
        printf("NTT DMA transfer failed\n");
        return 0;
    }

    print_vector("NTT:    ", ntt_data);

    if (!run_dma_transfer(CH_INTT, ntt_data, final_data, VEC_SIZE)) {
        printf("iNTT DMA transfer failed\n");
        return 0;
    }

    print_vector("Final:  ", final_data);

    for (uint32_t i = 0; i < VEC_SIZE; i++) {
        printf("a[%u] input=%u final=%u\n", i, input_data[i], final_data[i]);

        if (final_data[i] != input_data[i]) {
            pass = 0;
        }
    }

    if (pass) {
        printf("NTT8 -> iNTT8 DMA chain Test PASS\n");
    } else {
        printf("NTT8 -> iNTT8 DMA chain Test FAIL\n");
    }

    return pass;
}

int main(void) {
    int pass = 1;

    const uint32_t test0[VEC_SIZE] = {
        1, 2, 3, 4, 5, 6, 7, 8
    };

    const uint32_t test1[VEC_SIZE] = {
        100, 200, 300, 400, 500, 600, 700, 800
    };

    const uint32_t test2[VEC_SIZE] = {
        10000, 5000, 3000, 7000, 12288, 1, 4091, 7888
    };

    const uint32_t test3[VEC_SIZE] = {
        12288, 12287, 1, 2, 4091, 7888, 11060, 11208
    };

    printf("Falcon NTT8 -> iNTT8 DMA chain test\n");

#ifdef DMA_HW_FIFO_MODE
    printf("DMA_HW_FIFO_MODE enabled\n");
#else
    printf("DMA_HW_FIFO_MODE disabled\n");
#endif

    dma_init(NULL);

    pass &= run_chain_test(test0);
    pass &= run_chain_test(test1);
    pass &= run_chain_test(test2);
    pass &= run_chain_test(test3);

    if (pass) {
        printf("Falcon NTT8 -> iNTT8 DMA chain test PASS\n");
        return 0;
    } else {
        printf("Falcon NTT8 -> iNTT8 DMA chain test FAIL\n");
        return 1;
    }
}
