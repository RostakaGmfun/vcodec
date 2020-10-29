#include "vcodec/vcodec.h"
#include "vcodec_common.h"

#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#define GOP 15

typedef struct {
    uint8_t *p_ref_frame;
    int gop_cnt;
    int *blockstore[2];
} vcodec_dct_ctx_t;

static const int jpeg_zigzag_order[8][8] = {
  {  0,  1,  5,  6, 14, 15, 27, 28 },
  {  2,  4,  7, 13, 16, 26, 29, 42 },
  {  3,  8, 12, 17, 25, 30, 41, 43 },
  {  9, 11, 18, 24, 31, 40, 44, 53 },
  { 10, 19, 23, 32, 39, 45, 52, 54 },
  { 20, 22, 33, 38, 46, 51, 55, 60 },
  { 21, 34, 37, 47, 50, 56, 59, 61 },
  { 35, 36, 48, 49, 57, 58, 62, 63 }
};

typedef enum {
    DCT_CODING_IMMEDIATE,
    DCT_CODING_DIFF_T,
    DCT_CODING_DIFF_L,
    DCT_CODING_DIFF_LT,
} dct_coding_t;

static vcodec_status_t vcodec_dct_process_frame(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_frame);
static vcodec_status_t vcodec_dct_reset(vcodec_enc_ctx_t *p_ctx);
static vcodec_status_t vcodec_dct_deinit(vcodec_enc_ctx_t *p_ctx);

static vcodec_status_t encode_key_frame(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_frame);
static vcodec_status_t encode_p_frame(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_frame);

static dct_coding_t decide_coding_mode(vcodec_enc_ctx_t *p_ctx, int *p_out_block, int block_x, int block_y);

static int compute_residual_cost(const int *p_prev_block, const int *p_coded_block);
static int compute_immediate_cost(const int *p_coded_block);

static void predict_line(int *p_out, const uint8_t *p_in, const uint8_t *p_prev_line, size_t size);

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
    p_dct_ctx->blockstore[0] = p_ctx->alloc(sizeof(int) * 8 * p_ctx->width);
    if (NULL == p_dct_ctx->blockstore[0]) {
        return VCODEC_STATUS_NOMEM;
    }
    p_dct_ctx->blockstore[1] = p_ctx->alloc(sizeof(int) * 8 * p_ctx->width);
    if (NULL == p_dct_ctx->blockstore[1]) {
        return VCODEC_STATUS_NOMEM;
    }

    p_ctx->process_frame = vcodec_dct_process_frame;
    p_ctx->reset = vcodec_dct_reset;
    p_ctx->deinit = vcodec_dct_deinit;
    return VCODEC_STATUS_OK;
}

static vcodec_status_t vcodec_dct_process_frame(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_frame) {
    vcodec_dct_ctx_t *p_dct_ctx = p_ctx->encoder_ctx;
    if (0 == p_dct_ctx->gop_cnt++ % GOP) {
        printf("KEYFRAME\n");
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
    uint32_t block_width = 8;
    int block[block_width * block_width];
    int dct_block[block_width * block_width];
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
    int zigzag_block[block_width * block_width];

    int prev_dc = 0;
    vcodec_dct_ctx_t *p_dct_ctx = p_ctx->encoder_ctx;
    memcpy(p_dct_ctx->p_ref_frame, p_frame, p_ctx->width * p_ctx->height);
    //memset(p_dct_ctx->blockstore[0], 0, sizeof(int) * 8 * p_ctx->width);
    //memset(p_dct_ctx->blockstore[1], 0, sizeof(int) * 8 * p_ctx->width);
    int cnt = 0;
    const uint8_t zero_line[8] = { 0 };
    for (int y = 0; y < p_ctx->height; y += block_width) {
        for (int x = 0; x < p_ctx->width; x += block_width) {
            // Copy block to temp location
            //printf("\n");
            for (int j = y; j < y + block_width; j++) {
                for (int i = 0; i < 8; i++) {
                    //printf("%3d ", p_frame[j * p_ctx->width + x + i]);
                }
                //printf("\n");
                const uint8_t *p_line_start_ptr = &p_frame[j * p_ctx->width + x];
                predict_line(block + (j - y) * block_width, p_line_start_ptr, 0 == j ? zero_line : p_line_start_ptr - p_ctx->width, block_width);
                //memcpy(block + (j - y) * block_width, &p_frame[j * p_ctx->width + x], block_width);
            }
            //printf("\n");
            printf("Block:\n");
            for (int i = 0; i < block_width; i++) {
                for (int j = 0; j < block_width; j++) {
                    printf("%3d ", block[i * block_width + j]);
                }
                printf("\n");
            }
            jpeg_fdct_islow(dct_block, block);
            for (int i = 0; i < block_width; i++) {
                for (int j = 0; j < block_width; j++) {
                    //p_dct_ctx->blockstore[1][x * block_width + jpeg_zigzag_order[i][j]] = dct_block[i * block_width + j] / quant[i * block_width + j];
                    zigzag_block[jpeg_zigzag_order[i][j]] = dct_block[i * block_width + j] / quant[i * block_width + j];
                }
            }
            int coded_data[block_width * block_width];
            //printf("CODING\n");
            for (int i = 0; i < block_width * block_width; i++) {
                //printf("%3d ", zigzag_block[i]);
                if (0 == zigzag_block[i]) {
                    cnt++;
                }
            }
            //printf("\n");
        }
        int *temp = p_dct_ctx->blockstore[0];
        p_dct_ctx->blockstore[0] = p_dct_ctx->blockstore[1];
        p_dct_ctx->blockstore[1] = temp;
    }
    printf("CNT %d\n", cnt);

    return VCODEC_STATUS_OK;
}

static vcodec_status_t encode_p_frame(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_frame) {
    return VCODEC_STATUS_OK;
}

static dct_coding_t decide_coding_mode(vcodec_enc_ctx_t *p_ctx, int *p_out_data, int block_x, int block_y) {
    vcodec_dct_ctx_t *p_dct_ctx = p_ctx->encoder_ctx;
    const int *p_cur_block = &p_dct_ctx->blockstore[1][block_x * 8];
    if (0 == block_x && 0 == block_y) {
        memcpy(p_out_data, p_cur_block, sizeof(int) * 64);
        return DCT_CODING_IMMEDIATE;
    }
    //printf("XY %d %d\n", block_x, block_y);
    const int mae_i = compute_immediate_cost(p_cur_block);
    if (0 == block_x) {
        const int *p_t_block = &p_dct_ctx->blockstore[0][block_x * 8];
        const int mae_t = compute_residual_cost(p_t_block, p_cur_block);
        if (mae_t < mae_i) {
            for (int i = 0; i < 64; i++) {
                p_out_data[i] = p_cur_block[i] - p_t_block[i];
            }
            return DCT_CODING_DIFF_T;
        } else {
            memcpy(p_out_data, p_cur_block, sizeof(int) * 64);
            return DCT_CODING_IMMEDIATE;
        }
    }

    if (0 == block_y) {
        const int *p_l_block = &p_dct_ctx->blockstore[1][block_x * 8 - 64];
        const int mae_l = compute_residual_cost(p_l_block, p_cur_block);
        if (mae_l < mae_i) {
            for (int i = 0; i < 64; i++) {
                p_out_data[i] = p_cur_block[i] - p_l_block[i];
            }
            return DCT_CODING_DIFF_L;
        } else {
            memcpy(p_out_data, p_cur_block, sizeof(int) * 64);
            return DCT_CODING_IMMEDIATE;
        }
    }

    const int *p_t_block = &p_dct_ctx->blockstore[0][block_x * 8];
    const int *p_l_block = &p_dct_ctx->blockstore[1][block_x * 8 - 64];
    const int *p_lt_block = &p_dct_ctx->blockstore[0][block_x * 8 - 64];
    const int mae_l = compute_residual_cost(p_l_block, p_cur_block);
    const int mae_t = compute_residual_cost(p_t_block, p_cur_block);
    const int mae_lt = compute_residual_cost(p_lt_block, p_cur_block);
    const int min_mae = MIN(MIN(mae_l, mae_t), MIN(mae_lt, mae_i));
    const int *p_prev_block = NULL;
    dct_coding_t ret = DCT_CODING_IMMEDIATE;
    if (mae_i == min_mae) {
        memcpy(p_out_data, p_cur_block, sizeof(int) * 64);
        return ret;
    } else if (mae_l == min_mae) {
        p_prev_block = p_l_block;
        ret = DCT_CODING_DIFF_L;
    } else if (mae_t == min_mae) {
        p_prev_block = p_t_block;
        ret = DCT_CODING_DIFF_T;
    } else {
        p_prev_block = p_lt_block;
        ret = DCT_CODING_DIFF_LT;
    }

    //printf("diff\n", mae_i, mae_l, mae_t, mae_lt);
    for (int i = 0; i < 64; i++) {
        //printf("%3d ", p_prev_block[i]);
        p_out_data[i] = p_cur_block[i] - p_prev_block[i];
    }
    //printf("\n");
    return ret;

}

static int compute_residual_cost(const int *p_prev_block, const int *p_coded_block) {
    return 1;
}

static int compute_immediate_cost(const int *p_coded_block) {
    return 2;
}

static void predict_line(int *p_out, const uint8_t *p_in, const uint8_t *p_prev_line, size_t size) {
    //p_out[0] = p_in[0]; // TODO
    //printf("%02x ", p_out[0]);
    for (int i = 0; i < size; i++) {
        // MED predictor
        const int A = p_in[i - 1];
        const int B = p_prev_line[i];
        const int C = p_prev_line[i - 1];
        const int P = p_in[i];
        const int predicted_value = (C >= MAX(A, B)) ? MIN(A, B) : (C <= MIN(A, B)) ? MAX(A, B) : (A + B - C);
        p_out[i] = P - predicted_value;
        //p_out[i] = p_in[i];
        //printf("%03d,%03d ", p_in[i], predicted_value);
    }
    //printf("\n");
}
