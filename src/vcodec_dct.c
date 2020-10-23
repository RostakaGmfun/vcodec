#include "vcodec/vcodec.h"
#include "vcodec_common.h"
#include "vec_table.h"

#include <string.h>
#include <stdio.h>
#include <stdbool.h>

typedef struct {
    vec_table_t *vec_table;
    vec_table_t *rle_table;
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

static vcodec_status_t vcodec_dct_process_frame(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_frame);
static vcodec_status_t vcodec_dct_reset(vcodec_enc_ctx_t *p_ctx);
static vcodec_status_t vcodec_dct_deinit(vcodec_enc_ctx_t *p_ctx);

vcodec_status_t vcodec_dct_init(vcodec_enc_ctx_t *p_ctx) {
    if (0 == p_ctx->width || 0 == p_ctx->height) {
        return VCODEC_STATUS_INVAL;
    }

    p_ctx->encoder_ctx = p_ctx->alloc(sizeof(vcodec_dct_ctx_t));
    if (NULL == p_ctx->encoder_ctx) {
        return VCODEC_STATUS_NOMEM;
    }

    p_ctx->process_frame = vcodec_dct_process_frame;
    p_ctx->reset = vcodec_dct_reset;
    p_ctx->deinit = vcodec_dct_deinit;
    return VCODEC_STATUS_OK;
}

static vcodec_status_t vcodec_dct_process_frame(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_frame) {
    uint32_t block_width = 8;
    uint8_t block[block_width * block_width];
    int dct_block[block_width * block_width];
    int zigzag_block[block_width * block_width];
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
    for (int x = 0; x < p_ctx->width; x += block_width) {
        for (int y = 0; y < p_ctx->height; y += block_width) {
            // Copy block to temp location
            for (int j = y; j < block_width; j++) {
                memcpy(block + (j - y) * block_width, &p_frame[j * p_ctx->width + x], block_width);
            }
            jpeg_fdct_islow(dct_block, block);
            for (int i = 0; i < block_width; i++) {
                for (int j = 0; j < block_width; j++) {
                    printf("%3d ", dct_block[i * block_width + j] / quant[i * block_width + j]);
                }
                printf(" L\n");
            }
            printf("\n");
            for (int i = 0; i < block_width; i++) {
                for (int j = 0; j < block_width; j++) {
                    zigzag_block[i * block_width + j] = dct_block[jpeg_zigzag_order[j][i]] / quant[i * block_width + j];
                }
            }
            for (int i = 0; i < block_width * block_width; i++) {
                printf("%3d ", zigzag_block[i]);
            }
            printf("\n");
            p_ctx->write((uint8_t *)zigzag_block, sizeof(zigzag_block), p_ctx->io_ctx);
        }
    }
    return VCODEC_STATUS_OK;
}

static vcodec_status_t vcodec_dct_reset(vcodec_enc_ctx_t *p_ctx) {
    return VCODEC_STATUS_OK;
}

static vcodec_status_t vcodec_dct_deinit(vcodec_enc_ctx_t *p_ctx) {
    return VCODEC_STATUS_OK;
}
