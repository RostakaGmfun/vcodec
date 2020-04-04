#ifndef _VCODEC_COMMON_H_
#define _VCODEC_COMMON_H_

#include "vcodec/vcodec.h"

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
vcodec_status_t vcodec_write_elias_delta_code(vcodec_enc_ctx_t *p_ctx, unsigned int value);

vcodec_status_t vcodec_bit_buffer_write(vcodec_enc_ctx_t *p_ctx, uint32_t bits, int num_bits);

vcodec_status_t vcodec_med_gr_init(vcodec_enc_ctx_t *p_ctx);

vcodec_status_t vcodec_inter_init(vcodec_enc_ctx_t *p_ctx);

vcodec_status_t vcodec_vec_init(vcodec_enc_ctx_t *p_ctx);

vcodec_status_t vcodec_med_gr_dpcm_med_predictor_golomb(vcodec_enc_ctx_t *p_ctx, const uint8_t *p_current_line, const uint8_t *p_prev_line);

vcodec_status_t vcodec_med_gr_write_golomb_rice_code(vcodec_enc_ctx_t *p_ctx, unsigned int value, int m);

#endif // _VCODEC_COMMON_H_
