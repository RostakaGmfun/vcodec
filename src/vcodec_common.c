#include "vcodec/vcodec.h"
#include "vcodec_common.h"
#include <stdlib.h>
#include <limits.h>
#include <string.h>

//#define debug_printf printf
#define debug_printf

vcodec_status_t vcodec_write_elias_delta_code(vcodec_enc_ctx_t *p_ctx, unsigned int value) {
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

vcodec_status_t vcodec_write_exp_golomb_code(vcodec_enc_ctx_t *p_ctx, unsigned int value) {
    const int num_bits  = sizeof(int) * 8 - 1 - __builtin_clz(value + 1);
    const int zero_prefix = 0;
    vcodec_status_t status = vcodec_bit_buffer_write(p_ctx, zero_prefix, num_bits - 1);
    if (VCODEC_STATUS_OK != status) {
        return status;
    }

    return vcodec_bit_buffer_write(p_ctx, value + 1, num_bits);
}

vcodec_status_t vcodec_read_exp_golomb_code(vcodec_dec_ctx_t *p_ctx, unsigned int *p_value) {
    uint32_t prefix = 0;
    vcodec_status_t ret = VCODEC_STATUS_OK;
    int num_bits = 0;
    do {
        num_bits++;
        ret = vcodec_bit_buffer_read(p_ctx, &prefix, 1);
        if (VCODEC_STATUS_OK != ret) {
            return ret;
        }
    } while (prefix == 0);

    *p_value = 1 << num_bits;
    uint32_t data = 0;
    ret = vcodec_bit_buffer_read(p_ctx, &data, num_bits - 1);
    *p_value |= data;
    *p_value--;
    return ret;
}

vcodec_status_t vcodec_bit_buffer_write(vcodec_enc_ctx_t *p_ctx, uint32_t bits, int num_bits) {
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

vcodec_status_t vcodec_bit_buffer_read(vcodec_dec_ctx_t *p_ctx, uint32_t *bits, int num_bits) {
    while (num_bits > 0) {
        if (0 == p_ctx->bits_available) {
            uint32_t num_read = 0;
            vcodec_status_t status = p_ctx->read((const uint8_t *)&p_ctx->bit_buffer, sizeof(p_ctx->bit_buffer), &num_read, p_ctx->io_ctx);
            if (VCODEC_STATUS_OK != status) {
                return status;
            }
            p_ctx->bits_available = num_read * 8;
            p_ctx->bit_buffer_index = 0;
        }
        const int to_read = MIN(num_bits, p_ctx->bits_available);
        if (0 == to_read) {
            return VCODEC_STATUS_EOF;
        }
        num_bits -= to_read;
        const uint32_t mask = 32 == to_read ? 0xFFFFFFFF : (1 << to_read) - 1;
        *bits |= (p_ctx->bit_buffer >> p_ctx->bit_buffer_index) & mask;
        *bits <<= to_read;
        p_ctx->bit_buffer_index += to_read;
        p_ctx->bits_available -= to_read;
    }
    return VCODEC_STATUS_OK;
}

vcodec_status_t vcodec_enc_init(vcodec_enc_ctx_t *p_ctx, vcodec_type_t type) {
    switch (type) {
    case VCODEC_TYPE_MED_GR:
        return vcodec_med_gr_init(p_ctx);
    case VCODEC_TYPE_INTER:
        return vcodec_inter_init(p_ctx);
    case VCODEC_TYPE_VEC:
        return vcodec_vec_init(p_ctx);
    case VCODEC_TYPE_DCT:
        return vcodec_dct_init(p_ctx);
    default:
        return VCODEC_STATUS_INVAL;
    }
}

vcodec_status_t vcodec_dec_init(vcodec_dec_ctx_t *p_ctx, vcodec_type_t type) {
    switch (type) {
    case VCODEC_TYPE_DCT:
        //return vcodec_dec_dct_init(p_ctx);
    default:
        return VCODEC_STATUS_INVAL;
    }
}

static void predict_dc(int *pred_block, const uint8_t *ref_start, const int *p_src, int block_size, int ref_width) {
    int dc_val = 0;
    for (int i = 1; i < block_size; i++) {
        dc_val += ref_start[i];
    }

    for (int i = 1; i < block_size; i++) {
        dc_val += ref_start[i * ref_width];
    }

    const int pred = dc_val / (block_size * 2 - 1);
    for (int i = 0; i < block_size; i++) {
        for (int j = 0; j < block_size; j++) {
            pred_block[i * block_size + j] = p_src[i * block_size + j] - pred;
        }
    }
}

static void predict_horizontal(int *pred_block, const uint8_t *ref_start, const int *p_src, int block_size, int ref_width) {
    for (int i = 0; i < block_size; i++) {
        const int pred_val = ref_start[i * ref_width];
        for (int j = 0; j < block_size; j++) {
            pred_block[i * block_size + j] = p_src[i * block_size + j] - pred_val;
        }
    }
}

static void predict_vertical(int *pred_block, const uint8_t *ref_start, const int *p_src, int block_size, int frame_width) {
    for (int i = 0; i < block_size; i++) {
        for (int j = 0; j < block_size; j++) {
            pred_block[i * block_size + j] = p_src[i * block_size + j] - ref_start[j];
        }
    }
}

static void unpredict_dc(int *pred_block, const uint8_t *ref_start, int block_size, int ref_width) {
    int dc_val = 0;
    for (int i = 1; i < block_size; i++) {
        dc_val += ref_start[i];
    }

    for (int i = 1; i < block_size; i++) {
        dc_val += ref_start[i * ref_width];
    }

    const int pred = dc_val / (block_size * 2 - 1);
    for (int i = 0; i < block_size; i++) {
        for (int j = 0; j < block_size; j++) {
            pred_block[i * block_size + j] += pred;
        }
    }
}

static void unpredict_horizontal(int *pred_block, const uint8_t *ref_start, int block_size, int ref_width) {
    for (int i = 0; i < block_size; i++) {
        const int pred_val = ref_start[i * ref_width];
        for (int j = 0; j < block_size; j++) {
            pred_block[i * block_size + j] += pred_val;
        }
    }
}

static void unpredict_vertical(int *pred_block, const uint8_t *ref_start, int block_size, int ref_width) {
    for (int i = 0; i < block_size; i++) {
        for (int j = 0; j < block_size; j++) {
            pred_block[i * block_size + j] += ref_start[j];
        }
    }
}

static int compute_block_sum(const int *source, int block_size) {
    int sum = 0;
    for (int i = 0; i < block_size; i++) {
        for (int j = 0; j < block_size; j++) {
            sum += abs(source[i * block_size + j]);
        }
    }
    return sum;
}

vcodec_prediction_mode_t vcodec_predict_block(int *prediction, const uint8_t *p_ref_frame, int x, int y, const uint8_t *p_source_frame, int frame_width, int block_size) {
    int none_pred[block_size * block_size];
    int horizontal_pred[block_size * block_size];
    int vertical_pred[block_size * block_size];
    int dc_pred[block_size * block_size];
    int original_sum = INT_MAX;
    int vertical_sum = INT_MAX;
    int horizontal_sum = INT_MAX;
    int dc_sum = INT_MAX;

    debug_printf("PRED input:\n");
    for (int i = 0; i < block_size; i++) {
        for (int j = 0; j < block_size; j++) {
            none_pred[i * block_size + j] = p_source_frame[(y + i) * frame_width + x + j];
            debug_printf("%4d ", none_pred[i * block_size + j]);
        }
        debug_printf("\n");
    }
    debug_printf("\n");

    if (0 == y && 0 == x) {
        // No prediction is available for top-left block
        memcpy(prediction, none_pred, sizeof(none_pred));
        return VCODEC_PREDICTION_MODE_NONE;
    } else if (0 == y) {
        original_sum = compute_block_sum(none_pred, block_size);
        predict_horizontal(horizontal_pred, p_ref_frame + y * frame_width + x - 1, none_pred, block_size, frame_width);
        horizontal_sum = compute_block_sum(horizontal_pred, block_size);
    } else if (0 == x) {
        original_sum = compute_block_sum(none_pred, block_size);
        predict_vertical(vertical_pred, p_ref_frame + (y - 1) * frame_width + x, none_pred, block_size, frame_width);
        vertical_sum = compute_block_sum(vertical_pred, block_size);
    } else {
        original_sum = compute_block_sum(none_pred, block_size);

        predict_horizontal(horizontal_pred, p_ref_frame + y * frame_width + x - 1, none_pred, block_size, frame_width);
        horizontal_sum = compute_block_sum(horizontal_pred, block_size);

        predict_vertical(vertical_pred, p_ref_frame + (y - 1) * frame_width + x, none_pred, block_size, frame_width);
        vertical_sum = compute_block_sum(vertical_pred, block_size);

        predict_dc(dc_pred, p_ref_frame + (y - 1) * frame_width + x - 1, none_pred, block_size, frame_width);
        dc_sum = compute_block_sum(dc_pred, block_size);
    }

    debug_printf("Sums computed: %d %d %d %d\n", original_sum, dc_sum, horizontal_sum, vertical_sum);
    const int min_sum = MIN(original_sum, MIN(dc_sum, MIN(vertical_sum, horizontal_sum)));
    if (dc_sum == min_sum) {
        memcpy(prediction, dc_pred, sizeof(dc_pred));
        return VCODEC_PREDICTION_MODE_DC;
    } else if (horizontal_sum == min_sum) {
        memcpy(prediction, horizontal_pred, sizeof(horizontal_pred));
        return VCODEC_PREDICTION_MODE_HORIZONTAL;
    } else if (vertical_sum == min_sum) {
        memcpy(prediction, vertical_pred, sizeof(vertical_pred));
        return VCODEC_PREDICTION_MODE_VERTICAL;
    } else {
        memcpy(prediction, none_pred, sizeof(none_pred));
        return VCODEC_PREDICTION_MODE_NONE;
    }
}

void vcodec_unpredict_block(int *reconstructed, const uint8_t *p_ref_frame, int x, int y, int block_size, int frame_width, vcodec_prediction_mode_t pred_mode) {
    switch (pred_mode) {
    case VCODEC_PREDICTION_MODE_NONE:
        break;
    case VCODEC_PREDICTION_MODE_DC:
        unpredict_dc(reconstructed, p_ref_frame + (y - 1) * frame_width + x - 1, block_size, frame_width);
        break;
    case VCODEC_PREDICTION_MODE_HORIZONTAL:
        unpredict_horizontal(reconstructed, p_ref_frame + y * frame_width + x - 1, block_size, frame_width);
        break;
    case VCODEC_PREDICTION_MODE_VERTICAL:
        unpredict_vertical(reconstructed, p_ref_frame + (y - 1) * frame_width + x, block_size, frame_width);
        break;
    }
}
