#pragma once

#include "vcodec/vcodec.h"

typedef struct vcodec_bitstream_writer vcodec_bitstream_writer_t;
typedef struct vcodec_bitstream_reader vcodec_bitstream_reader_t;

/**
 * Write coefficient block of size @c count from @c p_coeffs into bitstream represented by @c p_bitstream_writer.
 */
void vcodec_ec_write_coeffs(vcodec_bitstream_writer_t *p_bitstream_writer, const int *p_coeffs, int count);

/**
 * Read coefficient block of size @c count into @c p_coeffs from bitstream represented by @c p_bitstream_reader.
 */
vcodec_status_t vcodec_ec_read_coeffs(vcodec_bitstream_reader_t *p_bitstream_reader, int *p_coeffs, int count);
