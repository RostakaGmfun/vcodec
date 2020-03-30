#include "tools/source.h"
#include "tools/y4m.h"

vcodec_status_t vcodec_source_init(vcodec_source_t *p_ctx, vcodec_source_type_t source_type, const char *path) {
    p_ctx->source_type = source_type;
    switch (source_type) {
    case VCODEC_SOURCE_Y4M:
        return vcodec_y4m_init(p_ctx, path);
    default:
        return VCODEC_STATUS_INVAL;
    }
}
