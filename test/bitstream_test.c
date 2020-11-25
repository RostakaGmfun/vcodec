#include <unity.h>
#include <unity_fixture.h>
#include <string.h>

#include "vcodec/bitstream.h"

TEST_GROUP(bitstream_tests);

#define TEST_IO_BUFFER_SIZE 32

static struct {
    uint8_t buffer[TEST_IO_BUFFER_SIZE];
    uint32_t cursor;
} io_ctx;

static vcodec_status_t write_mock(const uint8_t *p_data, uint32_t size, void *ctx) {
    TEST_ASSERT_LESS_THAN(TEST_IO_BUFFER_SIZE + 1, io_ctx.cursor + size);
    memcpy(io_ctx.buffer + io_ctx.cursor, p_data, size);
    io_ctx.cursor += size;
    return VCODEC_STATUS_OK;
}

TEST_SETUP(bitstream_tests) {
    memset(&io_ctx, 0, sizeof(io_ctx));
}

TEST_TEAR_DOWN(bitstream_tests) {

}

TEST(bitstream_tests, test_bitstream_writer) {
    vcodec_bitstream_writer_t writer = {
        .write = write_mock,
    };
    _Static_assert(VCODEC_BITSTREAM_WRITER_BUFFER_LEN >= 4, "bitstream writer buffer too small for unit tests");

    vcodec_bitstream_writer_putbits(&writer, 0xabcd, 16);
    TEST_ASSERT_EQUAL(0xab, writer.buffer[0]);
    TEST_ASSERT_EQUAL(0xcd, writer.buffer[1]);
    vcodec_bitstream_writer_putbits(&writer, 0xa, 4);
    vcodec_bitstream_writer_putbits(&writer, 0xb, 4);
    TEST_ASSERT_EQUAL(0xab, writer.buffer[2]);
    vcodec_bitstream_writer_reset(&writer);

    vcodec_bitstream_writer_putbits(&writer, 1, 1);
    TEST_ASSERT_EQUAL(0, io_ctx.cursor);
    TEST_ASSERT_EQUAL(0, io_ctx.buffer[io_ctx.cursor]);
    TEST_ASSERT_EQUAL(1, writer.bit_pos);
    TEST_ASSERT_EQUAL(0x80, writer.buffer[0]);

    vcodec_bitstream_writer_putbits(&writer, 1, 7);
    TEST_ASSERT_EQUAL(0, io_ctx.cursor);
    TEST_ASSERT_EQUAL(0, io_ctx.buffer[io_ctx.cursor]);
    TEST_ASSERT_EQUAL(8, writer.bit_pos);
    TEST_ASSERT_EQUAL(0x81, writer.buffer[0]);

    vcodec_bitstream_writer_putbits(&writer, 0x1a, 5);
    TEST_ASSERT_EQUAL(0, io_ctx.cursor);
    TEST_ASSERT_EQUAL(0, io_ctx.buffer[io_ctx.cursor]);
    TEST_ASSERT_EQUAL(8 + 5, writer.bit_pos);
    TEST_ASSERT_EQUAL(0x1a << 3, writer.buffer[1]);

    vcodec_bitstream_writer_putbits(&writer, 0x7ff, 11);
    TEST_ASSERT_EQUAL(0, io_ctx.cursor);
    TEST_ASSERT_EQUAL(0, io_ctx.buffer[io_ctx.cursor]);
    TEST_ASSERT_EQUAL(24, writer.bit_pos);
    const uint8_t expected = (0x1a << 3) | (0x7ff >> 8);
    TEST_ASSERT_EQUAL(expected, writer.buffer[1]);
    TEST_ASSERT_EQUAL(0xff, writer.buffer[2]);

    for (int i = 0; i < VCODEC_BITSTREAM_WRITER_BUFFER_LEN - 4; i++) {
        vcodec_bitstream_writer_putbits(&writer, 0xFF, 8);
        TEST_ASSERT_EQUAL(32 + i * 8, writer.bit_pos);
        TEST_ASSERT_EQUAL(0, io_ctx.cursor);
        TEST_ASSERT_EQUAL(0xff, writer.buffer[2 + i]);
    }

    vcodec_bitstream_writer_putbits(&writer, 0xFF, 8);
    TEST_ASSERT_EQUAL(0, writer.bit_pos);
    TEST_ASSERT_EQUAL(VCODEC_BITSTREAM_WRITER_BUFFER_LEN, io_ctx.cursor);
    TEST_ASSERT_EQUAL(0x81, io_ctx.buffer[0]);
    TEST_ASSERT_EACH_EQUAL_UINT8(0, writer.buffer, sizeof(writer.buffer));

    for (int i = 0; i < VCODEC_BITSTREAM_WRITER_BUFFER_LEN - 1; i++) {
        vcodec_bitstream_writer_putbits(&writer, 0x55, 8);
        TEST_ASSERT_EQUAL((i + 1) * 8, writer.bit_pos);
        TEST_ASSERT_EQUAL(VCODEC_BITSTREAM_WRITER_BUFFER_LEN, io_ctx.cursor);
        TEST_ASSERT_EQUAL(0x55, writer.buffer[i]);
    }

    vcodec_bitstream_writer_putbits(&writer, 0x55, 8);
    TEST_ASSERT_EQUAL(0, writer.bit_pos);
    TEST_ASSERT_EQUAL(2 * VCODEC_BITSTREAM_WRITER_BUFFER_LEN, io_ctx.cursor);
    TEST_ASSERT_EACH_EQUAL_UINT8(0x55, io_ctx.buffer + VCODEC_BITSTREAM_WRITER_BUFFER_LEN, VCODEC_BITSTREAM_WRITER_BUFFER_LEN);
    TEST_ASSERT_EACH_EQUAL_UINT8(0, writer.buffer, sizeof(writer.buffer));
}

TEST(bitstream_tests, test_bitstream_writer_zeroes_ones_flush) {
    vcodec_bitstream_writer_t writer = {
        .write = write_mock,
    };
    vcodec_bitstream_writer_putones(&writer, 10);
    TEST_ASSERT_EQUAL(0, io_ctx.cursor);
    TEST_ASSERT_EQUAL(0, io_ctx.buffer[0]);
    TEST_ASSERT_EQUAL(10, writer.bit_pos);
    TEST_ASSERT_EQUAL(0xFF, writer.buffer[0]);
    TEST_ASSERT_EQUAL(0xc0, writer.buffer[1]);
    vcodec_bitstream_writer_putzeroes(&writer, 6);
    TEST_ASSERT_EQUAL(0, io_ctx.cursor);
    TEST_ASSERT_EQUAL(0, io_ctx.buffer[io_ctx.cursor]);
    TEST_ASSERT_EQUAL(16, writer.bit_pos);
    TEST_ASSERT_EQUAL(0xc0, writer.buffer[1]);

    vcodec_bitstream_writer_putones(&writer, VCODEC_BITSTREAM_WRITER_BUFFER_LEN * 8 - 16);
    TEST_ASSERT_EQUAL(0, writer.bit_pos);
    TEST_ASSERT_EQUAL(VCODEC_BITSTREAM_WRITER_BUFFER_LEN, io_ctx.cursor);
    TEST_ASSERT_EACH_EQUAL_UINT8(0xFF, io_ctx.buffer + 2, VCODEC_BITSTREAM_WRITER_BUFFER_LEN - 2);
    TEST_ASSERT_EACH_EQUAL_UINT8(0, writer.buffer, sizeof(writer.buffer));

    vcodec_bitstream_writer_putones(&writer, 1);
    vcodec_bitstream_writer_flush(&writer);
    TEST_ASSERT_EQUAL(VCODEC_BITSTREAM_WRITER_BUFFER_LEN + 1, io_ctx.cursor);
    TEST_ASSERT_EQUAL(0x80, io_ctx.buffer[io_ctx.cursor - 1]);
}

TEST(bitstream_tests, test_bitstream_writer_exp_golomb) {
    vcodec_bitstream_writer_t writer = {
        .write = write_mock,
    };
    vcodec_bitstream_writer_write_exp_golomb(&writer, 0);
    TEST_ASSERT_EQUAL(0, io_ctx.cursor);
    TEST_ASSERT_EQUAL(0, io_ctx.buffer[0]);
    TEST_ASSERT_EQUAL(1, writer.bit_pos);
    TEST_ASSERT_EQUAL(0x80, writer.buffer[0]);
    vcodec_bitstream_writer_flush(&writer);
    TEST_ASSERT_EQUAL(1, io_ctx.cursor);
    TEST_ASSERT_EQUAL(0x80, io_ctx.buffer[0]);
    TEST_ASSERT_EQUAL(0, writer.bit_pos);
    TEST_ASSERT_EQUAL(0x00, writer.buffer[0]);

    memset(&io_ctx, 0, sizeof(io_ctx));
    // 1 is coded as 0b010
    vcodec_bitstream_writer_write_exp_golomb(&writer, 1);
    TEST_ASSERT_EQUAL(0, io_ctx.cursor);
    TEST_ASSERT_EQUAL(0, io_ctx.buffer[0]);
    TEST_ASSERT_EQUAL(3, writer.bit_pos);
    TEST_ASSERT_EQUAL(0x40, writer.buffer[0]);
    vcodec_bitstream_writer_flush(&writer);

    memset(&io_ctx, 0, sizeof(io_ctx));
    // 17 bits: 0b00000000100000001 -> 0x01, 0x01, 0x00
    vcodec_bitstream_writer_write_exp_golomb(&writer, 256);
    TEST_ASSERT_EQUAL(0, io_ctx.cursor);
    TEST_ASSERT_EQUAL(0, io_ctx.buffer[0]);
    TEST_ASSERT_EQUAL(17, writer.bit_pos);
    TEST_ASSERT_EQUAL(0x00, writer.buffer[0]);
    TEST_ASSERT_EQUAL(0x80, writer.buffer[1]);
    TEST_ASSERT_EQUAL(0x80, writer.buffer[1]);
}

TEST_GROUP_RUNNER(bitstream_tests)
{
    RUN_TEST_CASE(bitstream_tests, test_bitstream_writer);
    RUN_TEST_CASE(bitstream_tests, test_bitstream_writer_zeroes_ones_flush);
    RUN_TEST_CASE(bitstream_tests, test_bitstream_writer_exp_golomb);
}
