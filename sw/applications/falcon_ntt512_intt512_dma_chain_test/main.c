#include <stdio.h>
#include <stdint.h>

#include "dma.h"
#include "hart.h"

#define CH_NTT512   0u
#define CH_INTT512  1u

#define VEC_SIZE 512u
#define FALCON_Q  12289u

static uint32_t input_data[VEC_SIZE];
static uint32_t ntt_data[VEC_SIZE];
static uint32_t final_data[VEC_SIZE];

static void make_test_vector(uint32_t test_id) {
    for (uint32_t i = 0; i < VEC_SIZE; i++) {
        if (test_id == 0u) {
            input_data[i] = i;
        } else if (test_id == 1u) {
            input_data[i] = ((i + 1u) * 100u) % FALCON_Q;
        } else if (test_id == 2u) {
            input_data[i] = ((i * 17u) + 3u) % FALCON_Q;
        } else {
            input_data[i] = (FALCON_Q - 1u - i) % FALCON_Q;
        }

        ntt_data[i] = 0u;
        final_data[i] = 0u;
    }
}

static void print_first_last(const char *name, uint32_t v[VEC_SIZE]) {
    printf("%s first 8: [", name);

    for (uint32_t i = 0; i < 8u; i++) {
        printf("%u", v[i]);

        if (i != 7u) {
            printf(", ");
        }
    }

    printf("]\n");

    printf("%s last 8:  [", name);

    for (uint32_t i = VEC_SIZE - 8u; i < VEC_SIZE; i++) {
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

static int run_chain_test(uint32_t test_id) {
    int pass = 1;

    make_test_vector(test_id);

    printf("Running NTT512 -> iNTT512 chain test %u\n", test_id);

    print_first_last("Input  ", input_data);

    if (!run_dma_transfer(CH_NTT512, input_data, ntt_data, VEC_SIZE)) {
        printf("NTT512 DMA transfer failed\n");
        return 0;
    }

    print_first_last("NTT512 ", ntt_data);

    if (!run_dma_transfer(CH_INTT512, ntt_data, final_data, VEC_SIZE)) {
        printf("iNTT512 DMA transfer failed\n");
        return 0;
    }

    print_first_last("Final  ", final_data);

    for (uint32_t i = 0; i < VEC_SIZE; i++) {
        if (final_data[i] != input_data[i]) {
            printf("Mismatch at index %u: input=%u final=%u\n",
                   i,
                   input_data[i],
                   final_data[i]);
            pass = 0;
        }
    }

    if (pass) {
        printf("NTT512 -> iNTT512 DMA chain Test PASS\n");
    } else {
        printf("NTT512 -> iNTT512 DMA chain Test FAIL\n");
    }

    return pass;
}

int main(void) {
    int pass = 1;

    printf("Falcon NTT512 -> iNTT512 DMA chain test\n");

#ifdef DMA_HW_FIFO_MODE
    printf("DMA_HW_FIFO_MODE enabled\n");
#else
    printf("DMA_HW_FIFO_MODE disabled\n");
#endif

    dma_init(NULL);

    pass &= run_chain_test(0u);
    pass &= run_chain_test(1u);
    pass &= run_chain_test(2u);
    pass &= run_chain_test(3u);

    if (pass) {
        printf("Falcon NTT512 -> iNTT512 DMA chain test PASS\n");
        return 0;
    } else {
        printf("Falcon NTT512 -> iNTT512 DMA chain test FAIL\n");
        return 1;
    }
}
