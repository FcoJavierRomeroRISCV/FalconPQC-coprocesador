#include "falcon_accel.h"

void falcon_accel_clear_done(mmio_region_t accel) {
    mmio_region_write32(
        accel,
        FALCON_ACCEL_CTRL_OFFSET,
        FALCON_ACCEL_CTRL_CLEAR_DONE
    );
}

void falcon_accel_start(mmio_region_t accel) {
    mmio_region_write32(
        accel,
        FALCON_ACCEL_CTRL_OFFSET,
        FALCON_ACCEL_CTRL_START
    );
}

uint32_t falcon_accel_wait_done(mmio_region_t accel) {
    uint32_t status;

    do {
        status = mmio_region_read32(
            accel,
            FALCON_ACCEL_STATUS_OFFSET
        );
    } while ((status & FALCON_ACCEL_STATUS_DONE) == 0);

    return status;
}

void falcon_accel_write_buffer(
    mmio_region_t accel,
    uint32_t index,
    uint32_t value
) {
    mmio_region_write32(
        accel,
        FALCON_ACCEL_ADDR_OFFSET,
        index
    );

    mmio_region_write32(
        accel,
        FALCON_ACCEL_WDATA_OFFSET,
        value
    );

    mmio_region_write32(
        accel,
        FALCON_ACCEL_CTRL_OFFSET,
        FALCON_ACCEL_CTRL_BUFFER_WRITE
    );
}

uint32_t falcon_accel_read_buffer(
    mmio_region_t accel,
    uint32_t index
) {
    mmio_region_write32(
        accel,
        FALCON_ACCEL_ADDR_OFFSET,
        index
    );

    mmio_region_write32(
        accel,
        FALCON_ACCEL_CTRL_OFFSET,
        FALCON_ACCEL_CTRL_BUFFER_READ
    );

    return mmio_region_read32(
        accel,
        FALCON_ACCEL_RDATA_OFFSET
    );
}

void falcon_accel_write_vector(
    mmio_region_t accel,
    const uint32_t *data,
    uint32_t len
) {
    for (uint32_t i = 0; i < len; i++) {
        falcon_accel_write_buffer(accel, i, data[i]);
    }
}

void falcon_accel_read_vector(
    mmio_region_t accel,
    uint32_t *data,
    uint32_t len
) {
    for (uint32_t i = 0; i < len; i++) {
        data[i] = falcon_accel_read_buffer(accel, i);
    }
}

uint32_t falcon_accel_get_size(mmio_region_t accel) {
    return mmio_region_read32(
        accel,
        FALCON_ACCEL_SIZE_OFFSET
    );
}

uint32_t falcon_accel_get_cycles(mmio_region_t accel) {
    return mmio_region_read32(
        accel,
        FALCON_ACCEL_PERF_CYCLES_OFFSET
    );
}
