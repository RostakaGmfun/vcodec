#include "vcodec/vcodec.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

/**
 * Simple RLE format: if there are two the same consecutive bytes, read the next byte as the run count (2, 255).
 */
static vcodec_status_t vcodec_enc_rle(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_buffer, size_t size);

/**
 * Encode difference between consecutive bytes within a row usign Elias delta code.
 *
 * 1. Write n in binary. The leftmost (most-significant) bit will be a 1.
 * 2. Count the bits, remove the leftmost bit of n, and prepend the count, in binary,
 *    to what is left of n after its leftmost bit has been removed.
 * 3. Subtract 1 from the count of step 2 and prepend that number of zeros to the code.
 */
static vcodec_status_t vcodec_enc_dpcm_delta(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_buffer, size_t size);

static vcodec_status_t vcodec_bit_buffer_write(vcodec_enc_ctx_t *p_ctx, uint32_t bits, int num_bits);

vcodec_status_t vcodec_enc_init(vcodec_enc_ctx_t *p_ctx) {
    if (0 == p_ctx->width || 0 == p_ctx->height) {
        return VCODEC_STATUS_INVAL;
    }
    p_ctx->buffer_size = p_ctx->width * 3;
    p_ctx->p_buffer = p_ctx->alloc(p_ctx->buffer_size);
    if (NULL == p_ctx->p_buffer) {
        return VCODEC_STATUS_NOMEM;
    }
    return VCODEC_STATUS_OK;
}

vcodec_status_t vcodec_enc_deinit(vcodec_enc_ctx_t *p_ctx) {
    p_ctx->free(p_ctx->p_buffer);
    return VCODEC_STATUS_OK;
}

vcodec_status_t vcodec_enc_reset(vcodec_enc_ctx_t *p_ctx) {
    return VCODEC_STATUS_OK;
}

vcodec_status_t vcodec_enc_process_frame(vcodec_enc_ctx_t *p_ctx) {
    uint32_t size = p_ctx->width * p_ctx->height;
    vcodec_status_t status = VCODEC_STATUS_OK;
    while (size > 0) {
        uint32_t read_size = MIN(p_ctx->buffer_size, size);
        status = p_ctx->read(p_ctx->p_buffer, &read_size, p_ctx->io_ctx);
        if (VCODEC_STATUS_OK != status || 0 == read_size) {
            break;
        }
        //status = vcodec_enc_rle(p_ctx, p_ctx->p_buffer, read_size);
        //status = p_ctx->write(p_ctx->p_buffer, read_size, p_ctx->io_ctx);
        status = vcodec_enc_dpcm_delta(p_ctx, p_ctx->p_buffer, read_size);
        size -= read_size;
    }
    return status;
}

static vcodec_status_t vcodec_enc_rle(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_buffer, size_t size) {
    uint8_t rle_byte = *p_buffer;
    vcodec_status_t status = VCODEC_STATUS_OK;
    if (0 == size) {
        return status;
    }
    status = p_ctx->write(p_buffer, 1, p_ctx->io_ctx);
    if (VCODEC_STATUS_OK != status) {
        return status;
    }
    p_buffer++;
    size--;
    unsigned int run_length = 1;
    while (size > 0) {
        if (rle_byte == *p_buffer) {
            run_length++;
        } else {
            if (run_length > 2) {
                const uint8_t rle_byte = run_length;
                status = p_ctx->write(&rle_byte, 1, p_ctx->io_ctx);
                if (VCODEC_STATUS_OK != status) {
                    return status;
                }
            }
            run_length = 1;
            rle_byte = *p_buffer;
        }

        if (run_length < 3) {
            status = p_ctx->write(p_buffer, 1, p_ctx->io_ctx);
            if (VCODEC_STATUS_OK != status) {
                return status;
            }
        } else if (255 == run_length) {
            const uint8_t rle_byte = run_length;
            status = p_ctx->write(&rle_byte, 1, p_ctx->io_ctx);
            if (VCODEC_STATUS_OK != status) {
                return status;
            }
            run_length = 1;
        }
        p_buffer++;
        size--;
    }

    return status;
}

static vcodec_status_t vcodec_enc_dpcm_delta(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_buffer, size_t size) {
    uint32_t bit_buffer = 0;
    uint32_t bit_idx = 0;
    for (int row = 0; row < size / p_ctx->width; row++) {
        uint8_t prev = *p_buffer++;
        vcodec_status_t status = vcodec_bit_buffer_write(p_ctx, prev, 8);
        if (VCODEC_STATUS_OK != status) {
            return status;
        }
        for (int i = 1; i < p_ctx->width; i++) {
            const int diff = *p_buffer - prev;
            prev = *p_buffer++;
            const int value = (diff <= 0 ? -diff * 2 : diff * 2 - 1) + 1;
            const int num_bits  = sizeof(int) * 8 - __builtin_clz(value);
            const int num_bits2 = sizeof(int) * 8 - __builtin_clz(num_bits);
            int zero_prefix = 0;
            status = vcodec_bit_buffer_write(p_ctx, zero_prefix, num_bits2 - 1);
            if (VCODEC_STATUS_OK != status) {
                return status;
            }

            status = vcodec_bit_buffer_write(p_ctx, num_bits, num_bits2);
            if (VCODEC_STATUS_OK != status) {
                return status;
            }

            status = vcodec_bit_buffer_write(p_ctx, value, num_bits - 1);
            if (VCODEC_STATUS_OK != status) {
                return status;
            }
        }
    }
    return VCODEC_STATUS_OK;
}

static vcodec_status_t vcodec_bit_buffer_write(vcodec_enc_ctx_t *p_ctx, uint32_t bits, int num_bits) {
    while (num_bits > 0) {
        const int to_write = MIN(num_bits, sizeof(p_ctx->bit_buffer) * 8 - p_ctx->bit_buffer_index);
        num_bits -= to_write;
        p_ctx->bit_buffer |= (bits & to_write) << p_ctx->bit_buffer_index;
        p_ctx->bit_buffer_index += to_write;
        bits >> to_write;
        if (sizeof(p_ctx->bit_buffer) * 8 == p_ctx->bit_buffer_index) {
            vcodec_status_t status = p_ctx->write((const uint8_t *)&p_ctx->bit_buffer, sizeof(p_ctx->bit_buffer), p_ctx->io_ctx);
            if (VCODEC_STATUS_OK != status) {
                return status;
            }
            p_ctx->bit_buffer = 0;
            p_ctx->bit_buffer_index = 0;
        }
    }

    return VCODEC_STATUS_OK;
}
