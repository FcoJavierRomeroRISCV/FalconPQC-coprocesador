#include <stdio.h>
#include <stdint.h>

#include "mmio.h"
#include "gr_heep.h"

#define ACCEL_CTRL_OFFSET      0x00
#define ACCEL_STATUS_OFFSET    0x04
#define ACCEL_ADDR_OFFSET      0x08
#define ACCEL_WDATA_OFFSET     0x0C
#define ACCEL_RDATA_OFFSET     0x10
#define ACCEL_SIZE_OFFSET      0x14

#define ACCEL_CTRL_START        0x1
#define ACCEL_CTRL_CLEAR_DONE   0x2
#define ACCEL_CTRL_BUFFER_WRITE 0x4
#define ACCEL_CTRL_BUFFER_READ  0x8

#define ACCEL_STATUS_DONE       0x1

#define FALCON_Q   12289u
#define FALCON_Q0I 12287u

#define GMB_1 7888u
#define GMB_2 11060u
#define GMB_3 11208u
#define GMB_4 6960u
#define GMB_5 4342u
#define GMB_6 6275u
#define GMB_7 9759u

#define NTT8_SIZE 8

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
    } else {
        return u + FALCON_Q - v;
    }
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

static void ntt_butterfly_ref(
    uint32_t u,
    uint32_t x,
    uint32_t s,
    uint32_t *out_u,
    uint32_t *out_x
) {
    uint32_t v = montgomery_ref(x, s);

    *out_u = mod_add_ref(u, v);
    *out_x = mod_sub_ref(u, v);
}

static void mini_ntt8_ref(uint32_t a[NTT8_SIZE]) {
    uint32_t s1[NTT8_SIZE];
    uint32_t s2[NTT8_SIZE];
    uint32_t s3[NTT8_SIZE];

    ntt_butterfly_ref(a[0], a[4], GMB_1, &s1[0], &s1[4]);
    ntt_butterfly_ref(a[1], a[5], GMB_1, &s1[1], &s1[5]);
    ntt_butterfly_ref(a[2], a[6], GMB_1, &s1[2], &s1[6]);
    ntt_butterfly_ref(a[3], a[7], GMB_1, &s1[3], &s1[7]);

    ntt_butterfly_ref(s1[0], s1[2], GMB_2, &s2[0], &s2[2]);
    ntt_butterfly_ref(s1[1], s1[3], GMB_2, &s2[1], &s2[3]);

    ntt_butterfly_ref(s1[4], s1[6], GMB_3, &s2[4], &s2[6]);
    ntt_butterfly_ref(s1[5], s1[7], GMB_3, &s2[5], &s2[7]);

    ntt_butterfly_ref(s2[0], s2[1], GMB_4, &s3[0], &s3[1]);
    ntt_butterfly_ref(s2[2], s2[3], GMB_5, &s3[2], &s3[3]);
    ntt_butterfly_ref(s2[4], s2[5], GMB_6, &s3[4], &s3[5]);
    ntt_butterfly_ref(s2[6], s2[7], GMB_7, &s3[6], &s3[7]);

    for (int i = 0; i < NTT8_SIZE; i++) {
        a[i] = s3[i];
    }
}

static void accel_write_buffer(mmio_region_t accel, uint32_t index, uint32_t value) {
    mmio_region_write32(accel, ACCEL_ADDR_OFFSET, index);
    mmio_region_write32(accel, ACCEL_WDATA_OFFSET, value);
    mmio_region_write32(accel, ACCEL_CTRL_OFFSET, ACCEL_CTRL_BUFFER_WRITE);
}

static uint32_t accel_read_buffer(mmio_region_t accel, uint32_t index) {
    mmio_region_write32(accel, ACCEL_ADDR_OFFSET, index);
    mmio_region_write32(accel, ACCEL_CTRL_OFFSET, ACCEL_CTRL_BUFFER_READ);
    return mmio_region_read32(accel, ACCEL_RDATA_OFFSET);
}

static void accel_clear_done(mmio_region_t accel) {
    mmio_region_write32(accel, ACCEL_CTRL_OFFSET, ACCEL_CTRL_CLEAR_DONE);
}

static void accel_start(mmio_region_t accel) {
    mmio_region_write32(accel, ACCEL_CTRL_OFFSET, ACCEL_CTRL_START);
}

static uint32_t accel_wait_done(mmio_region_t accel) {
    uint32_t status;

    do {
        status = mmio_region_read32(accel, ACCEL_STATUS_OFFSET);
    } while ((status & ACCEL_STATUS_DONE) == 0);

    return status;
}

static int run_ntt8_buffer_test(mmio_region_t ntt_accel, const uint32_t input[NTT8_SIZE]) {
    uint32_t ref[NTT8_SIZE];
    uint32_t hw[NTT8_SIZE];
    uint32_t status;
    uint32_t size;
    int pass = 1;

    size = mmio_region_read32(ntt_accel, ACCEL_SIZE_OFFSET);

    for (int i = 0; i < NTT8_SIZE; i++) {
        ref[i] = input[i];
    }

    mini_ntt8_ref(ref);

    accel_clear_done(ntt_accel);

    for (int i = 0; i < NTT8_SIZE; i++) {
        accel_write_buffer(ntt_accel, (uint32_t)i, input[i]);
    }

    accel_start(ntt_accel);
    status = accel_wait_done(ntt_accel);

    for (int i = 0; i < NTT8_SIZE; i++) {
        hw[i] = accel_read_buffer(ntt_accel, (uint32_t)i);
    }

    printf("Buffer size = %u\n", size);

    printf("Input:  [");
    for (int i = 0; i < NTT8_SIZE; i++) {
        printf("%u", input[i]);
        if (i != NTT8_SIZE - 1) {
            printf(", ");
        }
    }
    printf("]\n");

    printf("STATUS = 0x%08x\n", status);

    for (int i = 0; i < NTT8_SIZE; i++) {
        printf("a[%d] hw=%u ref=%u\n", i, hw[i], ref[i]);
        if (hw[i] != ref[i]) {
            pass = 0;
        }
    }

    if (pass) {
        printf("NTT8 buffer Test PASS\n");
        return 1;
    } else {
        printf("NTT8 buffer Test FAIL\n");
        return 0;
    }
}

int main(void) {
    mmio_region_t ntt_accel =
        mmio_region_from_addr((uintptr_t)FALCON_NTT_ACCEL_PERIPH_START_ADDRESS);

    int pass = 1;

    const uint32_t test0[NTT8_SIZE] = {
        1, 2, 3, 4, 5, 6, 7, 8
    };

    const uint32_t test1[NTT8_SIZE] = {
        100, 200, 300, 400, 500, 600, 700, 800
    };

    const uint32_t test2[NTT8_SIZE] = {
        10000, 5000, 3000, 7000, 12288, 1, 4091, 7888
    };

    const uint32_t test3[NTT8_SIZE] = {
        12288, 12287, 1, 2, 4091, 7888, 11060, 11208
    };

    printf("Falcon NTT8 buffer interface test\n");

    pass &= run_ntt8_buffer_test(ntt_accel, test0);
    pass &= run_ntt8_buffer_test(ntt_accel, test1);
    pass &= run_ntt8_buffer_test(ntt_accel, test2);
    pass &= run_ntt8_buffer_test(ntt_accel, test3);

    if (pass) {
        printf("Falcon NTT8 buffer interface test PASS\n");
        return 0;
    } else {
        printf("Falcon NTT8 buffer interface test FAIL\n");
        return 1;
    }
}
