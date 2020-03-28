#pragma once

#include <stdint.h>

typedef struct v4l_capture_context v4l_capture_context_t;

v4l_capture_context_t *v4l_capture_init(const char *path, int width, int height);
int v4l_capture_get_framebuffer_size(v4l_capture_context_t *p_ctx);
int v4l_capture_read_frame(v4l_capture_context_t *p_ctx, uint8_t *p_framebuffer);
int v4l_capture_close(v4l_capture_context_t *p_ctx);
