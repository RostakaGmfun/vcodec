#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include "vcodec/vcodec.h"
#include "tools/source.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

typedef struct {
    int width;
    int height;
    int bpp;
    uint8_t *p_data;
} pgm_file_t;

typedef struct {
    uint32_t data_size;
    uint64_t current_frame_size;
    FILE    *out_file;
    uint32_t out_size;
} io_ctx_t;

#define FILE_NOT_FOUND -2

static int pgm_read(const char *path, pgm_file_t *file) {
    FILE *f = fopen(path, "rb");
    if (NULL == f) {
        return FILE_NOT_FOUND;
    }

    const int old_size = file->width * file->height * file->bpp / 8;

    int maxval;
    file->bpp = 8;
    if (fscanf(f, "P5\n%d %d\n%d", &file->width, &file->height, &maxval) != 3) {
        fprintf(stderr, "Bad header format\n");
        fclose(f);
        return -1;
    }
    char c = 0;
    if (fread(&c, 1, 1, f) != 1 || !isspace(c)) {
        fprintf(stderr, "Bad header format\n");
        fclose(f);
        return -1;
    }

    if (maxval != 255) {
        fprintf(stderr, "Unsupported bpp, maxval %d\n", maxval);
        fclose(f);
        return -1;
    }

    const size_t image_size = file->width * file->height;
    if (NULL == file->p_data) {
        file->p_data = malloc(image_size);
    } else if (old_size < image_size) {
        file->p_data = realloc(file->p_data, image_size);
    }
    if (NULL == file->p_data) {
        fprintf(stderr, "Malloc failed\n");
        fclose(f);
        return -1;
    }

    size_t ret = fread(file->p_data, image_size, 1, f);
    if (ret != 1) {
        fprintf(stderr, "read failed %d %d %d %d %ld %ld\n", ferror(f), feof(f), file->width, file->height, ftell(f), ret);
        fclose(f);
        return -1;
    }

    fclose(f);
    return 0;
}

static vcodec_status_t vcodec_write(const uint8_t *p_data, uint32_t size, void *ctx) {
    io_ctx_t *p_io_ctx = ctx;
    return VCODEC_STATUS_OK;
    if (fwrite(p_data, size, 1, p_io_ctx->out_file) != 1) {
        return VCODEC_STATUS_IO_FAILED;
    }

    p_io_ctx->out_size += size;
    p_io_ctx->current_frame_size += size;

    return VCODEC_STATUS_OK;
}

static void *vcodec_alloc(size_t size) {
    return malloc(size);
}

static void vcodec_free(void *ptr) {
    return free(ptr);
}

static void print_vcodec_stats(const vcodec_enc_ctx_t *p_ctx, clock_t diff) {
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
    if (argc != 2 && argc != 3) {
        fprintf(stderr, "Wrong number of args %d\n", argc);
        return EXIT_FAILURE;
    }
    io_ctx_t io_ctx = { 0 };
    vcodec_enc_ctx_t vcodec_enc_ctx = {
        .write  = vcodec_write,
        .alloc  = vcodec_alloc,
        .free   = vcodec_free,
        .io_ctx = &io_ctx,
    };

    vcodec_source_t source_ctx = { 0 };
    if (vcodec_source_init(&source_ctx, VCODEC_SOURCE_Y4M, argv[1]) != VCODEC_STATUS_OK) {
        fprintf(stderr, "Failed to initialize vcodec source from %s\n", argv[1]);
        return 1;
    }

    io_ctx.out_file = stdout;
    if (NULL == io_ctx.out_file) {
        fprintf(stderr, "Failed to open output file\n");
        return 1;
    }
    //fprintf(io_ctx.out_file, "YUV4MPEG2 W%d H%d F%d:%d I%c A%d:%d C%s\n", source_ctx.width, source_ctx.height, 30, 1, 'p', 0, 0, "mono");

    uint8_t *p_framebuffer = malloc(source_ctx.frame_size);
    if (NULL == p_framebuffer) {
        fprintf(stderr, "Failed to allocate framebuffer of size %u\n", source_ctx.frame_size);
        return 1;
    }

    vcodec_enc_ctx.width = source_ctx.width;
    vcodec_enc_ctx.height = source_ctx.height;
    vcodec_status_t ret = vcodec_enc_init(&vcodec_enc_ctx, VCODEC_TYPE_DCT);
    if (ret != VCODEC_STATUS_OK) {
        fprintf(stderr, "Failed to initialize vcodec %d for %dx%d\n", ret, vcodec_enc_ctx.width, vcodec_enc_ctx.height);
        return EXIT_FAILURE;
    }

    fprintf(stderr, "Initialized from %s: %dx%d %u bytes/frame\n", argv[1], source_ctx.width, source_ctx.height, source_ctx.frame_size);

    int num_frames = 0;
    vcodec_status_t vcodec_ret = VCODEC_STATUS_OK;
    while (VCODEC_STATUS_OK == (vcodec_ret = source_ctx.read_frame(&source_ctx, p_framebuffer))) {
        const clock_t start_time = clock();
        ret = vcodec_enc_ctx.process_frame(&vcodec_enc_ctx, p_framebuffer);
        const clock_t end_time = clock();
        io_ctx.data_size = source_ctx.frame_size;

        if (VCODEC_STATUS_OK != ret) {
            fprintf(stderr, "vcodec error %d", ret);
            break;
        }

        print_vcodec_stats(&vcodec_enc_ctx, end_time - start_time);
        num_frames++;
        fprintf(stderr, "Frame size %lu\n", io_ctx.current_frame_size);
        io_ctx.current_frame_size = 0;
    }

    fprintf(stderr, "Finish encoding, status %d\n", vcodec_ret);
    fclose(io_ctx.out_file);

    vcodec_enc_ctx.deinit(&vcodec_enc_ctx);
    return 0;
}
