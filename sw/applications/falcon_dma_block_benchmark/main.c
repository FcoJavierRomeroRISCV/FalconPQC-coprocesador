#include <stdio.h>
#include <stdint.h>

#include "dma.h"
#include "hart.h"

#define CH_NTT   0u
#define CH_INTT  1u

#define BLOCK_SIZE 8u
#define SIZE_512   512u
#define SIZE_1024  1024u

static uint32_t input_512[SIZE_512];
static uint32_t ntt_512[SIZE_512];
static uint32_t final_512[SIZE_512];

static uint32_t input_1024[SIZE_1024];
static uint32_t ntt_1024[SIZE_1024];
static uint32_t final_1024[SIZE_1024];

static inline uint64_t read_mcycle(void) {
    uint32_t hi0;
    uint32_t lo;
    uint32_t hi1;

    /*
     * RV32 safe read of mcycle.
     * Read high-low-high and repeat if high changed.
     */
    do {
        __asm__ volatile("csrr %0, mcycleh" : "=r"(hi0));
        __asm__ volatile("csrr %0, mcycle"  : "=r"(lo));
        __asm__ volatile("csrr %0, mcycleh" : "=r"(hi1));
    } while (hi0 != hi1);

    return (((uint64_t)hi1) << 32) | lo;
}

static int run_dma_transfer_measured(
    uint32_t channel,
    uint32_t *src,
    uint32_t *dst,
    uint32_t size,
    uint64_t *validate_cycles,
    uint64_t *load_cycles,
    uint64_t *launch_cycles,
    uint64_t *total_cycles
) {
    static dma_target_t src_target;
    static dma_target_t dst_target;
    static dma_trans_t trans;

    uint64_t t0;
    uint64_t t1;

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

    uint64_t total_start = read_mcycle();

    t0 = read_mcycle();
    dma_config_flags_t res = dma_validate_transaction(
        &trans,
        DMA_ENABLE_REALIGN,
        DMA_PERFORM_CHECKS_INTEGRITY
    );
    t1 = read_mcycle();
    *validate_cycles += (t1 - t0);

    if (res & DMA_CONFIG_CRITICAL_ERROR) {
        printf("DMA validation failed on channel %u, res=0x%08x\n", channel, res);
        return 0;
    }

    t0 = read_mcycle();
    res = dma_load_transaction(&trans);
    t1 = read_mcycle();
    *load_cycles += (t1 - t0);

    if (res & DMA_CONFIG_CRITICAL_ERROR) {
        printf("DMA load failed on channel %u, res=0x%08x\n", channel, res);
        return 0;
    }

    /*
     * dma_launch() is the most important measurement here because with
     * DMA_TRANS_END_INTR_WAIT it includes the wait until the DMA transfer ends.
     */
    t0 = read_mcycle();
    res = dma_launch(&trans);
    t1 = read_mcycle();
    *launch_cycles += (t1 - t0);

    if (res & DMA_CONFIG_CRITICAL_ERROR) {
        printf("DMA launch failed on channel %u, res=0x%08x\n", channel, res);
        return 0;
    }

    uint64_t total_end = read_mcycle();
    *total_cycles += (total_end - total_start);

    return 1;
}

static void init_input(uint32_t *input, uint32_t *ntt, uint32_t *final, uint32_t size) {
    for (uint32_t i = 0; i < size; i++) {
        input[i] = i % 12289u;
        ntt[i] = 0u;
        final[i] = 0u;
    }
}

static int check_result(uint32_t *input, uint32_t *final, uint32_t size) {
    int pass = 1;

    for (uint32_t i = 0; i < size; i++) {
        if (final[i] != input[i]) {
            printf("Mismatch at index %u: input=%u final=%u\n", i, input[i], final[i]);
            pass = 0;
            break;
        }
    }

    return pass;
}

static void print_cycles_u64(const char *name, uint64_t value) {
    printf("%s = %u%09u\n",
           name,
           (uint32_t)(value / 1000000000ULL),
           (uint32_t)(value % 1000000000ULL));
}

static uint32_t div_u64_u32(uint64_t value, uint32_t div) {
    if (div == 0u) {
        return 0u;
    }

    return (uint32_t)(value / div);
}

static int run_block_chain_benchmark(
    const char *name,
    uint32_t *input,
    uint32_t *ntt,
    uint32_t *final,
    uint32_t size
) {
    uint32_t blocks = size / BLOCK_SIZE;
    int pass = 1;

    uint64_t benchmark_start;
    uint64_t benchmark_end;

    uint64_t init_start;
    uint64_t init_end;

    uint64_t check_start;
    uint64_t check_end;

    uint64_t ntt_validate_cycles = 0;
    uint64_t ntt_load_cycles = 0;
    uint64_t ntt_launch_cycles = 0;
    uint64_t ntt_total_cycles = 0;

    uint64_t intt_validate_cycles = 0;
    uint64_t intt_load_cycles = 0;
    uint64_t intt_launch_cycles = 0;
    uint64_t intt_total_cycles = 0;

    uint64_t all_transfers_start;
    uint64_t all_transfers_end;

    printf("==== %s block DMA benchmark ====\n", name);
    printf("Total coefficients = %u\n", size);
    printf("Block size = %u\n", BLOCK_SIZE);
    printf("Blocks = %u\n", blocks);
    printf("DMA transfers = %u\n", blocks * 2u);
    printf("DMA words transferred = %u\n", blocks * BLOCK_SIZE * 2u);

    benchmark_start = read_mcycle();

    init_start = read_mcycle();
    init_input(input, ntt, final, size);
    init_end = read_mcycle();

    all_transfers_start = read_mcycle();

    for (uint32_t b = 0; b < blocks; b++) {
        uint32_t offset = b * BLOCK_SIZE;

        if (!run_dma_transfer_measured(
                CH_NTT,
                &input[offset],
                &ntt[offset],
                BLOCK_SIZE,
                &ntt_validate_cycles,
                &ntt_load_cycles,
                &ntt_launch_cycles,
                &ntt_total_cycles
            )) {
            printf("NTT DMA failed at block %u\n", b);
            return 0;
        }

        if (!run_dma_transfer_measured(
                CH_INTT,
                &ntt[offset],
                &final[offset],
                BLOCK_SIZE,
                &intt_validate_cycles,
                &intt_load_cycles,
                &intt_launch_cycles,
                &intt_total_cycles
            )) {
            printf("iNTT DMA failed at block %u\n", b);
            return 0;
        }
    }

    all_transfers_end = read_mcycle();

    check_start = read_mcycle();
    pass = check_result(input, final, size);
    check_end = read_mcycle();

    benchmark_end = read_mcycle();

    printf("First block input/final:\n");
    for (uint32_t i = 0; i < BLOCK_SIZE; i++) {
        printf("a[%u] input=%u final=%u\n", i, input[i], final[i]);
    }

    printf("Last block input/final:\n");
    for (uint32_t i = size - BLOCK_SIZE; i < size; i++) {
        printf("a[%u] input=%u final=%u\n", i, input[i], final[i]);
    }

    printf("---- Cycle measurements for %s ----\n", name);

    print_cycles_u64("Init cycles", init_end - init_start);

    print_cycles_u64("NTT validate cycles total", ntt_validate_cycles);
    print_cycles_u64("NTT load cycles total", ntt_load_cycles);
    print_cycles_u64("NTT launch/wait cycles total", ntt_launch_cycles);
    print_cycles_u64("NTT transfer cycles total", ntt_total_cycles);

    printf("NTT transfer cycles per block = %u\n",
           div_u64_u32(ntt_total_cycles, blocks));

    printf("NTT launch/wait cycles per block = %u\n",
           div_u64_u32(ntt_launch_cycles, blocks));

    print_cycles_u64("iNTT validate cycles total", intt_validate_cycles);
    print_cycles_u64("iNTT load cycles total", intt_load_cycles);
    print_cycles_u64("iNTT launch/wait cycles total", intt_launch_cycles);
    print_cycles_u64("iNTT transfer cycles total", intt_total_cycles);

    printf("iNTT transfer cycles per block = %u\n",
           div_u64_u32(intt_total_cycles, blocks));

    printf("iNTT launch/wait cycles per block = %u\n",
           div_u64_u32(intt_launch_cycles, blocks));

    print_cycles_u64("All DMA transfer cycles", all_transfers_end - all_transfers_start);
    print_cycles_u64("Check cycles", check_end - check_start);
    print_cycles_u64("Benchmark total cycles", benchmark_end - benchmark_start);

    printf("Cycles per coefficient, total x1000 = %u\n",
           div_u64_u32((benchmark_end - benchmark_start) * 1000ULL, size));

    printf("DMA transfer cycles per coefficient x1000 = %u\n",
           div_u64_u32((all_transfers_end - all_transfers_start) * 1000ULL, size));

    if (pass) {
        printf("%s block DMA benchmark PASS\n", name);
    } else {
        printf("%s block DMA benchmark FAIL\n", name);
    }

    return pass;
}

int main(void) {
    int pass = 1;

    printf("Falcon DMA block benchmark test\n");

#ifdef DMA_HW_FIFO_MODE
    printf("DMA_HW_FIFO_MODE enabled\n");
#else
    printf("DMA_HW_FIFO_MODE disabled\n");
#endif

    dma_init(NULL);

    pass &= run_block_chain_benchmark(
        "Falcon 512",
        input_512,
        ntt_512,
        final_512,
        SIZE_512
    );

    pass &= run_block_chain_benchmark(
        "Falcon 1024",
        input_1024,
        ntt_1024,
        final_1024,
        SIZE_1024
    );

    if (pass) {
        printf("Falcon DMA block benchmark test PASS\n");
        return 0;
    } else {
        printf("Falcon DMA block benchmark test FAIL\n");
        return 1;
    }
}
