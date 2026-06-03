#include <stdio.h>
#include <stdint.h>

#include "mmio.h"
#include "gr_heep.h"

#define ACCEL_CTRL_OFFSET      0x00
#define ACCEL_STATUS_OFFSET    0x04
#define ACCEL_DATA0_OFFSET     0x08
#define ACCEL_DATA1_OFFSET     0x0C
#define ACCEL_DATA2_OFFSET     0x10
#define ACCEL_DATA3_OFFSET     0x14

#define ACCEL_CTRL_START       0x1
#define ACCEL_CTRL_CLEAR_DONE  0x2

#define ACCEL_STATUS_DONE      0x1
#define ACCEL_STATUS_BUSY      0x2

#define FALCON_Q   12289u
#define FALCON_Q0I 12287u

static inline uint64_t read_cycles(void) {
    uint32_t cycles_low;
    uint32_t cycles_high;
    uint32_t cycles_high_check;

    do {
        asm volatile ("rdcycleh %0" : "=r"(cycles_high));
        asm volatile ("rdcycle %0"  : "=r"(cycles_low));
        asm volatile ("rdcycleh %0" : "=r"(cycles_high_check));
    } while (cycles_high != cycles_high_check);

    return ((uint64_t)cycles_high << 32) | cycles_low;
}

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
    uint32_t *add_out,
    uint32_t *sub_out,
    uint32_t *v_out
) {
    uint32_t v = montgomery_ref(x, s);

    *add_out = mod_add_ref(u, v);
    *sub_out = mod_sub_ref(u, v);
    *v_out = v;
}

static void intt_butterfly_ref(
    uint32_t u,
    uint32_t v,
    uint32_t s,
    uint32_t *add_out,
    uint32_t *mont_out,
    uint32_t *sub_out
) {
    uint32_t add_res = mod_add_ref(u, v);
    uint32_t sub_res = mod_sub_ref(u, v);
    uint32_t mont_res = montgomery_ref(sub_res, s);

    *add_out = add_res;
    *mont_out = mont_res;
    *sub_out = sub_res;
}

static void accel_clear_done(mmio_region_t accel) {
    mmio_region_write32(
        accel,
        ACCEL_CTRL_OFFSET,
        ACCEL_CTRL_CLEAR_DONE
    );
}

static void accel_start(mmio_region_t accel) {
    mmio_region_write32(
        accel,
        ACCEL_CTRL_OFFSET,
        ACCEL_CTRL_START
    );
}

static uint32_t accel_wait_done(mmio_region_t accel) {
    uint32_t status;

    do {
        status = mmio_region_read32(
            accel,
            ACCEL_STATUS_OFFSET
        );
    } while ((status & ACCEL_STATUS_DONE) == 0);

    return status;
}

static int run_ntt_test(mmio_region_t ntt_accel, uint32_t u, uint32_t x, uint32_t s) {
    uint32_t status;

    uint32_t v_ref;
    uint32_t add_ref;
    uint32_t sub_ref;

    uint32_t add_hw;
    uint32_t sub_hw;
    uint32_t v_hw;

    uint64_t sw_start;
    uint64_t sw_end;
    uint64_t hw_start;
    uint64_t hw_end;

    sw_start = read_cycles();
    ntt_butterfly_ref(u, x, s, &add_ref, &sub_ref, &v_ref);
    sw_end = read_cycles();

    hw_start = read_cycles();

    accel_clear_done(ntt_accel);

    mmio_region_write32(ntt_accel, ACCEL_DATA0_OFFSET, u);
    mmio_region_write32(ntt_accel, ACCEL_DATA1_OFFSET, x);
    mmio_region_write32(ntt_accel, ACCEL_DATA2_OFFSET, s);

    accel_start(ntt_accel);
    status = accel_wait_done(ntt_accel);

    add_hw = mmio_region_read32(ntt_accel, ACCEL_DATA0_OFFSET);
    sub_hw = mmio_region_read32(ntt_accel, ACCEL_DATA1_OFFSET);
    v_hw   = mmio_region_read32(ntt_accel, ACCEL_DATA2_OFFSET);

    hw_end = read_cycles();

    printf("[NTT] u=%u x=%u s=%u\n", u, x, s);
    printf("[NTT] STATUS = 0x%08x\n", status);
    printf("[NTT] v    hw=%u ref=%u\n", v_hw, v_ref);
    printf("[NTT] ADD  hw=%u ref=%u\n", add_hw, add_ref);
    printf("[NTT] SUB  hw=%u ref=%u\n", sub_hw, sub_ref);
    printf("[NTT] SW cycles = %u\n", (uint32_t)(sw_end - sw_start));
    printf("[NTT] HW cycles = %u\n", (uint32_t)(hw_end - hw_start));

    if ((v_hw == v_ref) && (add_hw == add_ref) && (sub_hw == sub_ref)) {
        printf("[NTT] Test PASS\n");
        return 1;
    } else {
        printf("[NTT] Test FAIL\n");
        return 0;
    }
}

static int run_intt_test(mmio_region_t intt_accel, uint32_t u, uint32_t v, uint32_t s) {
    uint32_t status;

    uint32_t add_ref;
    uint32_t sub_ref;
    uint32_t mont_ref;

    uint32_t add_hw;
    uint32_t mont_hw;
    uint32_t sub_hw;

    uint64_t sw_start;
    uint64_t sw_end;
    uint64_t hw_start;
    uint64_t hw_end;

    sw_start = read_cycles();
    intt_butterfly_ref(u, v, s, &add_ref, &mont_ref, &sub_ref);
    sw_end = read_cycles();

    hw_start = read_cycles();

    accel_clear_done(intt_accel);

    mmio_region_write32(intt_accel, ACCEL_DATA0_OFFSET, u);
    mmio_region_write32(intt_accel, ACCEL_DATA1_OFFSET, v);
    mmio_region_write32(intt_accel, ACCEL_DATA2_OFFSET, s);

    accel_start(intt_accel);
    status = accel_wait_done(intt_accel);

    add_hw  = mmio_region_read32(intt_accel, ACCEL_DATA0_OFFSET);
    mont_hw = mmio_region_read32(intt_accel, ACCEL_DATA1_OFFSET);
    sub_hw  = mmio_region_read32(intt_accel, ACCEL_DATA2_OFFSET);

    hw_end = read_cycles();

    printf("[iNTT] u=%u v=%u s=%u\n", u, v, s);
    printf("[iNTT] STATUS = 0x%08x\n", status);
    printf("[iNTT] ADD   hw=%u ref=%u\n", add_hw, add_ref);
    printf("[iNTT] MONT  hw=%u ref=%u\n", mont_hw, mont_ref);
    printf("[iNTT] SUB   hw=%u ref=%u\n", sub_hw, sub_ref);
    printf("[iNTT] SW cycles = %u\n", (uint32_t)(sw_end - sw_start));
    printf("[iNTT] HW cycles = %u\n", (uint32_t)(hw_end - hw_start));

    if ((add_hw == add_ref) && (mont_hw == mont_ref) && (sub_hw == sub_ref)) {
        printf("[iNTT] Test PASS\n");
        return 1;
    } else {
        printf("[iNTT] Test FAIL\n");
        return 0;
    }
}

int main(void) {
    mmio_region_t ntt_accel =
        mmio_region_from_addr((uintptr_t)FALCON_NTT_ACCEL_PERIPH_START_ADDRESS);

    mmio_region_t intt_accel =
        mmio_region_from_addr((uintptr_t)FALCON_INTT_ACCEL_PERIPH_START_ADDRESS);

    int pass = 1;

    printf("Falcon two accelerators cycle test\n");

    printf("Testing falcon_ntt_accel\n");

    pass &= run_ntt_test(ntt_accel, 10000, 5000, 4091);
    pass &= run_ntt_test(ntt_accel, 3000, 5000, 4091);
    pass &= run_ntt_test(ntt_accel, 12288, 12288, 4091);

    printf("Testing falcon_intt_accel\n");

    pass &= run_intt_test(intt_accel, 10000, 5000, 4091);
    pass &= run_intt_test(intt_accel, 3000, 5000, 4091);
    pass &= run_intt_test(intt_accel, 12288, 12288, 4091);

    if (pass) {
        printf("Falcon two accelerators cycle test PASS\n");
        return 0;
    } else {
        printf("Falcon two accelerators cycle test FAIL\n");
        return 1;
    }
}