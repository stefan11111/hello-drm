#ifndef __MODESETTING_H__
#define __MODESETTING_H__

#include <stdint.h>
#include <gbm.h>

void
modesetting_close(struct gbm_bo *dev);

struct gbm_bo*
modesetting_open(const char* restrict name);

int
modesetting_draw_image(struct gbm_bo *dev, void* image, int x_start, int y_start, int width, int height);

int
modesetting_fill_color(struct gbm_bo *dev, uint32_t color, int x_start, int y_start, int width, int height);

#endif /* __modesetting_H__ */
