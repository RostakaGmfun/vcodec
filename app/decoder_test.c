#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include "vcodec/vcodec.h"
#include "tools/source.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

typedef struct {
    uint32_t data_size;
    uint64_t current_frame_size;
    FILE    *out_file;
    uint32_t out_size;
} io_ctx_t;

static vcodec_status_t vcodec_read(uint8_t *p_data, uint32_t size, uint32_t *size_read, void *ctx) {
    io_ctx_t *p_io_ctx = ctx;
    int read = fread(p_data, 1, size, p_io_ctx->out_file);
    if (read <= 0) {
        if (feof(p_io_ctx->out_file)) {
            return VCODEC_STATUS_EOF;
        } else {
            return VCODEC_STATUS_IO_FAILED;
        }
    }
    *size_read = read;

    return VCODEC_STATUS_OK;
}

static void *vcodec_alloc(size_t size) {
    return malloc(size);
}

static void vcodec_free(void *ptr) {
    return free(ptr);
}

static void print_vcodec_stats(const vcodec_dec_ctx_t *p_ctx, clock_t diff) {
    static int cnt = 0;
    const io_ctx_t *p_io_ctx = p_ctx->io_ctx;
    static uint64_t total_size = 0;
    static float avg_fps = 0;
    static uint64_t total_time = 0;
    cnt++;
    total_size += p_io_ctx->data_size;
    avg_fps += ((diff * 1000) / CLOCKS_PER_SEC);
    total_time += diff;
    if (cnt % 100 == 0) {
        fprintf(stderr, "Compressed size %uK, ratio %f%%, avg fps %fms, total time %lus\n", p_io_ctx->out_size >> 10,
                ((float)p_io_ctx->out_size / total_size) * 100, avg_fps / cnt, total_time / CLOCKS_PER_SEC);
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Wrong number of args %d\n", argc);
        return EXIT_FAILURE;
    }
    io_ctx_t io_ctx = { 0 };
    vcodec_dec_ctx_t vcodec_dec_ctx = {
        .read  = vcodec_read,
        .alloc  = vcodec_alloc,
        .free   = vcodec_free,
        .io_ctx = &io_ctx,
    };

    io_ctx.out_file = fopen(argv[1], "rb");
    if (NULL == io_ctx.out_file) {
        fprintf(stderr, "Failed to open output file\n");
        return 1;
    }
    const int width = 640;
    const int height = 360;
    fprintf(stdout, "YUV4MPEG2 W%d H%d F%d:%d I%c A%d:%d C%s\n", width, height, 30, 1, 'p', 0, 0, "mono");

    uint8_t *p_framebuffer = malloc(width * height);
    if (NULL == p_framebuffer) {
        fprintf(stderr, "Failed to allocate framebuffer of size %u\n", width * height);
        return 1;
    }


    vcodec_dec_ctx.width = width;
    vcodec_dec_ctx.height = height;
    vcodec_status_t ret = vcodec_dec_init(&vcodec_dec_ctx, VCODEC_TYPE_DCT);
    if (ret != VCODEC_STATUS_OK) {
        fprintf(stderr, "Failed to initialize vcodec %d for %dx%d\n", ret, vcodec_dec_ctx.width, vcodec_dec_ctx.height);
        return EXIT_FAILURE;
    }

    int num_frames = 0;
    vcodec_status_t vcodec_ret = VCODEC_STATUS_OK;
    while (1) {
        const clock_t start_time = clock();
        ret = vcodec_dec_ctx.get_frame(&vcodec_dec_ctx, p_framebuffer);
        const clock_t end_time = clock();

        fprintf(stdout, "FRAME\n");
        fwrite(p_framebuffer, width * height, 1, stdout);

        if (VCODEC_STATUS_OK != ret) {
            fprintf(stderr, "vcodec error %d", ret);
            break;
        }

        //print_vcodec_stats(&vcodec_dec_ctx, end_time - start_time);
        num_frames++;
    }

    fprintf(stderr, "Finish encoding, status %d\n", vcodec_ret);
    fclose(io_ctx.out_file);

    vcodec_dec_ctx.deinit(&vcodec_dec_ctx);

    return 0;
}
