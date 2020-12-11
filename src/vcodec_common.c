#include "vcodec/vcodec.h"
#include "vcodec_common.h"
#include "vcodec/bitstream.h"
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>

//#define debug_printf printf
#define debug_printf

vcodec_status_t vcodec_enc_init(vcodec_enc_ctx_t *p_ctx, vcodec_type_t type) {
    p_ctx->bitstream_writer = p_ctx->alloc(sizeof(vcodec_bitstream_writer_t));
    memset(p_ctx->bitstream_writer, 0, sizeof(sizeof(vcodec_bitstream_writer_t)));
    p_ctx->bitstream_writer->p_io_ctx = p_ctx->io_ctx;
    p_ctx->bitstream_writer->write = p_ctx->write;

    switch (type) {
        /*
    case VCODEC_TYPE_MED_GR:
        return vcodec_med_gr_init(p_ctx);
    case VCODEC_TYPE_INTER:
        return vcodec_inter_init(p_ctx);
    case VCODEC_TYPE_VEC:
        return vcodec_vec_init(p_ctx);
        */
    case VCODEC_TYPE_DCT:
        return vcodec_dct_init(p_ctx);
    default:
        return VCODEC_STATUS_INVAL;
    }
}

vcodec_status_t vcodec_dec_init(vcodec_dec_ctx_t *p_ctx, vcodec_type_t type) {
    p_ctx->bitstream_reader = p_ctx->alloc(sizeof(vcodec_bitstream_reader_t));
    memset(p_ctx->bitstream_reader, 0, sizeof(sizeof(vcodec_bitstream_reader_t)));
    p_ctx->bitstream_reader->p_io_ctx = p_ctx->io_ctx;
    p_ctx->bitstream_reader->read = p_ctx->read;
    switch (type) {
    case VCODEC_TYPE_DCT:
        return vcodec_dec_dct_init(p_ctx);
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

static int compute_motion_block_sad(const uint8_t *p_source_frame, const uint8_t *p_ref_frame, int x, int y, int mvx, int mvy, int block_size, int frame_width) {
    int diff = 0;
    for (int i = y; i < y + block_size; i++) {
        for (int j = x; j < x + block_size; j++) {
            diff += abs(p_source_frame[i * frame_width + j] - p_ref_frame[(i + mvy) * frame_width + j + mvx]);
        }
    }
    return diff;
}

/**
 * Find best matching position from 9 points.
 * @param[in] p_ref_frame    Reference frame buffer.
 * @param[in] p_source_frame Source frame buffer
 * @param[in] x              Block center position (x) in pixels.
 * @param[in] y              Block center position (y) in pixels.
 * @param[in] frame_width    Source/reference frame width.
 * @param[in] block_size     Block width in pixels.
 * @param[in] p_mvx          Resulting vector on y axis.
 * @param[in] p_mvy          Resulting vector on y axis.
 *
 * @retval SAD for the resulting motion vector.
 */
int vcodec_match_block_tss(const uint8_t *p_ref_frame, const uint8_t *p_source_frame, int x, int y,
        int frame_width, int block_size, int *p_mvx, int *p_mvy, compute_motion_block_cost_t cost_function) {
    int sad_min = INT_MAX;
    int mvx_min = 0;
    int mvy_min = 0;

    *p_mvx = 0;
    *p_mvy = 0;

    for (int step_size = block_size; step_size > 0; step_size /= 2) {
        for (int i = -step_size; i < step_size + 1; i += step_size) {
            for (int j = -step_size; j < step_size + 1; j += step_size) {
                const int mvx = *p_mvx + j;
                const int mvy = *p_mvy + i;
                const int sad = cost_function(p_source_frame, p_ref_frame, x, y, mvx, mvy, block_size, frame_width);
                if (sad < sad_min) {
                    sad_min = sad;
                    mvx_min = mvx;
                    mvy_min = mvy;
                }
            }
        }
        *p_mvx = mvx_min;
        *p_mvy = mvy_min;
    }
    return sad_min;
}

vcodec_motion_prediction_mode_t vcodec_predict_motion_block(int *prediction, const uint8_t *p_ref_frame, int x, int y,
        const uint8_t *p_source_frame, int frame_width, int block_size, int *p_mvx, int *p_mvy, int *p_sad, vcodec_prediction_mode_t *p_intra_mode) {
    const vcodec_prediction_mode_t intra_pred = vcodec_predict_block(prediction, p_ref_frame, x, y, p_source_frame, frame_width, block_size);
    const int intra_pred_diff = compute_block_sum(prediction, block_size);
    const int inter_pred_diff = vcodec_match_block_tss(p_ref_frame, p_source_frame, x, y, frame_width, block_size, p_mvx, p_mvy, compute_motion_block_sad);
    if (inter_pred_diff < intra_pred_diff) {
        *p_sad = inter_pred_diff;
        for (int i = 0; i < block_size; i++) {
            for (int j = 0; j < block_size; j++) {
                prediction[i * block_size + j] = p_source_frame[(i + y) * frame_width + j + x]
                    - p_ref_frame[(i + y + *p_mvy) * frame_width + j + x + *p_mvx];
            }
        }
        return VCODEC_MOTION_PREDICTION_MODE_MV;
    } else {
        *p_sad = inter_pred_diff;
        *p_intra_mode = intra_pred;
        return VCODEC_MOTION_PREDICTION_MODE_INTRA;
    }
}

void vcodec_unpredict_motion_block(int *reconstructed, const uint8_t *p_ref_frame, int x, int y,
        int block_size, int frame_width, vcodec_motion_prediction_mode_t pred_mode, vcodec_prediction_mode_t intra_pred_mode, int mvx, int mvy) {
    switch (pred_mode) {
        case VCODEC_MOTION_PREDICTION_MODE_SKIP:
            for (int i = 0; i < block_size; i++) {
                for (int j = 0; j < block_size; j++) {
                    reconstructed[i * block_size + j] = p_ref_frame[(i + y + mvy) * frame_width + j + x + mvx];
                }
            }
            break;
        case VCODEC_MOTION_PREDICTION_MODE_INTRA:
            vcodec_unpredict_block(reconstructed, p_ref_frame, x, y, block_size, frame_width, pred_mode);
            break;
        case VCODEC_MOTION_PREDICTION_MODE_MV:
            for (int i = 0; i < block_size; i++) {
                for (int j = 0; j < block_size; j++) {
                    reconstructed[i * block_size + j] += p_ref_frame[(i + y + mvy) * frame_width + j + x + mvx];
                }
            }
            break;
    }
}
