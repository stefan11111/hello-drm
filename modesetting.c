#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <gbm.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

#include "modesetting.h"

#define FALSE 0
#define TRUE 1

#ifndef GBM_BO_USE_FRONT_RENDERING
#define GBM_BO_USE_FRONT_RENDERING 0
#endif

typedef struct {
    drmModeConnector *connector;
    drmModeRes *resources;

    void *map_data;
    void *map_addr;

    uint32_t fb_id;
} gbm_user_data_t;

static inline void*
gbm_bo_get_map(struct gbm_bo *bo)
{
    gbm_user_data_t *data = gbm_bo_get_user_data(bo);
    return data ? data->map_addr : NULL;
}

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
gbm_bo_create_and_map(struct gbm_device *gbm, gbm_user_data_t *data)
{
    uint32_t width = data->connector->modes[0].hdisplay;
    uint32_t height = data->connector->modes[0].vdisplay;

    uint32_t format = GBM_FORMAT_XRGB8888;
    uint32_t flags = GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING | GBM_BO_USE_FRONT_RENDERING;
    uint32_t flags2 = GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING;
    uint32_t flags_dumb = GBM_BO_USE_SCANOUT | GBM_BO_USE_WRITE;

    struct gbm_bo *bo = NULL;

    bo = gbm_bo_create_and_map_once(gbm, data, width, height, format, flags);
    if (!bo) {
        bo = gbm_bo_create_and_map_once(gbm, data, width, height, format, flags2);
    }
    if (!bo) {
        bo = gbm_bo_create_and_map_once(gbm, data, width, height, format, flags_dumb);
    }

    return bo;
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

    int ret = drmModeAddFB(fd, width, height, 24, 32, pitch, handle, &fb_id);
    return ret ? 0 : fb_id;
}

static drmModeConnector*
modesetting_find_connector(drmModeRes *res, int fd, uint32_t *conn_id)
{
    for (int i = 0; i < res->count_connectors; i++) {
        drmModeConnector *conn;
        *conn_id = res->connectors[i];
        conn = drmModeGetConnector(fd, *conn_id);
        if (!conn) {
            continue;
        }

        if (conn->modes && conn->count_modes) {
            return conn;
        }

        drmModeFreeConnector(conn);
    }
    return NULL;
}

struct gbm_bo*
modesetting_open(const char* restrict filename)
{
    struct gbm_device *gbm = NULL;
    struct gbm_bo *ret = NULL;
    gbm_user_data_t *data = NULL;

    int fd;

    uint32_t conn_id;
    uint32_t crtc_id;

    if (!filename) {
        printf("No filename was provided\n");
        return NULL;
    }

    fd = open(filename, O_RDWR);
    if (fd < 0) {
        perror("open");
        return NULL;
    }

    gbm = gbm_create_device(fd);
    if (!gbm) {
        printf("Could not create gbm device\n");
        goto gbm_dev_fail;
    }

    data = calloc(1, sizeof(*data));
    if (!data) {
        goto alloc_fail;
    }

    data->resources = drmModeGetResources(fd);
    if (!data->resources) {
        goto res_fail;
    }

    crtc_id = data->resources->crtcs[0];

    data->connector = modesetting_find_connector(data->resources, fd, &conn_id);
    if (!data->connector) {
        goto conn_fail;
    }

    ret = gbm_bo_create_and_map(gbm, data);
    if (!ret) {
        goto bo_fail;
    }

    /* From here on, destroy_user_data cleans up any errors */
    gbm_bo_set_user_data(ret, data, destroy_user_data);

    data->fb_id = gbm_bo_create_fb(ret);
    if (!data->fb_id) {
        goto create_fb_fail;
    }

    if (drmModeSetCrtc(fd, crtc_id, data->fb_id, 0, 0, &conn_id, 1, &data->connector->modes[0])) {
        goto set_crtc_fail;
    }

    return ret;

bo_fail:
    drmModeFreeConnector(data->connector);
conn_fail:
    drmModeFreeResources(data->resources);
res_fail:
    free(data);
alloc_fail:
    gbm_device_destroy(gbm);
gbm_dev_fail:
    close(fd);
    return NULL;

set_crtc_fail:
create_fb_fail:
    gbm_bo_destroy(ret);
    return NULL;
}

int
modesetting_draw_image(struct gbm_bo *bo, void *image, int x_start, int y_start, int width, int height)
{
    int fb_width = gbm_bo_get_width(bo);
    int fb_height = gbm_bo_get_height(bo);
    int fb_pitch = gbm_bo_get_stride(bo) / sizeof(uint32_t);
    if (fb_width < width + x_start ||
        fb_height < height + y_start) {
        return 0;
    }

    uint32_t *src = image;
    uint32_t *dst = gbm_bo_get_map(bo);

    for(int i = 0; i < height; i++) {
        memcpy(dst + (i + y_start) * fb_pitch + x_start, src + i * width, width * sizeof(uint32_t));
    }
    return 1;
}

int
modesetting_fill_color(struct gbm_bo *bo, uint32_t color, int x_start, int y_start, int width, int height)
{
    int fb_width = gbm_bo_get_width(bo);
    int fb_height = gbm_bo_get_height(bo);
    int fb_pitch = gbm_bo_get_stride(bo) / sizeof(uint32_t);
    if (fb_width < width + x_start ||
        fb_height < height + y_start) {
        return 0;
    }

    uint32_t *mem = malloc(width * sizeof(uint32_t));
    if (!mem) {
        return 0;
    }

    uint32_t *src = mem;
    uint32_t *dst = gbm_bo_get_map(bo);

    for(int i = 0; i < width; i++) {
        src[i] = color;
    }

    for(int i = 0; i < height; i++) {
        memcpy(dst + (i + y_start) * fb_pitch + x_start, src, width * sizeof(uint32_t));
    }

    free(mem);
    return 1;
}
