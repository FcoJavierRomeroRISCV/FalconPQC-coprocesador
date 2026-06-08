#include <stdio.h>
#include <stdint.h>

#include "dma.h"
#include "hart.h"

#define FALCON_Q   12289u
#define FALCON_Q0I 12287u

#define NTT_SIZE 512u
#define CH_NTT   0u

static uint32_t input_data[NTT_SIZE];
static uint32_t output_data[NTT_SIZE];
static uint32_t ref_data[NTT_SIZE];

static const uint32_t GMb[512] = {
    4091u, 7888u, 11060u, 11208u, 6960u, 4342u, 6275u, 9759u,
    1591u, 6399u, 9477u, 5266u, 586u, 5825u, 7538u, 9710u,
    1134u, 6407u, 1711u, 965u, 7099u, 7674u, 3743u, 6442u,
    10414u, 8100u, 1885u, 1688u, 1364u, 10329u, 10164u, 9180u,
    12210u, 6240u, 997u, 117u, 4783u, 4407u, 1549u, 7072u,
    2829u, 6458u, 4431u, 8877u, 7144u, 2564u, 5664u, 4042u,
    12189u, 432u, 10751u, 1237u, 7610u, 1534u, 3983u, 7863u,
    2181u, 6308u, 8720u, 6570u, 4843u, 1690u, 14u, 3872u,
    5569u, 9368u, 12163u, 2019u, 7543u, 2315u, 4673u, 7340u,
    1553u, 1156u, 8401u, 11389u, 1020u, 2967u, 10772u, 7045u,
    3316u, 11236u, 5285u, 11578u, 10637u, 10086u, 9493u, 6180u,
    9277u, 6130u, 3323u, 883u, 10469u, 489u, 1502u, 2851u,
    11061u, 9729u, 2742u, 12241u, 4970u, 10481u, 10078u, 1195u,
    730u, 1762u, 3854u, 2030u, 5892u, 10922u, 9020u, 5274u,
    9179u, 3604u, 3782u, 10206u, 3180u, 3467u, 4668u, 2446u,
    7613u, 9386u, 834u, 7703u, 6836u, 3403u, 5351u, 12276u,
    3580u, 1739u, 10820u, 9787u, 10209u, 4070u, 12250u, 8525u,
    10401u, 2749u, 7338u, 10574u, 6040u, 943u, 9330u, 1477u,
    6865u, 9668u, 3585u, 6633u, 12145u, 4063u, 3684u, 7680u,
    8188u, 6902u, 3533u, 9807u, 6090u, 727u, 10099u, 7003u,
    6945u, 1949u, 9731u, 10559u, 6057u, 378u, 7871u, 8763u,
    8901u, 9229u, 8846u, 4551u, 9589u, 11664u, 7630u, 8821u,
    5680u, 4956u, 6251u, 8388u, 10156u, 8723u, 2341u, 3159u,
    1467u, 5460u, 8553u, 7783u, 2649u, 2320u, 9036u, 6188u,
    737u, 3698u, 4699u, 5753u, 9046u, 3687u, 16u, 914u,
    5186u, 10531u, 4552u, 1964u, 3509u, 8436u, 7516u, 5381u,
    10733u, 3281u, 7037u, 1060u, 2895u, 7156u, 8887u, 5357u,
    6409u, 8197u, 2962u, 6375u, 5064u, 6634u, 5625u, 278u,
    932u, 10229u, 8927u, 7642u, 351u, 9298u, 237u, 5858u,
    7692u, 3146u, 12126u, 7586u, 2053u, 11285u, 3802u, 5204u,
    4602u, 1748u, 11300u, 340u, 3711u, 4614u, 300u, 10993u,
    5070u, 10049u, 11616u, 12247u, 7421u, 10707u, 5746u, 5654u,
    3835u, 5553u, 1224u, 8476u, 9237u, 3845u, 250u, 11209u,
    4225u, 6326u, 9680u, 12254u, 4136u, 2778u, 692u, 8808u,
    6410u, 6718u, 10105u, 10418u, 3759u, 7356u, 11361u, 8433u,
    6437u, 3652u, 6342u, 8978u, 5391u, 2272u, 6476u, 7416u,
    8418u, 10824u, 11986u, 5733u, 876u, 7030u, 2167u, 2436u,
    3442u, 9217u, 8206u, 4858u, 5964u, 2746u, 7178u, 1434u,
    7389u, 8879u, 10661u, 11457u, 4220u, 1432u, 10832u, 4328u,
    8557u, 1867u, 9454u, 2416u, 3816u, 9076u, 686u, 5393u,
    2523u, 4339u, 6115u, 619u, 937u, 2834u, 7775u, 3279u,
    2363u, 7488u, 6112u, 5056u, 824u, 10204u, 11690u, 1113u,
    2727u, 9848u, 896u, 2028u, 5075u, 2654u, 10464u, 7884u,
    12169u, 5434u, 3070u, 6400u, 9132u, 11672u, 12153u, 4520u,
    1273u, 9739u, 11468u, 9937u, 10039u, 9720u, 2262u, 9399u,
    11192u, 315u, 4511u, 1158u, 6061u, 6751u, 11865u, 357u,
    7367u, 4550u, 983u, 8534u, 8352u, 10126u, 7530u, 9253u,
    4367u, 5221u, 3999u, 8777u, 3161u, 6990u, 4130u, 11652u,
    3374u, 11477u, 1753u, 292u, 8681u, 2806u, 10378u, 12188u,
    5800u, 11811u, 3181u, 1988u, 1024u, 9340u, 2477u, 10928u,
    4582u, 6750u, 3619u, 5503u, 5233u, 2463u, 8470u, 7650u,
    7964u, 6395u, 1071u, 1272u, 3474u, 11045u, 3291u, 11344u,
    8502u, 9478u, 9837u, 1253u, 1857u, 6233u, 4720u, 11561u,
    6034u, 9817u, 3339u, 1797u, 2879u, 6242u, 5200u, 2114u,
    7962u, 9353u, 11363u, 5475u, 6084u, 9601u, 4108u, 7323u,
    10438u, 9471u, 1271u, 408u, 6911u, 3079u, 360u, 8276u,
    11535u, 9156u, 9049u, 11539u, 850u, 8617u, 784u, 7919u,
    8334u, 12170u, 1846u, 10213u, 12184u, 7827u, 11903u, 5600u,
    9779u, 1012u, 721u, 2784u, 6676u, 6552u, 5348u, 4424u,
    6816u, 8405u, 9959u, 5150u, 2356u, 5552u, 5267u, 1333u,
    8801u, 9661u, 7308u, 5788u, 4910u, 909u, 11613u, 4395u,
    8238u, 6686u, 4302u, 3044u, 2285u, 12249u, 1963u, 9216u,
    4296u, 11918u, 695u, 4371u, 9793u, 4884u, 2411u, 10230u,
    2650u, 841u, 3890u, 10231u, 7248u, 8505u, 11196u, 6688u
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

static void ntt512_ref(uint32_t a[NTT_SIZE]) {
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

static void make_test_vector(uint32_t test_id) {
    for (uint32_t i = 0; i < NTT_SIZE; i++) {
        if (test_id == 0u) {
            input_data[i] = i;
        } else if (test_id == 1u) {
            input_data[i] = ((i + 1u) * 100u) % FALCON_Q;
        } else if (test_id == 2u) {
            input_data[i] = GMb[i];
        } else {
            input_data[i] = (FALCON_Q - 1u - i) % FALCON_Q;
        }

        output_data[i] = 0u;
        ref_data[i] = input_data[i];
    }
}

static void print_first_last(const char *name, uint32_t v[NTT_SIZE]) {
    printf("%s first 8: [", name);

    for (uint32_t i = 0; i < 8u; i++) {
        printf("%u", v[i]);

        if (i != 7u) {
            printf(", ");
        }
    }

    printf("]\n");

    printf("%s last 8:  [", name);

    for (uint32_t i = NTT_SIZE - 8u; i < NTT_SIZE; i++) {
        printf("%u", v[i]);

        if (i != NTT_SIZE - 1u) {
            printf(", ");
        }
    }

    printf("]\n");
}

static int run_dma_ntt512_test(uint32_t test_id) {
    int pass = 1;

    make_test_vector(test_id);
    ntt512_ref(ref_data);

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

    printf("Running NTT512 test %u\n", test_id);
    print_first_last("Input ", input_data);
    print_first_last("HW out", output_data);
    print_first_last("REF   ", ref_data);

    for (uint32_t i = 0; i < NTT_SIZE; i++) {
        if (output_data[i] != ref_data[i]) {
            printf("Mismatch at index %u: hw=%u ref=%u\n",
                   i,
                   output_data[i],
                   ref_data[i]);
            pass = 0;
        }
    }

    if (pass) {
        printf("NTT512 DMA Test PASS\n");
    } else {
        printf("NTT512 DMA Test FAIL\n");
    }

    return pass;
}

int main(void) {
    int pass = 1;

    printf("Falcon NTT512 DMA accelerator test\n");

#ifdef DMA_HW_FIFO_MODE
    printf("DMA_HW_FIFO_MODE enabled\n");
#else
    printf("DMA_HW_FIFO_MODE disabled\n");
#endif

    dma_init(NULL);

    pass &= run_dma_ntt512_test(0u);
    pass &= run_dma_ntt512_test(1u);
    pass &= run_dma_ntt512_test(2u);
    pass &= run_dma_ntt512_test(3u);

    if (pass) {
        printf("Falcon NTT512 DMA accelerator test PASS\n");
        return 0;
    } else {
        printf("Falcon NTT512 DMA accelerator test FAIL\n");
        return 1;
    }
}
