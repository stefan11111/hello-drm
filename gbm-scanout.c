#include <gbm.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

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

int main(int ac)
{
    int fd = open("/dev/dri/card0", O_RDWR);

    printf("fd: %d\n", fd);

    drmSetMaster(fd);

    struct gbm_device *gbm = gbm_create_device(fd);

    printf("gbm: %p\n", gbm);

    uint32_t width = 1920;
    uint32_t height = 1080;

    drmModeRes *resources = drmModeGetResources(fd);

    uint32_t crtc_id = resources->crtcs[0];
    uint32_t conn_id = resources->connectors[1];

    drmModeConnector *connector = modesetting_find_connector(resources, fd, &conn_id);

    struct gbm_bo *primary = gbm_bo_create_with_modifiers2(gbm, width, height,
                                                           ac >= 2 ? GBM_FORMAT_XBGR8888 : GBM_FORMAT_XRGB8888,
                                                           (uint64_t[]){0}, 1,
                                                           GBM_BO_USE_SCANOUT | GBM_BO_USE_FRONT_RENDERING);

    printf("bo: %p, format: 0x%x\n", primary, gbm_bo_get_format(primary));

    void* bo = primary;

    void *map;
    void *unused = NULL;
    uint32_t stride;
    map = gbm_bo_map(bo, 0, 0, width, height, GBM_BO_TRANSFER_READ_WRITE, &stride, &unused);
    printf("map: %p\n", map);

    uint32_t *ptr = map;

    uint32_t fb_id = gbm_bo_create_fb(bo);

    drmModeSetCrtc(fd, crtc_id, fb_id, 0, 0, &conn_id, 1, &connector->modes[0]);

printf("crtc_id :%d, fb_id: %d, conn_id: %d\n", crtc_id, fb_id, conn_id);

    printf("set_crtc\n");

    uint32_t blue = 0xff;
    uint32_t red = 0xff0000;
    uint32_t pitch = /* gbm_bo_get_stride(bo) */ stride / sizeof(uint32_t);

    printf("modifier: 0x%lx\n", (long)gbm_bo_get_modifier(bo));

    signal(SIGINT, sigint_handler);

    drmModeClip full_clip = (drmModeClip){.x1 = 0, .y1 = 0, .x2 = width, .y2 = height,};

    while(!done) {
unused = NULL;
ptr = gbm_bo_map(bo, 0, 0, width, height, GBM_BO_TRANSFER_READ_WRITE, &stride, &unused);
        draw_frame(ptr, width, height, pitch);
        drmModeDirtyFB(fd, fb_id, &full_clip, 1);
gbm_bo_unmap(primary, unused);
        sleep(1);
        printf("flip\n");
    }
    return 0;
}
