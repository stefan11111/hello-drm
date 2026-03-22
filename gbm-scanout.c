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

volatile int done = 0;

static void sigint_handler(int arg)
{
	done = 1;
}

int main()
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

    drmModeConnector *connector = drmModeGetConnector(fd, conn_id);

    struct gbm_bo *primary = gbm_bo_create(gbm, width, height, GBM_FORMAT_XRGB8888,
                                           GBM_BO_USE_SCANOUT | GBM_BO_USE_WRITE);

    printf("bo: %p\n", primary);

    void* bo = primary;

    void *map;
    void *unused = NULL;
    uint32_t stride;
    map = gbm_bo_map(bo, 0, 0, width, height, GBM_BO_TRANSFER_READ_WRITE, &stride, &unused);
    printf("map: %p\n", map);

    uint32_t *ptr = map;

    uint32_t fb_id = gbm_bo_create_fb(bo);

    drmModeSetCrtc(fd, crtc_id, fb_id, 0, 0, &conn_id, 1, &connector->modes[0]);

    uint32_t blue = 0xff;
    uint32_t red = 0xff0000;
    uint32_t pitch = gbm_bo_get_stride(bo) / sizeof(uint32_t);

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

    signal(SIGINT, sigint_handler);

    while (!done) {
        sleep(1);
    }
    return 0;
}
