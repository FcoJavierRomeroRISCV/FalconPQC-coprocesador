#include <stdio.h>
#include <stdint.h>

#include "dma.h"
#include "hart.h"

#define FALCON_Q   12289u
#define FALCON_Q0I 12287u

#define INTT_SIZE 256u
#define CH_INTT   0u

static uint32_t input_data[INTT_SIZE];
static uint32_t output_data[INTT_SIZE];
static uint32_t ref_data[INTT_SIZE];

static const uint32_t iGMb[256] = {
    4091u, 4401u, 1081u, 1229u, 2530u, 6014u, 7947u, 5329u,
    2579u, 4751u, 6464u, 11703u, 7023u, 2812u, 5890u, 10698u,
    3109u, 2125u, 1960u, 10925u, 10601u, 10404u, 4189u, 1875u,
    5847u, 8546u, 4615u, 5190u, 11324u, 10578u, 5882u, 11155u,
    8417u, 12275u, 10599u, 7446u, 5719u, 3569u, 5981u, 10108u,
    4426u, 8306u, 10755u, 4679u, 11052u, 1538u, 11857u, 100u,
    8247u, 6625u, 9725u, 5145u, 3412u, 7858u, 5831u, 9460u,
    5217u, 10740u, 7882u, 7506u, 12172u, 11292u, 6049u, 79u,
    13u, 6938u, 8886u, 5453u, 4586u, 11455u, 2903u, 4676u,
    9843u, 7621u, 8822u, 9109u, 2083u, 8507u, 8685u, 3110u,
    7015u, 3269u, 1367u, 6397u, 10259u, 8435u, 10527u, 11559u,
    11094u, 2211u, 1808u, 7319u, 48u, 9547u, 2560u, 1228u,
    9438u, 10787u, 11800u, 1820u, 11406u, 8966u, 6159u, 3012u,
    6109u, 2796u, 2203u, 1652u, 711u, 7004u, 1053u, 8973u,
    5244u, 1517u, 9322u, 11269u, 900u, 3888u, 11133u, 10736u,
    4949u, 7616u, 9974u, 4746u, 10270u, 126u, 2921u, 6720u,
    6635u, 6543u, 1582u, 4868u, 42u, 673u, 2240u, 7219u,
    1296u, 11989u, 7675u, 8578u, 11949u, 989u, 10541u, 7687u,
    7085u, 8487u, 1004u, 10236u, 4703u, 163u, 9143u, 4597u,
    6431u, 12052u, 2991u, 11938u, 4647u, 3362u, 2060u, 11357u,
    12011u, 6664u, 5655u, 7225u, 5914u, 9327u, 4092u, 5880u,
    6932u, 3402u, 5133u, 9394u, 11229u, 5252u, 9008u, 1556u,
    6908u, 4773u, 3853u, 8780u, 10325u, 7737u, 1758u, 7103u,
    11375u, 12273u, 8602u, 3243u, 6536u, 7590u, 8591u, 11552u,
    6101u, 3253u, 9969u, 9640u, 4506u, 3736u, 6829u, 10822u,
    9130u, 9948u, 3566u, 2133u, 3901u, 6038u, 7333u, 6609u,
    3468u, 4659u, 625u, 2700u, 7738u, 3443u, 3060u, 3388u,
    3526u, 4418u, 11911u, 6232u, 1730u, 2558u, 10340u, 5344u,
    5286u, 2190u, 11562u, 6199u, 2482u, 8756u, 5387u, 4101u,
    4609u, 8605u, 8226u, 144u, 5656u, 8704u, 2621u, 5424u,
    10812u, 2959u, 11346u, 6249u, 1715u, 4951u, 9540u, 1888u,
    3764u, 39u, 8219u, 2080u, 2502u, 1469u, 10550u, 8709u
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

static void intt256_ref(uint32_t a[INTT_SIZE]) {
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
        a[i] = montgomery_ref(a[i], 256u);
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

static int run_dma_intt256_test(uint32_t test_id) {
    int pass = 1;

    make_test_vector(test_id);
    intt256_ref(ref_data);

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

    printf("Running iNTT256 test %u\n", test_id);
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
        printf("iNTT256 DMA Test PASS\n");
    } else {
        printf("iNTT256 DMA Test FAIL\n");
    }

    return pass;
}

int main(void) {
    int pass = 1;

    printf("Falcon iNTT256 DMA accelerator test\n");

#ifdef DMA_HW_FIFO_MODE
    printf("DMA_HW_FIFO_MODE enabled\n");
#else
    printf("DMA_HW_FIFO_MODE disabled\n");
#endif

    dma_init(NULL);

    pass &= run_dma_intt256_test(0u);
    pass &= run_dma_intt256_test(1u);
    pass &= run_dma_intt256_test(2u);
    pass &= run_dma_intt256_test(3u);

    if (pass) {
        printf("Falcon iNTT256 DMA accelerator test PASS\n");
        return 0;
    } else {
        printf("Falcon iNTT256 DMA accelerator test FAIL\n");
        return 1;
    }
}
