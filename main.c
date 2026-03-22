#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

#include <gbm.h>

#include "modesetting.h"
#include "letters.h"

void*
scale_image(void* image, int src_x, int src_y, int dst_x, int dst_y)
{
    uint32_t *dst = malloc(dst_x * dst_y * sizeof(uint32_t));
    uint32_t *src = image;
    if (!dst) {
        return NULL;
    }

#define DST(i,j) dst[i * dst_x + j]
#define SRC(i,j) src[i * src_x + j]

    float x_ratio = src_x / (float)dst_x;
    float y_ratio = src_y / (float)dst_y;
    for(int i = 0; i < dst_y; i++) {
        for(int j = 0; j < dst_x; j++) {
            DST(i,j) = SRC((int)(i * y_ratio), (int)(j * x_ratio));
        }
    }

#undef DST
#undef SRC

    return dst;
}

volatile int done = 0;

static void sigint_handler(int arg)
{
        done = 1;
}

int main()
{
    struct gbm_bo *bo = modesetting_open("/dev/dri/card0");
    for(int i = 0; i < 31 && !bo; i++) {
        char device[] = "/dev/cardxx";
        snprintf(device, sizeof(device), "/dev/card%d", i);
        bo = modesetting_open(device);
    }

    if (!bo) {
        return 0;
    }

    struct timespec duration = {.tv_sec = 0,
                                .tv_nsec = 8000000, /* 1/125 sec */
                               };

#define SCALE_IMAGE(x) \
    void *image_##x = scale_image(scalable_##x.buf, scalable_##x.width, scalable_##x.height, 100, 300); \
    if (!image_##x) { \
        printf("Could not scale image\n"); \
        return 0; \
    }

    SCALE_IMAGE(h);
    SCALE_IMAGE(e);
    SCALE_IMAGE(l);
 /* SCALE_IMAGE(l); */
    SCALE_IMAGE(o);

    SCALE_IMAGE(space);

    SCALE_IMAGE(w);
 /* SCALE_IMAGE(o); */
    SCALE_IMAGE(r);
 /* SCALE_IMAGE(l); */
    SCALE_IMAGE(d);
    SCALE_IMAGE(ex);

#define DRAW_IMAGE(x) \
        if (!modesetting_draw_image(bo, image_##x, 100 * idx, 100, 100, 300)) { \
            printf("Could not paint framebuffer\n"); \
            break; \
        } \
        idx++;



    signal(SIGINT, sigint_handler);

    uint32_t blue = 0xff;
    uint32_t red = 0xff0000;

    uint32_t width = gbm_bo_get_width(bo);
    uint32_t height = gbm_bo_get_height(bo);

    modesetting_fill_color(bo, blue, 0, 0, width, height / 2);
    modesetting_fill_color(bo, red, 0, height / 2, width, height / 2);

    do {
        int idx = 0;

        DRAW_IMAGE(h);
        DRAW_IMAGE(e);
        DRAW_IMAGE(l);
        DRAW_IMAGE(l);
        DRAW_IMAGE(o);

        DRAW_IMAGE(space);

        DRAW_IMAGE(w);
        DRAW_IMAGE(o);
        DRAW_IMAGE(r);
        DRAW_IMAGE(l);
        DRAW_IMAGE(d);
        DRAW_IMAGE(ex);
    } while(0);

    while (!done) {
        sleep(1);
    }

#undef SCALE_IMAGE
#undef DRAW_IMAGE

    gbm_bo_destroy(bo);
    return 0;
}
