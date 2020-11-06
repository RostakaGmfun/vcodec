#include "vcodec/vcodec.h"
#include "vcodec_common.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <limits.h>

#define GOP 1

//#define debug_printf printf
#define debug_printf

typedef struct {
    uint8_t *p_ref_frame;
    int gop_cnt;
} vcodec_dct_ctx_t;

static const int jpeg_zigzag_order8x8[8][8] = {
  {  0,  1,  5,  6, 14, 15, 27, 28 },
  {  2,  4,  7, 13, 16, 26, 29, 42 },
  {  3,  8, 12, 17, 25, 30, 41, 43 },
  {  9, 11, 18, 24, 31, 40, 44, 53 },
  { 10, 19, 23, 32, 39, 45, 52, 54 },
  { 20, 22, 33, 38, 46, 51, 55, 60 },
  { 21, 34, 37, 47, 50, 56, 59, 61 },
  { 35, 36, 48, 49, 57, 58, 62, 63 }
};

static const int jpeg_zigzag_order4x4[4][4] = {
  {  0,  1,  5,  6, },
  {  2,  4,  7, 12, },
  {  3,  8, 11, 13, },
  {  9, 10, 14, 15, },
};

typedef enum {
    PREDICTION_MODE_NONE,
    PREDICTION_MODE_DC,
    PREDICTION_MODE_HORIZONTAL,
    PREDICTION_MODE_VERTICAL,
} prediction_mode_t;

static vcodec_status_t vcodec_dct_process_frame(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_frame);
static vcodec_status_t vcodec_dct_reset(vcodec_enc_ctx_t *p_ctx);
static vcodec_status_t vcodec_dct_deinit(vcodec_enc_ctx_t *p_ctx);

static vcodec_status_t encode_key_frame(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_frame);
static vcodec_status_t encode_p_frame(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_frame);

static void predict_dc(int *pred_block, const uint8_t *ref_start, const int *p_src, int block_size, int ref_width);
static void predict_horizontal(int *pred_block, const uint8_t *ref_start, const int *p_src, int block_size, int ref_width);
static void predict_vertical(int *pred_block, const uint8_t *ref_start, const int *p_src, int block_size, int ref_width);
static int compute_sad(const int *pred_block, const uint8_t *source, int block_size, int source_width);
static int compute_block_sum(const int *source, int block_size);
static prediction_mode_t predict_block(int *prediction, const uint8_t *p_ref_frame, int block_x, int block_y, const uint8_t *p_source_frame, int frame_width, int block_size);
static void unpredict_block(int *reconstructed, const uint8_t *p_ref_frame, int x, int y, int block_size, int frame_width, prediction_mode_t pred_mode);

static void encode_macroblock_i(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_frame, int macroblock_x, int macroblock_y, const int *p_quant);

vcodec_status_t vcodec_dct_init(vcodec_enc_ctx_t *p_ctx) {
    if (0 == p_ctx->width || 0 == p_ctx->height) {
        return VCODEC_STATUS_INVAL;
    }

    p_ctx->encoder_ctx = p_ctx->alloc(sizeof(vcodec_dct_ctx_t));
    if (NULL == p_ctx->encoder_ctx) {
        return VCODEC_STATUS_NOMEM;
    }


    vcodec_dct_ctx_t *p_dct_ctx = p_ctx->encoder_ctx;
    p_dct_ctx->p_ref_frame = p_ctx->alloc(p_ctx->width * p_ctx->height);
    if (NULL == p_dct_ctx->p_ref_frame) {
        return VCODEC_STATUS_NOMEM;
    }
    p_dct_ctx->gop_cnt = 0;

    p_ctx->process_frame = vcodec_dct_process_frame;
    p_ctx->reset = vcodec_dct_reset;
    p_ctx->deinit = vcodec_dct_deinit;
    return VCODEC_STATUS_OK;
}

static vcodec_status_t vcodec_dct_process_frame(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_frame) {
    vcodec_dct_ctx_t *p_dct_ctx = p_ctx->encoder_ctx;
    if (0 == p_dct_ctx->gop_cnt++ % GOP) {
        debug_printf("KEYFRAME\n");
        encode_key_frame(p_ctx, p_frame);
    } else {
        encode_p_frame(p_ctx, p_frame);
    }
    return VCODEC_STATUS_OK;
}

static vcodec_status_t vcodec_dct_reset(vcodec_enc_ctx_t *p_ctx) {
    return VCODEC_STATUS_OK;
}

static vcodec_status_t vcodec_dct_deinit(vcodec_enc_ctx_t *p_ctx) {
    return VCODEC_STATUS_OK;
}

static vcodec_status_t encode_key_frame(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_frame) {
    uint32_t block_size = 4;
    int block[block_size * block_size];
    int dct_block[block_size * block_size];
    int quant[8*8] = {
        16,	11,	10,	16,	24,  40,  51,  61,
        12,	12,	14,	19,	26,  58,  60,  55,
        14,	13,	16,	24,	40,  57,  69,  56,
        14,	17,	22,	29,	51,  87,  80,  62,
        18,	22,	37,	56,	68,  109, 103, 77,
        24,	35,	55,	64,	81,  104, 113, 92,
        49,	64,	78,	87,	103, 121, 120, 101,
        72,	92,	95,	98,	112, 100, 103, 99,
    };
    int zigzag_block[block_size * block_size];

    int prev_dc = 0;
    vcodec_dct_ctx_t *p_dct_ctx = p_ctx->encoder_ctx;
    for (int y = 0; y < p_ctx->height; y += block_size) {
        for (int x = 0; x < p_ctx->width; x += block_size) {
            //printf("%d ", p_frame[y + p_ctx->width + x]);
            encode_macroblock_i(p_ctx, p_frame, x, y, quant);
        }
    }
    int mse = 0;
    for (int i = 0; i < p_ctx->width * p_ctx->height; i++) {
        const int diff = p_frame[i]- p_dct_ctx->p_ref_frame[i];
        //printf("%d ", diff);
        if (i % p_ctx->width == 0) {
            //printf("\n\n");
        }
        mse += diff * diff;
    }
    double mse_divided = (double)mse / (p_ctx->width * p_ctx->height);
    double psnr = 10 * log10((double)(255 * 255) / mse_divided);
    printf("PSNR %f mse %f\n", psnr, mse_divided);

    return VCODEC_STATUS_OK;
}

static vcodec_status_t encode_p_frame(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_frame) {
    return VCODEC_STATUS_OK;
}

static void predict_dc(int *pred_block, const uint8_t *ref_start, const int *p_src, int block_size, int ref_width) {
    int dc_val = 0;
    for (int i = 0; i < block_size; i++) {
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
    for (int i = 0; i < block_size; i++) {
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

static prediction_mode_t predict_block(int *prediction, const uint8_t *p_ref_frame, int x, int y,
        const uint8_t *p_source_frame, int frame_width, int block_size) {
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
        return PREDICTION_MODE_NONE;
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
        return PREDICTION_MODE_DC;
    } else if (horizontal_sum == min_sum) {
        memcpy(prediction, horizontal_pred, sizeof(horizontal_pred));
        return PREDICTION_MODE_HORIZONTAL;
    } else if (vertical_sum == min_sum) {
        memcpy(prediction, vertical_pred, sizeof(vertical_pred));
        return PREDICTION_MODE_VERTICAL;
    } else {
        memcpy(prediction, none_pred, sizeof(none_pred));
        return PREDICTION_MODE_NONE;
    }
}

static void unpredict_block(int *reconstructed, const uint8_t *p_ref_frame, int x, int y, int block_size, int frame_width, prediction_mode_t pred_mode) {
    switch (pred_mode) {
    case PREDICTION_MODE_NONE:
        break;
    case PREDICTION_MODE_DC:
        unpredict_dc(reconstructed, p_ref_frame + (y - 1) * frame_width + x - 1, block_size, frame_width);
        break;
    case PREDICTION_MODE_HORIZONTAL:
        unpredict_horizontal(reconstructed, p_ref_frame + y * frame_width + x - 1, block_size, frame_width);
        break;
    case PREDICTION_MODE_VERTICAL:
        unpredict_vertical(reconstructed, p_ref_frame + (y - 1) * frame_width + x, block_size, frame_width);
        break;
    }
}

static void encode_macroblock_i(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_frame, int macroblock_x, int macroblock_y, const int *p_quant) {
    const int macroblock_size = 16;
    const int block_size = 4;
    vcodec_dct_ctx_t *p_dct_ctx = p_ctx->encoder_ctx;
    // Copy block to temp location
    int macroblock[macroblock_size * macroblock_size];
    const prediction_mode_t pred_mode = predict_block(macroblock, p_dct_ctx->p_ref_frame, macroblock_x, macroblock_y, p_frame, p_ctx->width, macroblock_size);
    debug_printf("Block predicted with %d:\n", pred_mode);
    for (int i = 0; i < macroblock_size; i++) {
        for (int j = 0; j < macroblock_size; j++) {
            debug_printf("%4d, ", macroblock[i * block_size + j]);
        }
        debug_printf("\n");
    }
    for (int x = 0; x < macroblock_size; x += block_size) {
        for (int y = 0; y < macroblock_size; y += block_size) {
            debug_printf("DCT:\n");
            int block[block_size * block_size];
            for (int i = 0; i < block_size; i++) {
                memcpy(block + i * block_size, macroblock + (y + i) * block_size + x, sizeof(int) * block_size);
            }
            forward4x4(block, block);
            for (int i = 0; i < block_size; i++) {
                for (int j = 0; j < block_size; j++) {
                    debug_printf("%4d ", block[i * block_size + j]);
                }
                debug_printf("\n");
            }
            int rescaled_block[block_size * block_size];
            int zigzag_block[block_size * block_size];
            for (int i = 0; i < block_size; i++) {
                for (int j = 0; j < block_size; j++) {
                    zigzag_block[jpeg_zigzag_order4x4[i][j]] = block[i * block_size + j];// / (p_quant[i * block_size + j]);
                    rescaled_block[i * block_size + j] = zigzag_block[jpeg_zigzag_order4x4[i][j]];// * (p_quant[i * block_size + j]);
                }
            }
            int coded_data[block_size * block_size];
            debug_printf("CODING\n");
            for (int i = 0; i < block_size * block_size; i++) {
                debug_printf("%4d ", zigzag_block[i]);
            }
            debug_printf("\n");
            int reconstructed_macroblock[block_size * block_size];
            inverse4x4(reconstructed_macroblock, rescaled_block);
            debug_printf("IDCT:\n");
            for (int i = 0; i < block_size; i++) {
                for (int j = 0; j < block_size; j++) {
                    reconstructed_macroblock[i * block_size + j] /= 16;
                    debug_printf("%4d ", reconstructed_macroblock[i * block_size + j]);
                }
                memcpy(macroblock + y * macroblock_size + x, reconstructed_macroblock + i * block_size, block_size);
                debug_printf("\n");
            }
        }
    }

    unpredict_block(macroblock, p_dct_ctx->p_ref_frame, macroblock_x, macroblock_y, macroblock_size, p_ctx->width, pred_mode);
    debug_printf("Reconstructed:\n");
    int sad = 0;
    for (int i = 0; i < macroblock_size; i++) {
        for (int j = 0; j < macroblock_size; j++) {
            p_dct_ctx->p_ref_frame[(macroblock_y + i) * p_ctx->width + macroblock_x + j] = MIN(macroblock[i * macroblock_size + j], 255);
            debug_printf("%3d ", p_dct_ctx->p_ref_frame[(macroblock_y + i) * p_ctx->width + macroblock_x + j]);
            sad += abs(p_dct_ctx->p_ref_frame[(macroblock_y + i) * p_ctx->width + macroblock_x + j] - p_frame[(macroblock_y + i) * p_ctx->width + macroblock_x + j]);
        }
        debug_printf("\n");
    }
    debug_printf("SAD = %d at %4d %4d\n", sad, macroblock_x, macroblock_y);
}
