/* gcc gbm-scanout2.c -o gbm-scanout2 -O0 -ggdb3 -I/usr/include/libdrm -lgbm -ldrm */

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <gbm.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#define TRUE 1
#define FALSE 0

typedef struct {
    drmModeConnector *connector;
    drmModeRes *resources;
    drmModeModeInfo *mode;

    void *map_data;
    void *map_addr;

    uint32_t conn_id;
    uint32_t crtc_id;
    uint32_t fb_id;
} gbm_user_data_t;

static const char *dev = "/dev/dri/card0";
static int no_damage = FALSE;
static int no_modeset = FALSE;
static int no_master = FALSE;

static void
destroy_user_data(struct gbm_bo *bo, void *_data)
{
    struct gbm_device *gbm = gbm_bo_get_device(bo);
    int fd = gbm_device_get_fd(gbm);
    gbm_user_data_t* data = _data;
    if (!data) {
        return;
    }

    if (data->fb_id) {
        drmModeRmFB(fd, data->fb_id);
    }

    if (data->map_data) {
        gbm_bo_unmap(bo, data->map_data);
    }

    if (data->connector) {
        drmModeFreeConnector(data->connector);
    }

    if (data->resources) {
        drmModeFreeResources(data->resources);
    }

    free(data);
}

static inline int
gbm_bo_map_all(struct gbm_bo *bo, gbm_user_data_t *data)
{
    uint32_t stride = 0;

    if (!bo || !data) {
        return FALSE;
    }

    if (data->map_addr) {
        return TRUE;
    }

    uint32_t width = gbm_bo_get_width(bo);
    uint32_t height = gbm_bo_get_height(bo);

    /* must be NULL before the map call */
    data->map_data = NULL;

    /* While reading from gpu memory is often very slow, we do allow it */
    data->map_addr = gbm_bo_map(bo, 0, 0, width, height,
                                GBM_BO_TRANSFER_READ_WRITE,
                                &stride, &data->map_data);

    return !!data->map_addr;
}

static inline int
gbm_bo_map_or_free(struct gbm_bo *bo, gbm_user_data_t *data)
{
    if (gbm_bo_map_all(bo, data)) {
        return TRUE;
    }

    if (bo) {
        gbm_bo_destroy(bo);
    }
    return FALSE;
}

static inline struct gbm_bo*
gbm_bo_create_and_map_once(struct gbm_device *gbm,
                           gbm_user_data_t *data,
                           uint32_t width, uint32_t height,
                           uint32_t format, uint32_t flags)
{
    struct gbm_bo *ret = NULL;

    if (!data) {
        return NULL;
    }

    ret = gbm_bo_create(gbm, width, height, format, flags);
    if (ret && gbm_bo_map_or_free(ret, data)) {
        return ret;
    }

    return NULL;
}

static struct gbm_bo*
gbm_bo_create_and_map(struct gbm_device *gbm, gbm_user_data_t *data, uint32_t format)
{
    uint32_t width = data->mode ? data->mode->hdisplay : 1920;
    uint32_t height = data->mode ? data->mode->vdisplay : 1080;


    uint32_t flags = GBM_BO_USE_SCANOUT | GBM_BO_USE_FRONT_RENDERING;
    uint32_t flags2 = GBM_BO_USE_SCANOUT;

    uint32_t flags_dumb = GBM_BO_USE_SCANOUT | GBM_BO_USE_WRITE;

    struct gbm_bo *bo = NULL;

#if 0 /* non-GBM_BO_USE_WRITE buffers require unmap/map to flush writes */
    bo = gbm_bo_create_and_map_once(gbm, data, width, height, format, flags);
    if (!bo) {
        bo = gbm_bo_create_and_map_once(gbm, data, width, height, format, flags2);
    }
#endif
    if (!bo) {
        bo = gbm_bo_create_and_map_once(gbm, data, width, height, format, flags_dumb);
    }

#if 0
    if (!bo) {
        bo = gbm_bo_create(gbm, width, height, format, flags);
    }

    if (!bo) {
        bo = gbm_bo_create(gbm, width, height, format, flags2);
    }
#endif

    return bo;
}

int
gbm_format_get_depth(uint32_t format)
{
    switch (format) {
    case GBM_FORMAT_R8:
    case GBM_FORMAT_C8:
        return 8;
    case GBM_FORMAT_XRGB1555:
    case GBM_FORMAT_XBGR1555:
        return 15;
    case GBM_FORMAT_RGB565:
    case GBM_FORMAT_BGR565:
        return 16;
    case GBM_FORMAT_RGB888:
    case GBM_FORMAT_BGR888:
    case GBM_FORMAT_XRGB8888:
    case GBM_FORMAT_XBGR8888:
    default:
        return 24;
    case GBM_FORMAT_XRGB2101010:
    case GBM_FORMAT_XBGR2101010:
        return 30;
    case GBM_FORMAT_XBGR16161616F:
        return 48;
    }
}

static int
gbm_bo_create_fb(struct gbm_bo *bo)
{
    struct gbm_device *gbm = gbm_bo_get_device(bo);
    int fd = gbm_device_get_fd(gbm);

    uint32_t width = gbm_bo_get_width(bo);
    uint32_t height = gbm_bo_get_height(bo);
    uint32_t pitch = gbm_bo_get_stride(bo);
    uint32_t handle = gbm_bo_get_handle(bo).u32;
    uint32_t fb_id = 0;

    uint32_t format = gbm_bo_get_format(bo);
    int depth = gbm_format_get_depth(format);
    int bpp = gbm_bo_get_bpp(bo);

printf("depth: %d, bpp: %d\n", depth, bpp);

    int ret = drmModeAddFB(fd, width, height, depth, bpp, pitch, handle, &fb_id);
    return ret ? 0 : fb_id;
}

static int
modesetting_grade_mode(drmModeModeInfo *mode, uint32_t req_w, uint32_t req_h, uint32_t req_rate)
{
    int score = 1;

    if (req_w && (req_w == mode->hdisplay)) {
        score += 10;
    }

    if (req_h && (req_h == mode->vdisplay)) {
        score += 10;
    }

    if (req_rate && (req_rate == mode->vrefresh)) {
        score += 5;
    }

    if (mode->type & DRM_MODE_TYPE_PREFERRED) {
        score++;
    }

    return score;
}

static drmModeModeInfo*
modesetting_find_mode(drmModeConnector *conn, uint32_t req_w, uint32_t req_h, uint32_t req_rate)
{
    drmModeModeInfo *best_mode = NULL;
    int best_score = 0;

    for (int i = 0; i < conn->count_modes; i++) {
        drmModeModeInfo *mode = &conn->modes[i];
        int score;

        score = modesetting_grade_mode(mode, req_w, req_h, req_rate);
        if (score <= best_score) {
            continue;
        }

        best_mode = mode;
        best_score = score;
    }

    return best_mode;
}

static int
modesetting_grade_connector(drmModeConnector *conn)
{
    int score = 1;

    if (conn->modes && conn->count_modes) {
        score += 5;
    }

    switch(conn->connection) {
    case DRM_MODE_CONNECTED:
        score++;
    case DRM_MODE_UNKNOWNCONNECTION:
        score++;
    case DRM_MODE_DISCONNECTED:
        score++;
    }

    return score;
}


static drmModeConnector*
modesetting_find_connector(drmModeRes *res, int fd, uint32_t *conn_id)
{
    drmModeConnector *best_connector = NULL;
    int best_score = 0;

    for (int i = 0; i < res->count_connectors; i++) {
        drmModeConnector *conn;
        int id;
        int score;
        id = res->connectors[i];
        conn = drmModeGetConnector(fd, id);
        if (!conn) {
            continue;
        }

        score = modesetting_grade_connector(conn);
        if (score <= best_score) {
            drmModeFreeConnector(conn);
            continue;
        }

        if (best_connector) {
            drmModeFreeConnector(best_connector);
        }

        best_connector = conn;
        *conn_id = id;
        best_score = score;
    }

    return best_connector;
}

/* From man drm-kms */
static int
modeseting_find_crtc(int fd, drmModeRes *res, drmModeConnector *conn)
{
    drmModeEncoder *enc;
    unsigned int i, j;

    /* iterate all encoders of this connector */
    for (i = 0; i < conn->count_encoders; ++i) {
        enc = drmModeGetEncoder(fd, conn->encoders[i]);
        if (!enc) {
            /* cannot retrieve encoder, ignoring... */
            continue;
        }

        /* iterate all global CRTCs */
        for (j = 0; j < res->count_crtcs; ++j) {
            /* check whether this CRTC works with the encoder */
            if (!(enc->possible_crtcs & (1 << j)))
                continue;

            /* Here you need to check that no other connector
             * currently uses the CRTC with id "crtc". If you intend
             * to drive one connector only, then you can skip this
             * step. Otherwise, simply scan your list of configured
             * connectors and CRTCs whether this CRTC is already
             * used. If it is, then simply continue the search here. */
            drmModeFreeEncoder(enc);
            return res->crtcs[j];
        }

        drmModeFreeEncoder(enc);
    }

    /* cannot find a suitable CRTC */
    return -ENOENT;
}

static struct gbm_bo*
modesetting_open(struct gbm_device *gbm, int w, int h, int r, uint32_t format)
{
    int fd = gbm_device_get_fd(gbm);
    struct gbm_bo *ret = NULL;
    gbm_user_data_t *data = NULL;

    data = calloc(1, sizeof(*data));
    if (!data) {
        goto fail;
    }

    data->resources = drmModeGetResources(fd);
    if (!data->resources) {
        goto fail;
    }

    data->connector = modesetting_find_connector(data->resources, fd, &data->conn_id);
    if (!data->connector) {
        goto fail;
    }

    data->mode = modesetting_find_mode(data->connector, w, h, r);

    ret = gbm_bo_create_and_map(gbm, data, format);
    if (!ret) {
        goto fail;
    }

    gbm_bo_set_user_data(ret, data, destroy_user_data);

    data->fb_id = gbm_bo_create_fb(ret);
    if (!data->fb_id) {
        goto fail;
    }

    data->crtc_id = modeseting_find_crtc(fd, data->resources, data->connector);
    if (data->crtc_id < 0) {
        goto fail;
    }

    return ret;
fail:
    if (ret) {
        gbm_bo_destroy(ret);
        /* destroy_user_data takes care of the rest */
        return NULL;
    }

    if (data) {
        if (data->connector)
            drmModeFreeConnector(data->connector);
        if (data->resources)
            drmModeFreeResources(data->resources);
        free(data);
    }

    return NULL;
}

static int
modesetting_enable(struct gbm_bo *bo)
{
    struct gbm_device *gbm = gbm_bo_get_device(bo);
    gbm_user_data_t *data = gbm_bo_get_user_data(bo);
    int fd = gbm_device_get_fd(gbm);

    if (!no_master) {
        drmSetMaster(fd);
    }

    return no_modeset || !drmModeSetCrtc(fd, data->crtc_id, data->fb_id, 0, 0, &data->conn_id, !!data->mode, data->mode);
}

static void
modesetting_disable(struct gbm_bo *bo)
{
    struct gbm_device *gbm = gbm_bo_get_device(bo);
    gbm_user_data_t *data = gbm_bo_get_user_data(bo);
    int fd = gbm_device_get_fd(gbm);

    if (!no_modeset) {
        drmModeSetCrtc(fd, data->crtc_id, data->fb_id, 0, 0, &data->conn_id, 0, NULL);
    }

    if (!no_master) {
        drmDropMaster(fd);
    }
}

static void
draw_frame(uint32_t *ptr, uint32_t width, uint32_t height, uint32_t pitch)
{
    static int flip = 0;

    uint32_t blue, red;

    if (!flip) {
        blue = 0xff;
        red = 0xff << 16;
    } else {
        blue = 0xff << 16;
        red = 0xff;
    }

    flip = !flip;

    for (int i = 0; i < height / 2; i++) {
        for (int j = 0; j < width; j++) {
            ptr[i * pitch + j] = blue;
        }
    }

    for (int i = height / 2; i < height; i++) {
        for (int j = 0; j < width; j++) {
            ptr[i * pitch + j] = red;
        }
    }
}

volatile int done = 0;

static void sigint_handler(int arg)
{
    done = 1;
}

int main(int ac, char **av)
{
    int fd = -1;
    drmVersionPtr version = NULL;

    struct gbm_device *gbm = NULL;
    struct gbm_bo *bo = NULL;
    gbm_user_data_t *data = NULL;

    uint32_t *ptr = NULL;
    uint32_t fb_id = 0;
    uint32_t width = 0, height = 0, pitch = 0;

    uint32_t format = GBM_FORMAT_XRGB8888;

    for (int i = 0; i < ac; i++) {
        if ((i + 1) < ac && !strcmp(av[i], "-dev")) {
            dev = av[i + 1];
            continue;
        }

        if (!strcmp(av[i], "-no-damage")) {
            no_damage = TRUE;
            continue;
        }

        if (!strcmp(av[i], "-no-modeset")) {
            no_modeset = TRUE;
            continue;
        }

        if (!strcmp(av[i], "-no-master")) {
            no_master = TRUE;
            continue;
        }
    }

    fd = open(dev, O_RDWR);
    printf("fd: %d\n", fd);
    if (fd < 0) {
        goto fail;
    }

    version = drmGetVersion(fd);
    if (version) {
        printf("driver name: %s\n", version->name);
        drmFreeVersion(version);
    }

    gbm = gbm_create_device(fd);
    printf("gbm: %p\n", gbm);
    if (!gbm) {
        goto fail;
    }

    printf("backend name: %s\n", gbm_device_get_backend_name(gbm));

    bo = modesetting_open(gbm, 0, 0, 0, format);
    printf("bo: %p\n", bo);
    if (!bo) {
        goto fail;
    }

    if (!modesetting_enable(bo)) {
        goto fail;
    }

    data = gbm_bo_get_user_data(bo);
    ptr = data->map_addr;
    fb_id = data->fb_id;

    width = gbm_bo_get_width(bo);
    height = gbm_bo_get_height(bo);
    pitch = gbm_bo_get_stride(bo) / ((gbm_bo_get_bpp(bo) + 7) / 8);

    signal(SIGINT, sigint_handler);

    drmModeClip full_clip = (drmModeClip){.x1 = 0, .y1 = 0, .x2 = width, .y2 = height,};

    while(!done) {
        draw_frame(ptr, width, height, pitch);
        if (!no_damage) {
            drmModeDirtyFB(fd, fb_id, &full_clip, 1); /* Without this, the screen doesn't get updated */
        }
//modesetting_disable(bo);
        sleep(1);
        printf("flip\n");
    }

fail:
    if (bo) {
        gbm_bo_destroy(bo);
    }

    if (gbm) {
        gbm_device_destroy(gbm);
    }

    if (fd >= 0) {
        close(fd);
    }

    return 0;
}
