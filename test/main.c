#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

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
        printf("read failed %d %d %d %d %ld %ld\n", ferror(f), feof(f), file->width, file->height, ftell(f), ret);
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
        printf("Compressed size %luM, ratio %f%%, avg fps %fms, total time %ds\n", p_io_ctx->current_frame_size >> 20,
                ((float)p_io_ctx->current_frame_size / total_size) * 100, avg_fps / cnt, total_time / CLOCKS_PER_SEC);
    }
}

int main(int argc, char **argv) {
    if (argc < 3) {
        return EXIT_FAILURE;
    }
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

    int num_files = 0;
    pgm_file_t pgm_file = { 0 };
    while (1) {
        char filename[256];
        snprintf(filename, sizeof(filename), argv[1], num_files + 1);
        int pgm_ret = pgm_read(filename, &pgm_file);
        if (FILE_NOT_FOUND == pgm_ret) {
            printf("Finished processing %d files\n", num_files);
            if (NULL != io_ctx.out_file) {
                fclose(io_ctx.out_file);
            }
            return 0;
        }
        if (0 != pgm_read(filename, &pgm_file)) {
            fprintf(stderr, "Failed to read PGM file %s\n", filename);
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
            io_ctx.out_file = fopen(argv[2], "wb");
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

        const clock_t start_time = clock();
        ret = vcodec_enc_process_frame(&vcodec_enc_ctx);
        const clock_t end_time = clock();

        if (VCODEC_STATUS_OK != ret) {
            fprintf(stderr, "vcodec error %d", ret);
            break;
        }

        print_vcodec_stats(&vcodec_enc_ctx, end_time - start_time);
        num_files++;
    }

    vcodec_enc_deinit(&vcodec_enc_ctx);
    return 0;
}
