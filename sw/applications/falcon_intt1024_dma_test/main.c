#include <stdio.h>
#include <stdint.h>

#include "dma.h"
#include "hart.h"

#define FALCON_Q   12289u
#define FALCON_Q0I 12287u

#define INTT_SIZE 1024u
#define CH_INTT   0u

static uint32_t input_data[INTT_SIZE];
static uint32_t output_data[INTT_SIZE];
static uint32_t ref_data[INTT_SIZE];

static const uint32_t iGMb[1024] = {
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
    3764u, 39u, 8219u, 2080u, 2502u, 1469u, 10550u, 8709u,
    5601u, 1093u, 3784u, 5041u, 2058u, 8399u, 11448u, 9639u,
    2059u, 9878u, 7405u, 2496u, 7918u, 11594u, 371u, 7993u,
    3073u, 10326u, 40u, 10004u, 9245u, 7987u, 5603u, 4051u,
    7894u, 676u, 11380u, 7379u, 6501u, 4981u, 2628u, 3488u,
    10956u, 7022u, 6737u, 9933u, 7139u, 2330u, 3884u, 5473u,
    7865u, 6941u, 5737u, 5613u, 9505u, 11568u, 11277u, 2510u,
    6689u, 386u, 4462u, 105u, 2076u, 10443u, 119u, 3955u,
    4370u, 11505u, 3672u, 11439u, 750u, 3240u, 3133u, 754u,
    4013u, 11929u, 9210u, 5378u, 11881u, 11018u, 2818u, 1851u,
    4966u, 8181u, 2688u, 6205u, 6814u, 926u, 2936u, 4327u,
    10175u, 7089u, 6047u, 9410u, 10492u, 8950u, 2472u, 6255u,
    728u, 7569u, 6056u, 10432u, 11036u, 2452u, 2811u, 3787u,
    945u, 8998u, 1244u, 8815u, 11017u, 11218u, 5894u, 4325u,
    4639u, 3819u, 9826u, 7056u, 6786u, 8670u, 5539u, 7707u,
    1361u, 9812u, 2949u, 11265u, 10301u, 9108u, 478u, 6489u,
    101u, 1911u, 9483u, 3608u, 11997u, 10536u, 812u, 8915u,
    637u, 8159u, 5299u, 9128u, 3512u, 8290u, 7068u, 7922u,
    3036u, 4759u, 2163u, 3937u, 3755u, 11306u, 7739u, 4922u,
    11932u, 424u, 5538u, 6228u, 11131u, 7778u, 11974u, 1097u,
    2890u, 10027u, 2569u, 2250u, 2352u, 821u, 2550u, 11016u,
    7769u, 136u, 617u, 3157u, 5889u, 9219u, 6855u, 120u,
    4405u, 1825u, 9635u, 7214u, 10261u, 11393u, 2441u, 9562u,
    11176u, 599u, 2085u, 11465u, 7233u, 6177u, 4801u, 9926u,
    9010u, 4514u, 9455u, 11352u, 11670u, 6174u, 7950u, 9766u,
    6896u, 11603u, 3213u, 8473u, 9873u, 2835u, 10422u, 3732u,
    7961u, 1457u, 10857u, 8069u, 832u, 1628u, 3410u, 4900u,
    10855u, 5111u, 9543u, 6325u, 7431u, 4083u, 3072u, 8847u,
    9853u, 10122u, 5259u, 11413u, 6556u, 303u, 1465u, 3871u,
    4873u, 5813u, 10017u, 6898u, 3311u, 5947u, 8637u, 5852u,
    3856u, 928u, 4933u, 8530u, 1871u, 2184u, 5571u, 5879u,
    3481u, 11597u, 9511u, 8153u, 35u, 2609u, 5963u, 8064u,
    1080u, 12039u, 8444u, 3052u, 3813u, 11065u, 6736u, 8454u,
    2340u, 7651u, 1910u, 10709u, 2117u, 9637u, 6402u, 6028u,
    2124u, 7701u, 2679u, 5183u, 6270u, 7424u, 2597u, 6795u,
    9222u, 10837u, 280u, 8583u, 3270u, 6753u, 2354u, 3779u,
    6102u, 4732u, 5926u, 2497u, 8640u, 10289u, 6107u, 12127u,
    2958u, 12287u, 10292u, 8086u, 817u, 4021u, 2610u, 1444u,
    5899u, 11720u, 3292u, 2424u, 5090u, 7242u, 5205u, 5281u,
    9956u, 2702u, 6656u, 735u, 2243u, 11656u, 833u, 3107u,
    6012u, 6801u, 1126u, 6339u, 5250u, 10391u, 9642u, 5278u,
    3513u, 9769u, 3025u, 779u, 9433u, 3392u, 7437u, 668u,
    10184u, 8111u, 6527u, 6568u, 10831u, 6482u, 8263u, 5711u,
    9780u, 467u, 5462u, 4425u, 11999u, 1205u, 5015u, 6918u,
    5096u, 3827u, 5525u, 11579u, 3518u, 4875u, 7388u, 1931u,
    6615u, 1541u, 8708u, 260u, 3385u, 4792u, 4391u, 5697u,
    7895u, 2155u, 7337u, 236u, 10635u, 11534u, 1906u, 4793u,
    9527u, 7239u, 8354u, 5121u, 10662u, 2311u, 3346u, 8556u,
    707u, 1088u, 4936u, 678u, 10245u, 18u, 5684u, 960u,
    4459u, 7957u, 226u, 2451u, 6u, 8874u, 320u, 6298u,
    8963u, 8735u, 2852u, 2981u, 1707u, 5408u, 5017u, 9876u,
    9790u, 2968u, 1899u, 6729u, 4183u, 5290u, 10084u, 7679u,
    7941u, 8744u, 5694u, 3461u, 4175u, 5747u, 5561u, 3378u,
    5227u, 952u, 4319u, 9810u, 4356u, 3088u, 11118u, 840u,
    6257u, 486u, 6000u, 1342u, 10382u, 6017u, 4798u, 5489u,
    4498u, 4193u, 2306u, 6521u, 1475u, 6372u, 9029u, 8037u,
    1625u, 7020u, 4740u, 5730u, 7956u, 6351u, 6494u, 6917u,
    11405u, 7487u, 10202u, 10155u, 7666u, 7556u, 11509u, 1546u,
    6571u, 10199u, 2265u, 7327u, 5824u, 11396u, 11581u, 9722u,
    2251u, 11199u, 5356u, 7408u, 2861u, 4003u, 9215u, 484u,
    7526u, 9409u, 12235u, 6157u, 9025u, 2121u, 10255u, 2519u,
    9533u, 3824u, 8674u, 11419u, 10888u, 4762u, 11303u, 4097u,
    2414u, 6496u, 9953u, 10554u, 808u, 2999u, 2130u, 4286u,
    12078u, 7445u, 5132u, 7915u, 245u, 5974u, 4874u, 7292u,
    7560u, 10539u, 9952u, 9075u, 2113u, 3721u, 10285u, 10022u,
    9578u, 8934u, 11074u, 9498u, 294u, 4711u, 3391u, 1377u,
    9072u, 10189u, 4569u, 10890u, 9909u, 6923u, 53u, 4653u,
    439u, 10253u, 7028u, 10207u, 8343u, 1141u, 2556u, 7601u,
    8150u, 10630u, 8648u, 9832u, 7951u, 11245u, 2131u, 5765u,
    10343u, 9781u, 2718u, 1419u, 4531u, 3844u, 4066u, 4293u,
    11657u, 11525u, 11353u, 4313u, 4869u, 12186u, 1611u, 10892u,
    11489u, 8833u, 2393u, 15u, 10830u, 5003u, 17u, 565u,
    5891u, 12177u, 11058u, 10412u, 8885u, 3974u, 10981u, 7130u,
    5840u, 10482u, 8338u, 6035u, 6964u, 1574u, 10936u, 2020u,
    2465u, 8191u, 384u, 2642u, 2729u, 5399u, 2175u, 9396u,
    11987u, 8035u, 4375u, 6611u, 5010u, 11812u, 9131u, 11427u,
    104u, 6348u, 9643u, 6757u, 12110u, 5617u, 10935u, 541u,
    135u, 3041u, 7200u, 6526u, 5085u, 12136u, 842u, 4129u,
    7685u, 11079u, 8426u, 1008u, 2725u, 11772u, 6058u, 1101u,
    1950u, 8424u, 5688u, 6876u, 12005u, 10079u, 5335u, 927u,
    1770u, 273u, 8377u, 2271u, 5225u, 10283u, 116u, 11807u,
    91u, 11699u, 757u, 1304u, 7524u, 6451u, 8032u, 8154u,
    7456u, 4191u, 309u, 2318u, 2292u, 10393u, 11639u, 9481u,
    12238u, 10594u, 9569u, 7912u, 10368u, 9889u, 12244u, 7179u,
    3924u, 3188u, 367u, 2077u, 336u, 5384u, 5631u, 8596u,
    4621u, 1775u, 8866u, 451u, 6108u, 1317u, 6246u, 8795u,
    5896u, 7283u, 3132u, 11564u, 4977u, 12161u, 7371u, 1366u,
    12130u, 10619u, 3809u, 5149u, 6300u, 2638u, 4197u, 1418u,
    10065u, 4156u, 8373u, 8644u, 10445u, 882u, 8158u, 10173u,
    9763u, 12191u, 459u, 2966u, 3166u, 405u, 5000u, 9311u,
    6404u, 8986u, 1551u, 8175u, 3630u, 10766u, 9265u, 700u,
    8573u, 9508u, 6630u, 11437u, 11595u, 5850u, 3950u, 4775u,
    11941u, 1446u, 6018u, 3386u, 11470u, 5310u, 5476u, 553u,
    9474u, 2586u, 1431u, 2741u, 473u, 11383u, 4745u, 836u,
    4062u, 10666u, 7727u, 11752u, 5534u, 312u, 4307u, 4351u,
    5764u, 8679u, 8381u, 8187u, 5u, 7395u, 4363u, 1152u,
    5421u, 5231u, 6473u, 436u, 7567u, 8603u, 6229u, 8230u
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

static void intt1024_ref(uint32_t a[INTT_SIZE]) {
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
        a[i] = montgomery_ref(a[i], 12277u);
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

static int run_dma_intt1024_test(uint32_t test_id) {
    int pass = 1;

    make_test_vector(test_id);
    intt1024_ref(ref_data);

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

    printf("Running iNTT1024 test %u\n", test_id);
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
        printf("iNTT1024 DMA Test PASS\n");
    } else {
        printf("iNTT1024 DMA Test FAIL\n");
    }

    return pass;
}

int main(void) {
    int pass = 1;

    printf("Falcon iNTT1024 DMA accelerator test\n");

#ifdef DMA_HW_FIFO_MODE
    printf("DMA_HW_FIFO_MODE enabled\n");
#else
    printf("DMA_HW_FIFO_MODE disabled\n");
#endif

    dma_init(NULL);

    pass &= run_dma_intt1024_test(0u);
    pass &= run_dma_intt1024_test(1u);
    pass &= run_dma_intt1024_test(2u);
    pass &= run_dma_intt1024_test(3u);

    if (pass) {
        printf("Falcon iNTT1024 DMA accelerator test PASS\n");
        return 0;
    } else {
        printf("Falcon iNTT1024 DMA accelerator test FAIL\n");
        return 1;
    }
}
