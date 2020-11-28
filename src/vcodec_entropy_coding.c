#include "vcodec_entropy_coding.h"

#include <stdlib.h>
#include "vcodec/bitstream.h"

void vcodec_ec_write_coeffs(vcodec_bitstream_writer_t *p_bitstream_writer, const int *p_coeffs, int count) {
    int num_zeroes = 0;
    uint32_t sign_buffer = 0;
    int sign_buffer_size = 0;
    while (count > 0) {
        if (0 == p_coeffs[count - 1]) {
            num_zeroes++;
        } else {
            vcodec_bitstream_writer_write_exp_golomb(p_bitstream_writer, num_zeroes);
            num_zeroes = 0;
            sign_buffer <<= 1;
            sign_buffer |= (p_coeffs[count - 1] > 0);
            sign_buffer_size++;
            vcodec_bitstream_writer_write_exp_golomb(p_bitstream_writer, abs(p_coeffs[count - 1]) - 1);
        }
        count--;
    }
    if (0 != num_zeroes) {
        vcodec_bitstream_writer_write_exp_golomb(p_bitstream_writer, num_zeroes);
    }
    vcodec_bitstream_writer_putbits(p_bitstream_writer, sign_buffer, sign_buffer_size);
}

vcodec_status_t vcodec_ec_read_coeffs(vcodec_bitstream_reader_t *p_bitstream_reader, int *p_coeffs, int count) {
    int num_zeroes = 0;
    uint32_t sign_buffer = 0;
    uint32_t sign_buffer_size = 0;
    const int num_coeffs = count;
    while (count > 0) {
        num_zeroes = vcodec_bitstream_reader_read_exp_golomb(p_bitstream_reader);
        for (int i = count; i > 0; i--) {
            p_coeffs[i - 1] = 0;
        }
        count -= num_zeroes;
        if (count == 0) {
            break;
        }
        p_coeffs[count - 1] = vcodec_bitstream_reader_read_exp_golomb(p_bitstream_reader) + 1;
        sign_buffer_size++;
        count--;
    }
    vcodec_bitstream_reader_getbits(p_bitstream_reader, &sign_buffer, sign_buffer_size);
    for (int i = 0; i < num_coeffs; i++) {
        if (p_coeffs[i] != 0) {
            p_coeffs[i] = (sign_buffer & (1)) ? p_coeffs[i] : -p_coeffs[i];
            sign_buffer >>= 1;
        }
    }
    return vcodec_bitstream_reader_status(p_bitstream_reader);
}
