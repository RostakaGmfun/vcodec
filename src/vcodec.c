#include "vcodec/vcodec.h"

#include <string.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define ABS(a) ((a) > 0 ? (a) : -(a))

/*
 * Elias delta code.
 *
 * 1. Write n in binary. The leftmost (most-significant) bit will be a 1.
 * 2. Count the bits, remove the leftmost bit of n, and prepend the count, in binary,
 *    to what is left of n after its leftmost bit has been removed.
 * 3. Subtract 1 from the count of step 2 and prepend that number of zeros to the code.
 */
static vcodec_status_t vcodec_enc_write_elias_delta_code(vcodec_enc_ctx_t *p_ctx, unsigned int value);

/**
 * Write non-negative integer using Golomb-Rice code with divider 2^m.
 */
static vcodec_status_t vcodec_enc_write_golomb_rice_code(vcodec_enc_ctx_t *p_ctx, unsigned int value, int m);

static vcodec_status_t vcodec_enc_dpcm_med_predictor_golomb(vcodec_enc_ctx_t *p_ctx, uint8_t *p_current_line, const uint8_t *p_prev_line);

static vcodec_status_t vcodec_bit_buffer_write(vcodec_enc_ctx_t *p_ctx, uint32_t bits, int num_bits);

vcodec_status_t vcodec_enc_init(vcodec_enc_ctx_t *p_ctx) {
    if (0 == p_ctx->width || 0 == p_ctx->height) {
        return VCODEC_STATUS_INVAL;
    }
    p_ctx->buffer_size = p_ctx->width * p_ctx->height;
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
    uint8_t *prev_line = p_ctx->p_buffer + p_ctx->width;
    memset(prev_line, 0, p_ctx->width);
    while (size > 0) {
        uint32_t read_size = p_ctx->width;
        status = p_ctx->read(p_ctx->p_buffer, &read_size, p_ctx->io_ctx);
        if (VCODEC_STATUS_OK != status || 0 == read_size) {
            break;
        }
        status = vcodec_enc_dpcm_med_predictor_golomb(p_ctx, p_ctx->p_buffer, prev_line);
        if (status != VCODEC_STATUS_OK) {
            return status;
        }
        memcpy(prev_line, p_ctx->p_buffer, p_ctx->width);
        size -= read_size;
    }

    return status;
}

static vcodec_status_t vcodec_bit_buffer_write(vcodec_enc_ctx_t *p_ctx, uint32_t bits, int num_bits) {
    while (num_bits > 0) {
        const int to_write = MIN(num_bits, sizeof(p_ctx->bit_buffer) * 8 - p_ctx->bit_buffer_index);
        num_bits -= to_write;
        const uint32_t mask = 32 == to_write ? 0xFFFFFFFF : (1 << to_write) - 1;
        p_ctx->bit_buffer |= (bits & mask) << p_ctx->bit_buffer_index;
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

static vcodec_status_t vcodec_enc_write_elias_delta_code(vcodec_enc_ctx_t *p_ctx, unsigned int value) {
    const int num_bits  = sizeof(int) * 8 - 1 - __builtin_clz(value);
    const int num_bits2 = sizeof(int) * 8 - 1 - __builtin_clz(num_bits);
    const int zero_prefix = 0;
    vcodec_status_t status = vcodec_bit_buffer_write(p_ctx, zero_prefix, num_bits2 - 1);
    if (VCODEC_STATUS_OK != status) {
        return status;
    }

    status = vcodec_bit_buffer_write(p_ctx, num_bits, num_bits2);
    if (VCODEC_STATUS_OK != status) {
        return status;
    }

    return vcodec_bit_buffer_write(p_ctx, value, num_bits - 1);
}

static vcodec_status_t vcodec_enc_dpcm_med_predictor_golomb(vcodec_enc_ctx_t *p_ctx, uint8_t *p_current_line, const uint8_t *p_prev_line) {
    vcodec_status_t status = vcodec_bit_buffer_write(p_ctx, p_current_line[0], 8);
    int prev_value = 0;
    int min_rle_len = 4;
    for (int i = 1; i < p_ctx->width; i++) {
        const int A = p_current_line[i - 1];
        const int B = p_prev_line[i];
        const int C = p_prev_line[i - 1];
        const int P = p_current_line[i];
        if (A == B && B == C) {
            int prev = A;
            int rle_runs = 0;
            while (i < p_ctx->width) {
                if (prev != p_current_line[i]) {
                    break;
                }
                rle_runs++;
                prev = p_current_line[i];
                i++;
            }
            vcodec_status_t status = vcodec_enc_write_golomb_rice_code(p_ctx, rle_runs, 3); // TODO: predict optimal GR code parameter
            if (VCODEC_STATUS_OK != status) {
                return status;
            }
        } else {
            const int predicted_value = (C >= MAX(A, B)) ? MIN(A, B) : (C <= MIN(A, B)) ? MAX(A, B) : (A + B - C);
            int diff = (P - predicted_value);
            const unsigned int encoded_value = (diff <= 0 ? -diff * 2 : diff * 2 - 1);
            const int golomb_param = sizeof(int) * 8 - 1 - __builtin_clz(prev_value + 1);
            prev_value = encoded_value;
            vcodec_status_t status = vcodec_enc_write_golomb_rice_code(p_ctx, encoded_value, MAX(golomb_param, 1));
            if (VCODEC_STATUS_OK != status) {
                return status;
            }
        }
    }
    return status;
}

static vcodec_status_t vcodec_enc_write_golomb_rice_code(vcodec_enc_ctx_t *p_ctx, unsigned int value, int m) {
    const int max_q = 9;
    const int q = value >> m;
    uint32_t unary_zeroes = 0;
    if (q < max_q) {
        vcodec_status_t status = vcodec_bit_buffer_write(p_ctx, unary_zeroes, q);
        if (VCODEC_STATUS_OK != status) {
            return status;
        }
        return vcodec_bit_buffer_write(p_ctx, (1 << m) | (value & ((1 << m) - 1)), m + 1);
    } else {
        vcodec_status_t status = vcodec_bit_buffer_write(p_ctx, unary_zeroes, max_q);
        if (VCODEC_STATUS_OK != status) {
            return status;
        }
        return vcodec_bit_buffer_write(p_ctx, (1 << m) | value, 9);
    }
}
