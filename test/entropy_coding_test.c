#include <unity.h>
#include <unity_fixture.h>
#include <string.h>

#include "vcodec_entropy_coding.h"
#include "vcodec/bitstream.h"

TEST_GROUP(entropy_coding_tests);

#define TEST_IO_BUFFER_SIZE 1024

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

static vcodec_status_t read_mock(uint8_t *p_data, uint32_t size, uint32_t *bytes_read, void *ctx) {
    *bytes_read = MIN(size, sizeof(io_ctx.buffer) - io_ctx.cursor);
    memcpy(p_data, io_ctx.buffer + io_ctx.cursor, *bytes_read);
    io_ctx.cursor += *bytes_read;
    return VCODEC_STATUS_OK;
}

TEST_SETUP(entropy_coding_tests) {
    memset(&io_ctx, 0, sizeof(io_ctx));
}

TEST_TEAR_DOWN(entropy_coding_tests) {

}

TEST(entropy_coding_tests, test_vcodec_ec_read_write_coeffs) {
    vcodec_bitstream_reader_t reader = {
        .read = read_mock,
    };

    vcodec_bitstream_writer_t writer = {
        .write = write_mock,
    };

    static const int test_vectors16[][16] = {
        {
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
        },
        {
            30, -1, 0, 0,
            0, -10, 0, 0,
            0, 2, 5, 0,
            0, 0, 0, -1,
        },
        {
            -1, 2, 0, -1,
            -1, 0, -1, -1,
            0, 0, 0, 0,
            0, 0, 0, 0,
        },
    };
    const uint32_t num_test_vectors16 = sizeof(test_vectors16) / sizeof(test_vectors16[0]);

    for (uint32_t i = 0; i < num_test_vectors16; i++) {
        vcodec_ec_write_coeffs(&writer, test_vectors16[i], 16);
    }

    vcodec_bitstream_writer_flush(&writer);
    io_ctx.cursor = 0;

    int result_vectors16[num_test_vectors16][16];

    for (uint32_t i = 0; i < num_test_vectors16; i++) {
        const vcodec_status_t ret = vcodec_ec_read_coeffs(&reader, result_vectors16[i], 16);
        TEST_ASSERT_EQUAL(VCODEC_STATUS_OK, ret);
        TEST_ASSERT_EQUAL_INT_ARRAY(test_vectors16[i], result_vectors16[i], 16);
    }
}

TEST_GROUP_RUNNER(entropy_coding_tests)
{
    RUN_TEST_CASE(entropy_coding_tests, test_vcodec_ec_read_write_coeffs);
}
