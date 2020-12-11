#include <unity.h>
#include <unity_fixture.h>
#include <string.h>

#include "vcodec_common.h"

TEST_GROUP(motion_prediction_tests);

typedef struct {
    int mvx;
    int mvy;
    int cost;
    bool visited;
} cost_t;

cost_t *p_current_cost_table;
int cost_table_size;

static int mock_cost_function(const uint8_t *p_source_frame, const uint8_t *p_ref_frame, int x, int y, int mvx, int mvy, int block_size, int frame_width) {
    for (int i = 0; i < cost_table_size; i++) {
        cost_t *c = p_current_cost_table + i;
        if (mvx == c->mvx && mvy == c->mvy) {
            c->visited = true;
            return c->cost;
        }
    }
    TEST_ASSERT_FAIL_MESSAGE("Unexpected cost function arguments");
}

TEST_SETUP(motion_prediciton_tests) {

}

TEST_TEAR_DOWN(motion_prediciton_tests) {

}

/**
 * Block size: 4 -> 3*3*3 - 2 = 25 block comparisons
 *
 * 0  0  0  0  0  0  0  0
 * 0  0  0  0  0  0  0  0
 * 0  0  0  0  0  0  0  0
 * 0  0  0  0  0  0  0  0
 * 0  0  0  0  0  0  0  0
 * 0  0  0  0  0  0  0  0
 * 0  0  0  0  0  0  0  0
 * 0  0  0  0  0  0  0  0
 */
static cost_t tss_test_vector_1[] = {
    {
        0, 0, 1
    },
};

TEST(motion_prediciton_tests, test_match_block_tss) {
    p_current_cost_table = tss_test_vector_1;
    cost_table_size = sizeof(tss_test_vector_1) / sizeof(tss_test_vector_1[0]);
    for (int i = 0; i < cost_table_size; i++) {

    }
}

TEST_GROUP_RUNNER(motion_prediciton_tests)
{
    RUN_TEST_CASE(motion_prediciton_tests, test_vcodec_ec_read_write_coeffs);
}
