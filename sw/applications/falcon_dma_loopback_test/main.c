#include <stdio.h>
#include <stdint.h>

#include "dma.h"
#include "hart.h"

#define TRANSACTION_SIZE 32u

static uint32_t input_data[TRANSACTION_SIZE];
static uint32_t output_data[TRANSACTION_SIZE];

int main(void) {
    int pass = 1;

    printf("Falcon DMA loopback test\n");

#ifdef DMA_HW_FIFO_MODE
    printf("DMA_HW_FIFO_MODE enabled\n");
#else
    printf("DMA_HW_FIFO_MODE disabled\n");
#endif

    for (uint32_t i = 0; i < TRANSACTION_SIZE; i++) {
        input_data[i] = i + 1u;
        output_data[i] = 0u;
    }

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
    trans.size_d1_du = TRANSACTION_SIZE;
    trans.dim = DMA_DIM_CONF_1D;
    trans.src_type = DMA_DATA_TYPE_WORD;
    trans.dst_type = DMA_DATA_TYPE_WORD;
    trans.mode = DMA_TRANS_MODE_SINGLE;
    trans.end = DMA_TRANS_END_INTR_WAIT;
    trans.channel = 0;

    /*
     * Force HW FIFO mode.
     *
     * If this does not compile, then this X-HEEP DMA driver does not have
     * the hw_fifo_en field and we need to port the DMA HW FIFO support from
     * the tutor's project.
     */
    trans.hw_fifo_en = 1;

    dma_config_flags_t res = dma_validate_transaction(
        &trans,
        DMA_ENABLE_REALIGN,
        DMA_PERFORM_CHECKS_INTEGRITY
    );

    printf("dma_validate_transaction result = 0x%08x\n", res);

    if (res & DMA_CONFIG_CRITICAL_ERROR) {
        printf("DMA validation failed\n");
        return 1;
    }

    res = dma_load_transaction(&trans);

    printf("dma_load_transaction result = 0x%08x\n", res);

    if (res & DMA_CONFIG_CRITICAL_ERROR) {
        printf("DMA load failed\n");
        return 1;
    }

    res = dma_launch(&trans);

    printf("dma_launch result = 0x%08x\n", res);

    if (res & DMA_CONFIG_CRITICAL_ERROR) {
        printf("DMA launch failed\n");
        return 1;
    }

    for (uint32_t i = 0; i < TRANSACTION_SIZE; i++) {
        printf("data[%u] in=%u out=%u\n", i, input_data[i], output_data[i]);

        if (output_data[i] != input_data[i]) {
            pass = 0;
        }
    }

    if (pass) {
        printf("Falcon DMA loopback test PASS\n");
        return 0;
    } else {
        printf("Falcon DMA loopback test FAIL\n");
        return 1;
    }
}