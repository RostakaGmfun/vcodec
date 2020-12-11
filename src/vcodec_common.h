#ifndef _VCODEC_COMMON_H_
#define _VCODEC_COMMON_H_

#include "vcodec/vcodec.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define ABS(a) ((a) > 0 ? (a) : -(a))

typedef enum {
    VCODEC_PREDICTION_MODE_NONE       = 0,
    VCODEC_PREDICTION_MODE_DC         = 1,
    VCODEC_PREDICTION_MODE_HORIZONTAL = 2,
    VCODEC_PREDICTION_MODE_VERTICAL   = 3,
} vcodec_prediction_mode_t;

typedef enum {
    VCODEC_MOTION_PREDICTION_MODE_SKIP   = 0,
    VCODEC_MOTION_PREDICTION_MODE_INTRA  = 1,
    VCODEC_MOTION_PREDICTION_MODE_MV     = 2, // motion vector
} vcodec_motion_prediction_mode_t;

typedef enum {
    VCODEC_BLOCK_PARTITION_MODE_NONE       = 0, //< No partition, full block (16x16, 8x8, or 4x4, depending on context)
    VCODEC_BLOCK_PARTITION_MODE_QUAD       = 1, //< Four blocks
} vcodec_block_partition_mode_t;


typedef int (*compute_motion_block_cost_t)(const uint8_t *p_source_frame, const uint8_t *p_ref_frame, int x, int y, int mvx, int mvy, int block_size, int frame_width);

vcodec_status_t vcodec_med_gr_init(vcodec_enc_ctx_t *p_ctx);

vcodec_status_t vcodec_inter_init(vcodec_enc_ctx_t *p_ctx);

vcodec_status_t vcodec_vec_init(vcodec_enc_ctx_t *p_ctx);

vcodec_status_t vcodec_dct_init(vcodec_enc_ctx_t *p_ctx);

vcodec_status_t vcodec_dec_dct_init(vcodec_dec_ctx_t *p_ctx);

vcodec_status_t vcodec_med_gr_dpcm_med_predictor_golomb(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_current_line, const uint8_t *p_prev_line);

vcodec_status_t vcodec_med_gr_write_golomb_rice_code(vcodec_enc_ctx_t *p_ctx, unsigned int value, int m);

vcodec_prediction_mode_t vcodec_predict_block(int *prediction, const uint8_t *p_ref_frame, int block_x, int block_y, const uint8_t *p_source_frame, int frame_width, int block_size);

void vcodec_unpredict_block(int *reconstructed, const uint8_t *p_ref_frame, int x, int y, int block_size, int frame_width, vcodec_prediction_mode_t pred_mode);

vcodec_motion_prediction_mode_t vcodec_predict_motion_block(int *prediction, const uint8_t *p_ref_frame, int x, int y,
        const uint8_t *p_source_frame, int frame_width, int block_size, int *p_mvx, int *p_mvy, int *p_sad, vcodec_prediction_mode_t *p_intra_mode);

void vcodec_unpredict_motion_block(int *reconstructed, const uint8_t *p_ref_frame, int x, int y,
        int block_size, int frame_width, vcodec_motion_prediction_mode_t pred_mode, vcodec_prediction_mode_t intra_pred_mode, int mvx, int mvy);

int vcodec_match_block_tss(const uint8_t *p_ref_frame, const uint8_t *p_source_frame, int x, int y,
        int frame_width, int block_size, int *p_mvx, int *p_mvy, compute_motion_block_cost_t cost_function);

#endif // _VCODEC_COMMON_H_
