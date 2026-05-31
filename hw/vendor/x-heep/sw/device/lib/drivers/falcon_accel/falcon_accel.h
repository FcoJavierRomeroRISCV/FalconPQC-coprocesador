#ifndef FALCON_ACCEL_H_
#define FALCON_ACCEL_H_

#include <stdint.h>

#include "mmio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FALCON_ACCEL_CTRL_OFFSET        0x00
#define FALCON_ACCEL_STATUS_OFFSET      0x04
#define FALCON_ACCEL_ADDR_OFFSET        0x08
#define FALCON_ACCEL_WDATA_OFFSET       0x0C
#define FALCON_ACCEL_RDATA_OFFSET       0x10
#define FALCON_ACCEL_SIZE_OFFSET        0x14
#define FALCON_ACCEL_PERF_CYCLES_OFFSET 0x18

#define FALCON_ACCEL_CTRL_START         0x1
#define FALCON_ACCEL_CTRL_CLEAR_DONE    0x2
#define FALCON_ACCEL_CTRL_BUFFER_WRITE  0x4
#define FALCON_ACCEL_CTRL_BUFFER_READ   0x8

#define FALCON_ACCEL_STATUS_DONE        0x1
#define FALCON_ACCEL_STATUS_BUSY        0x2

void falcon_accel_clear_done(mmio_region_t accel);

void falcon_accel_start(mmio_region_t accel);

uint32_t falcon_accel_wait_done(mmio_region_t accel);

void falcon_accel_write_buffer(
    mmio_region_t accel,
    uint32_t index,
    uint32_t value
);

uint32_t falcon_accel_read_buffer(
    mmio_region_t accel,
    uint32_t index
);

void falcon_accel_write_vector(
    mmio_region_t accel,
    const uint32_t *data,
    uint32_t len
);

void falcon_accel_read_vector(
    mmio_region_t accel,
    uint32_t *data,
    uint32_t len
);

uint32_t falcon_accel_get_size(mmio_region_t accel);

uint32_t falcon_accel_get_cycles(mmio_region_t accel);

#ifdef __cplusplus
}
#endif

#endif  // FALCON_ACCEL_H_
