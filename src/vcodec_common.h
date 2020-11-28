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

vcodec_status_t vcodec_med_gr_init(vcodec_enc_ctx_t *p_ctx);

vcodec_status_t vcodec_inter_init(vcodec_enc_ctx_t *p_ctx);

vcodec_status_t vcodec_vec_init(vcodec_enc_ctx_t *p_ctx);

vcodec_status_t vcodec_dct_init(vcodec_enc_ctx_t *p_ctx);

vcodec_status_t vcodec_dec_dct_init(vcodec_dec_ctx_t *p_ctx);

vcodec_status_t vcodec_med_gr_dpcm_med_predictor_golomb(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_current_line, const uint8_t *p_prev_line);

vcodec_status_t vcodec_med_gr_write_golomb_rice_code(vcodec_enc_ctx_t *p_ctx, unsigned int value, int m);

vcodec_prediction_mode_t vcodec_predict_block(int *prediction, const uint8_t *p_ref_frame, int block_x, int block_y, const uint8_t *p_source_frame, int frame_width, int block_size);

void vcodec_unpredict_block(int *reconstructed, const uint8_t *p_ref_frame, int x, int y, int block_size, int frame_width, vcodec_prediction_mode_t pred_mode);

#endif // _VCODEC_COMMON_H_
