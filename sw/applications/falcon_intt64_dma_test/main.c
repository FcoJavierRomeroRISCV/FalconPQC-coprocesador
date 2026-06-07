#include <stdio.h>
#include <stdint.h>

#include "dma.h"
#include "hart.h"

#define FALCON_Q   12289u
#define FALCON_Q0I 12287u

#define INTT_SIZE 64u
#define CH_INTT   0u

static uint32_t input_data[INTT_SIZE];
static uint32_t output_data[INTT_SIZE];
static uint32_t ref_data[INTT_SIZE];

static const uint32_t iGMb[64] = {
    4091u, 4401u, 1081u, 1229u, 2530u, 6014u, 7947u, 5329u,
    2579u, 4751u, 6464u, 11703u, 7023u, 2812u, 5890u, 10698u,
    3109u, 2125u, 1960u, 10925u, 10601u, 10404u, 4189u, 1875u,
    5847u, 8546u, 4615u, 5190u, 11324u, 10578u, 5882u, 11155u,
    8417u, 12275u, 10599u, 7446u, 5719u, 3569u, 5981u, 10108u,
    4426u, 8226u, 2392u, 8761u, 9680u, 5003u, 4394u, 11981u,
    10275u, 4730u, 8194u, 6008u, 3006u, 131u, 241u, 6309u,
    8011u, 11414u, 2766u, 11337u, 834u, 3725u, 11336u, 10276u
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

static void intt_butterfly_ref(uint32_t *x0, uint32_t *x1, uint32_t s) {
    uint32_t u = *x0;
    uint32_t v = *x1;

    *x0 = mod_add_ref(u, v);
    *x1 = montgomery_ref(mod_sub_ref(u, v), s);
}

static void intt64_ref(uint32_t a[INTT_SIZE]) {
    uint32_t t = 1u;

    for (uint32_t m = INTT_SIZE; m > 1u; m >>= 1) {
        uint32_t hm = m >> 1;

        for (uint32_t i = 0; i < hm; i++) {
            uint32_t j1 = i * (t << 1);
            uint32_t j2 = j1 + t;
            uint32_t s = iGMb[hm + i];

            for (uint32_t j = j1; j < j2; j++) {
                intt_butterfly_ref(&a[j], &a[j + t], s);
            }
        }

        t <<= 1;
    }

    for (uint32_t i = 0; i < INTT_SIZE; i++) {
        a[i] = montgomery_ref(a[i], 1024u);
    }
}

static void make_test_vector(uint32_t test_id) {
    for (uint32_t i = 0; i < INTT_SIZE; i++) {
        if (test_id == 0u) {
            input_data[i] = i;
        } else if (test_id == 1u) {
            input_data[i] = ((i + 1u) * 100u) % FALCON_Q;
        } else if (test_id == 2u) {
            input_data[i] = iGMb[i];
        } else {
            input_data[i] = (FALCON_Q - 1u - i) % FALCON_Q;
        }

        output_data[i] = 0u;
        ref_data[i] = input_data[i];
    }
}

static void print_first_last(const char *name, uint32_t v[INTT_SIZE]) {
    printf("%s first 8: [", name);

    for (uint32_t i = 0; i < 8u; i++) {
        printf("%u", v[i]);
        if (i != 7u) {
            printf(", ");
        }
    }

    printf("]\n");

    printf("%s last 8:  [", name);

    for (uint32_t i = INTT_SIZE - 8u; i < INTT_SIZE; i++) {
        printf("%u", v[i]);
        if (i != INTT_SIZE - 1u) {
            printf(", ");
        }
    }

    printf("]\n");
}

static int run_dma_intt64_test(uint32_t test_id) {
    int pass = 1;

    make_test_vector(test_id);
    intt64_ref(ref_data);

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

    printf("Running iNTT64 test %u\n", test_id);
    print_first_last("Input ", input_data);
    print_first_last("HW out", output_data);
    print_first_last("REF   ", ref_data);

    for (uint32_t i = 0; i < INTT_SIZE; i++) {
        if (output_data[i] != ref_data[i]) {
            printf("Mismatch at index %u: hw=%u ref=%u\n",
                   i,
                   output_data[i],
                   ref_data[i]);
            pass = 0;
        }
    }

    if (pass) {
        printf("iNTT64 DMA Test PASS\n");
    } else {
        printf("iNTT64 DMA Test FAIL\n");
    }

    return pass;
}

int main(void) {
    int pass = 1;

    printf("Falcon iNTT64 DMA accelerator test\n");

#ifdef DMA_HW_FIFO_MODE
    printf("DMA_HW_FIFO_MODE enabled\n");
#else
    printf("DMA_HW_FIFO_MODE disabled\n");
#endif

    dma_init(NULL);

    pass &= run_dma_intt64_test(0u);
    pass &= run_dma_intt64_test(1u);
    pass &= run_dma_intt64_test(2u);
    pass &= run_dma_intt64_test(3u);

    if (pass) {
        printf("Falcon iNTT64 DMA accelerator test PASS\n");
        return 0;
    } else {
        printf("Falcon iNTT64 DMA accelerator test FAIL\n");
        return 1;
    }
}
