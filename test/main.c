#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "vcodec/vcodec.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

typedef struct {
    int width;
    int height;
    int bpp;
    uint8_t *p_data;
} pgm_file_t;

typedef struct {
    uint8_t *p_data;
    uint32_t data_size;
    uint32_t offset;
    uint32_t current_frame_size;
    FILE    *out_file;
    uint32_t out_size;
} io_ctx_t;

static int pgm_read(const char *path, pgm_file_t *file) {
    FILE *f = fopen(path, "rb");
    if (NULL == f) {
        return -1;
    }

    int maxval;
    file->bpp = 8;
    if (fscanf(f, "P5\n%d %d\n%d\n", &file->width, &file->height, &maxval) != 3) {
        fclose(f);
        return -1;
    }

    if (maxval != 255) {
        printf("Unsupported bpp, maxval %d\n", maxval);
        fclose(f);
        return -1;
    }

    const size_t image_size = file->width * file->height;
    file->p_data = malloc(image_size);
    if (NULL == file->p_data) {
        fclose(f);
        return -1;
    }

    if (fread(file->p_data, image_size, 1, f) != 1) {
        fclose(f);
        return -1;
    }

    fclose(f);
    return 0;
}

static vcodec_status_t vcodec_write(const uint8_t *p_data, uint32_t size, void *ctx) {
    io_ctx_t *p_io_ctx = ctx;
    if (fwrite(p_data, size, 1, p_io_ctx->out_file) != 1) {
        return VCODEC_STATUS_IO_FAILED;
    }

    p_io_ctx->out_size += size;
    p_io_ctx->current_frame_size += size;

    return VCODEC_STATUS_OK;
}

static vcodec_status_t vcodec_read(uint8_t *p_data, uint32_t *p_size, void *ctx) {
    io_ctx_t *p_io_ctx = ctx;
    const uint32_t to_copy = MIN(*p_size, p_io_ctx->data_size - p_io_ctx->offset);
    memcpy(p_data, p_io_ctx->p_data + p_io_ctx->offset, to_copy);
    *p_size = to_copy;
    p_io_ctx->offset += to_copy;
    return VCODEC_STATUS_OK;
}

static void *vcodec_alloc(size_t size) {
    return malloc(size);
}

static void vcodec_free(void *ptr) {
    return free(ptr);
}

static void print_vcodec_stats(const vcodec_enc_ctx_t *p_ctx, clock_t diff) {
    const io_ctx_t *p_io_ctx = p_ctx->io_ctx;
    printf("Compressed size %u, ratio %f%%, time %fms\n", p_io_ctx->current_frame_size,
            ((float)p_io_ctx->current_frame_size / p_io_ctx->data_size) * 100, ((float)diff * 1000) / CLOCKS_PER_SEC);
}

int main(int argc, char **argv) {
    io_ctx_t io_ctx = { 0 };
    vcodec_enc_ctx_t vcodec_enc_ctx = {
        .width  = 0,
        .height = 0,
        .write  = vcodec_write,
        .read   = vcodec_read,
        .alloc  = vcodec_alloc,
        .free   = vcodec_free,
        .io_ctx = &io_ctx,
    };

    for (int i = 1; i < argc; i++) {
        pgm_file_t pgm_file;
        if (0 != pgm_read(argv[i], &pgm_file)) {
            fprintf(stderr, "Failed to read PGM file %s\n", argv[i]);
            return EXIT_FAILURE;
        }

        vcodec_status_t ret = VCODEC_STATUS_OK;
        if (0 == vcodec_enc_ctx.width) {
            vcodec_enc_ctx.width = pgm_file.width;
            vcodec_enc_ctx.height = pgm_file.height;
            ret = vcodec_enc_init(&vcodec_enc_ctx);
            if (ret != VCODEC_STATUS_OK) {
                fprintf(stderr, "Failed to initialize vcodec %d for %dx%d\n", ret, vcodec_enc_ctx.width, vcodec_enc_ctx.height);
                free(pgm_file.p_data);
                return EXIT_FAILURE;
            }
            io_ctx.out_file = fopen("result.vcodec", "wb");
            if (NULL == io_ctx.out_file) {
                fprintf(stderr, "Failed to open output file");
                free(pgm_file.p_data);
                return EXIT_FAILURE;
            }
            setvbuf(io_ctx.out_file, NULL, _IOFBF, 8 * 1024 * 1024); // 8MB buffering
            printf("vcodec initialized for %dx%d\n", vcodec_enc_ctx.width, vcodec_enc_ctx.height);
        }

        io_ctx.p_data = pgm_file.p_data;
        io_ctx.data_size = pgm_file.width * pgm_file.height;
        io_ctx.offset = 0;
        io_ctx.current_frame_size = 0;

        const clock_t start_time = clock();
        ret = vcodec_enc_process_frame(&vcodec_enc_ctx);
        const clock_t end_time = clock();

        free(pgm_file.p_data);

        if (VCODEC_STATUS_OK != ret) {
            fprintf(stderr, "vcodec error %d", ret);
            break;
        }

        print_vcodec_stats(&vcodec_enc_ctx, end_time - start_time);
    }

    vcodec_enc_deinit(&vcodec_enc_ctx);
    return 0;
}
