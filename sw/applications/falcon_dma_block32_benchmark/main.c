#include <stdio.h>
#include <stdint.h>

#include "dma.h"
#include "hart.h"

#define CH_NTT32   0u
#define CH_INTT32  1u

#define BLOCK_SIZE 32u
#define SIZE_512   512u
#define SIZE_1024  1024u
#define FALCON_Q   12289u

static uint32_t input_512[SIZE_512];
static uint32_t ntt_512[SIZE_512];
static uint32_t final_512[SIZE_512];

static uint32_t input_1024[SIZE_1024];
static uint32_t ntt_1024[SIZE_1024];
static uint32_t final_1024[SIZE_1024];

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

static void init_input(
    uint32_t *input,
    uint32_t *ntt,
    uint32_t *final,
    uint32_t size
) {
    for (uint32_t i = 0; i < size; i++) {
        input[i] = i % FALCON_Q;
        ntt[i] = 0u;
        final[i] = 0u;
    }
}

static int check_result(uint32_t *input, uint32_t *final, uint32_t size) {
    for (uint32_t i = 0; i < size; i++) {
        if (final[i] != input[i]) {
            printf("Mismatch at index %u: input=%u final=%u\n",
                   i,
                   input[i],
                   final[i]);
            return 0;
        }
    }

    return 1;
}

static void print_first_last_blocks(
    uint32_t *input,
    uint32_t *final,
    uint32_t size
) {
    printf("First block input/final:\n");

    for (uint32_t i = 0; i < BLOCK_SIZE; i++) {
        printf("a[%u] input=%u final=%u\n", i, input[i], final[i]);
    }

    printf("Last block input/final:\n");

    for (uint32_t i = size - BLOCK_SIZE; i < size; i++) {
        printf("a[%u] input=%u final=%u\n", i, input[i], final[i]);
    }
}

static int run_block32_benchmark(
    const char *name,
    uint32_t *input,
    uint32_t *ntt,
    uint32_t *final,
    uint32_t size
) {
    uint32_t blocks = size / BLOCK_SIZE;

    printf("==== %s NTT32/iNTT32 block benchmark ====\n", name);
    printf("Total coefficients = %u\n", size);
    printf("Block size = %u\n", BLOCK_SIZE);
    printf("Blocks = %u\n", blocks);
    printf("DMA transfers = %u\n", blocks * 2u);
    printf("DMA words transferred = %u\n", blocks * BLOCK_SIZE * 2u);

    init_input(input, ntt, final, size);

    for (uint32_t b = 0; b < blocks; b++) {
        uint32_t offset = b * BLOCK_SIZE;

        if (!run_dma_transfer(
                CH_NTT32,
                &input[offset],
                &ntt[offset],
                BLOCK_SIZE
            )) {
            printf("NTT32 DMA failed at block %u\n", b);
            return 0;
        }

        if (!run_dma_transfer(
                CH_INTT32,
                &ntt[offset],
                &final[offset],
                BLOCK_SIZE
            )) {
            printf("iNTT32 DMA failed at block %u\n", b);
            return 0;
        }
    }

    print_first_last_blocks(input, final, size);

    if (check_result(input, final, size)) {
        printf("%s NTT32/iNTT32 block benchmark PASS\n", name);
        return 1;
    } else {
        printf("%s NTT32/iNTT32 block benchmark FAIL\n", name);
        return 0;
    }
}

int main(void) {
    int pass = 1;

    printf("Falcon DMA block32 benchmark test\n");

#ifdef DMA_HW_FIFO_MODE
    printf("DMA_HW_FIFO_MODE enabled\n");
#else
    printf("DMA_HW_FIFO_MODE disabled\n");
#endif

    dma_init(NULL);

    pass &= run_block32_benchmark(
        "Falcon 512",
        input_512,
        ntt_512,
        final_512,
        SIZE_512
    );

    pass &= run_block32_benchmark(
        "Falcon 1024",
        input_1024,
        ntt_1024,
        final_1024,
        SIZE_1024
    );

    if (pass) {
        printf("Falcon DMA block32 benchmark test PASS\n");
        return 0;
    } else {
        printf("Falcon DMA block32 benchmark test FAIL\n");
        return 1;
    }
}
