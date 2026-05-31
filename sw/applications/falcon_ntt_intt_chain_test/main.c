#include <stdio.h>
#include <stdint.h>

#include "mmio.h"
#include "gr_heep.h"
#include "falcon_accel.h"

#define FALCON_TEST_SIZE 8

static void print_vector(const char *label, const uint32_t v[FALCON_TEST_SIZE]) {
    printf("%s[", label);

    for (int i = 0; i < FALCON_TEST_SIZE; i++) {
        printf("%u", v[i]);
        if (i != FALCON_TEST_SIZE - 1) {
            printf(", ");
        }
    }

    printf("]\n");
}

static int compare_vectors(
    const uint32_t final[FALCON_TEST_SIZE],
    const uint32_t original[FALCON_TEST_SIZE]
) {
    int pass = 1;

    for (int i = 0; i < FALCON_TEST_SIZE; i++) {
        printf("a[%d] final=%u original=%u\n", i, final[i], original[i]);

        if (final[i] != original[i]) {
            pass = 0;
        }
    }

    return pass;
}

static int run_chain_test(
    mmio_region_t ntt_accel,
    mmio_region_t intt_accel,
    const uint32_t input[FALCON_TEST_SIZE]
) {
    uint32_t ntt_out[FALCON_TEST_SIZE];
    uint32_t final_out[FALCON_TEST_SIZE];

    uint32_t ntt_status;
    uint32_t intt_status;

    uint32_t ntt_cycles;
    uint32_t intt_cycles;

    uint32_t ntt_size;
    uint32_t intt_size;

    int pass;

    ntt_size = falcon_accel_get_size(ntt_accel);
    intt_size = falcon_accel_get_size(intt_accel);

    printf("================================\n");
    print_vector("Input:      ", input);

    /*
     * Step 1: execute NTT8 hardware.
     */
    falcon_accel_clear_done(ntt_accel);
    falcon_accel_write_vector(ntt_accel, input, FALCON_TEST_SIZE);

    falcon_accel_start(ntt_accel);
    ntt_status = falcon_accel_wait_done(ntt_accel);

    ntt_cycles = falcon_accel_get_cycles(ntt_accel);
    falcon_accel_read_vector(ntt_accel, ntt_out, FALCON_TEST_SIZE);

    /*
     * Step 2: execute iNTT8 hardware using NTT output as input.
     */
    falcon_accel_clear_done(intt_accel);
    falcon_accel_write_vector(intt_accel, ntt_out, FALCON_TEST_SIZE);

    falcon_accel_start(intt_accel);
    intt_status = falcon_accel_wait_done(intt_accel);

    intt_cycles = falcon_accel_get_cycles(intt_accel);
    falcon_accel_read_vector(intt_accel, final_out, FALCON_TEST_SIZE);

    print_vector("NTT output: ", ntt_out);
    print_vector("Final out:  ", final_out);

    printf("NTT  buffer size = %u\n", ntt_size);
    printf("iNTT buffer size = %u\n", intt_size);

    printf("NTT  STATUS = 0x%08x\n", ntt_status);
    printf("iNTT STATUS = 0x%08x\n", intt_status);

    printf("NTT  cycles = %u\n", ntt_cycles);
    printf("iNTT cycles = %u\n", intt_cycles);
    printf("Total accelerator cycles = %u\n", ntt_cycles + intt_cycles);

    pass = compare_vectors(final_out, input);

    if (pass) {
        printf("NTT -> iNTT chain Test PASS\n");
        return 1;
    } else {
        printf("NTT -> iNTT chain Test FAIL\n");
        return 0;
    }
}

int main(void) {
    mmio_region_t ntt_accel =
        mmio_region_from_addr((uintptr_t)FALCON_NTT_ACCEL_PERIPH_START_ADDRESS);

    mmio_region_t intt_accel =
        mmio_region_from_addr((uintptr_t)FALCON_INTT_ACCEL_PERIPH_START_ADDRESS);

    int pass = 1;

    const uint32_t test0[FALCON_TEST_SIZE] = {
        1, 2, 3, 4, 5, 6, 7, 8
    };

    const uint32_t test1[FALCON_TEST_SIZE] = {
        100, 200, 300, 400, 500, 600, 700, 800
    };

    const uint32_t test2[FALCON_TEST_SIZE] = {
        10000, 5000, 3000, 7000, 12288, 1, 4091, 7888
    };

    const uint32_t test3[FALCON_TEST_SIZE] = {
        12288, 12287, 1, 2, 4091, 7888, 11060, 11208
    };

    printf("Falcon NTT -> iNTT chain accelerator test\n");

    pass &= run_chain_test(ntt_accel, intt_accel, test0);
    pass &= run_chain_test(ntt_accel, intt_accel, test1);
    pass &= run_chain_test(ntt_accel, intt_accel, test2);
    pass &= run_chain_test(ntt_accel, intt_accel, test3);

    if (pass) {
        printf("Falcon NTT -> iNTT chain accelerator test PASS\n");
        return 0;
    } else {
        printf("Falcon NTT -> iNTT chain accelerator test FAIL\n");
        return 1;
    }
}
