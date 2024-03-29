#include "vcodec/vcodec.h"
#include "vcodec_common.h"

#include <string.h>

static vcodec_status_t vcodec_med_gr_process_frame(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_frame);

static vcodec_status_t vcodec_med_gr_reset(vcodec_enc_ctx_t *p_ctx);

static vcodec_status_t vcodec_med_gr_deinit(vcodec_enc_ctx_t *p_ctx);

vcodec_status_t vcodec_med_gr_init(vcodec_enc_ctx_t *p_ctx) {
    if (0 == p_ctx->width || 0 == p_ctx->height) {
        return VCODEC_STATUS_INVAL;
    }
    p_ctx->buffer_size = p_ctx->width;
    p_ctx->p_buffer = p_ctx->alloc(p_ctx->buffer_size);
    if (NULL == p_ctx->p_buffer) {
        return VCODEC_STATUS_NOMEM;
    }

    p_ctx->process_frame = vcodec_med_gr_process_frame;
    p_ctx->reset = vcodec_med_gr_reset;
    p_ctx->deinit = vcodec_med_gr_deinit;
    return VCODEC_STATUS_OK;
}

static vcodec_status_t vcodec_med_gr_process_frame(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_frame) {
    uint32_t size = p_ctx->width * p_ctx->height;
    vcodec_status_t status = VCODEC_STATUS_OK;
    uint8_t *prev_line = p_ctx->p_buffer;
    memset(prev_line, 0, p_ctx->width);
    while (size > 0) {
        status = vcodec_med_gr_dpcm_med_predictor_golomb(p_ctx, p_frame, prev_line);
        if (status != VCODEC_STATUS_OK) {
            return status;
        }
        memcpy(prev_line, p_frame, p_ctx->width);
        p_frame += p_ctx->width;
        size -= p_ctx->width;
    }

    return status;
}

static vcodec_status_t vcodec_med_gr_reset(vcodec_enc_ctx_t *p_ctx) {
    return VCODEC_STATUS_OK;
}

static vcodec_status_t vcodec_med_gr_deinit(vcodec_enc_ctx_t *p_ctx) {
    p_ctx->free(p_ctx->p_buffer);
    return VCODEC_STATUS_OK;
}

vcodec_status_t vcodec_med_gr_dpcm_med_predictor_golomb(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_current_line, const uint8_t *p_prev_line) {
    vcodec_status_t status = vcodec_bit_buffer_write(p_ctx, p_current_line[0], 8);
    int prev_value = 0;
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
            vcodec_status_t status = vcodec_med_gr_write_golomb_rice_code(p_ctx, rle_runs, 3); // TODO: predict optimal GR code parameter
            if (VCODEC_STATUS_OK != status) {
                return status;
            }
        } else {
            const int predicted_value = (C >= MAX(A, B)) ? MIN(A, B) : (C <= MIN(A, B)) ? MAX(A, B) : (A + B - C);
            int diff = (P - predicted_value);
            const unsigned int encoded_value = (diff <= 0 ? -diff * 2 : diff * 2 - 1);
            const int golomb_param = sizeof(int) * 8 - 1 - __builtin_clz(prev_value + 1);
            prev_value = encoded_value;
            vcodec_status_t status = vcodec_med_gr_write_golomb_rice_code(p_ctx, encoded_value, MAX(golomb_param, 1));
            if (VCODEC_STATUS_OK != status) {
                return status;
            }
        }
    }
    return status;
}

vcodec_status_t vcodec_med_gr_write_golomb_rice_code(vcodec_enc_ctx_t *p_ctx, unsigned int value, int m) {
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
