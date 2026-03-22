#ifndef __LETTERS_H__
#define __LETTERS_H__

#include <stddef.h>

typedef struct letter {
    uint32_t *buf;
    int width;
    int height;
} letter_t;

#define black ((uint32_t)0)
#define white ((uint32_t)0x00ffffff)

uint32_t buf_h[5][5] = {
                        {black, black, black, black, black},
                        {black, white, black, white, black},
                        {black, white, white, white, black},
                        {black, white, black, white, black},
                        {black, black, black, black, black},
                       };

letter_t scalable_h = {
                       .buf = (uint32_t*)buf_h,
                       .width = 5,
                       .height = 5,
                      };

uint32_t buf_e[7][5] = {
                        {black, black, black, black, black},
                        {black, white, white, white, black},
                        {black, white, black, black, black},
                        {black, white, white, white, black},
                        {black, white, black, black, black},
                        {black, white, white, white, black},
                        {black, black, black, black, black},
                       };

letter_t scalable_e = {
                       .buf = (uint32_t*)buf_e,
                       .width = 5,
                       .height = 7,
                      };

uint32_t buf_l[5][5] = {
                        {black, black, black, black, black},
                        {black, black, white, black, black},
                        {black, black, white, black, black},
                        {black, black, white, white, black},
                        {black, black, black, black, black},
                       };

letter_t scalable_l = {
                       .buf = (uint32_t*)buf_l,
                       .width = 5,
                       .height = 5,
                      };

uint32_t buf_o[5][5] = {
                        {black, black, black, black, black},
                        {black, white, white, white, black},
                        {black, white, black, white, black},
                        {black, white, white, white, black},
                        {black, black, black, black, black},
                       };

letter_t scalable_o = {
                       .buf = (uint32_t*)buf_o,
                       .width = 5,
                       .height = 5,
                      };

uint32_t buf_space[1][1] = {{black}};

letter_t scalable_space = {
                       .buf = (uint32_t*)buf_space,
                       .width = 1,
                       .height = 1,
                      };

uint32_t buf_w[5][7] = {
                        {black, black, black, black, black, black, black},
                        {black, white, black, white, black, white, black},
                        {black, white, black, white, black, white, black},
                        {black, black, white, black, white, black, black},
                        {black, black, black, black, black, black, black},
                       };

letter_t scalable_w = {
                       .buf = (uint32_t*)buf_w,
                       .width = 7,
                       .height = 5,
                      };


uint32_t buf_r[5][5] = {
                        {black, black, black, black, black},
                        {black, white, white, black, black},
                        {black, white, black, black, black},
                        {black, white, black, black, black},
                        {black, black, black, black, black},
                       };

letter_t scalable_r = {
                       .buf = (uint32_t*)buf_r,
                       .width = 5,
                       .height = 5,
                      };

uint32_t buf_d[8][5] = {
                        {black, black, black, black, black},
                        {black, black, black, white, black},
                        {black, black, black, white, black},
                        {black, black, white, white, black},
                        {black, white, black, white, black},
                        {black, white, black, white, black},
                        {black, white, white, white, black},
                        {black, black, black, black, black},
                       };

letter_t scalable_d = {
                       .buf = (uint32_t*)buf_d,
                       .width = 5,
                       .height = 8,
                      };

uint32_t buf_ex[7][5] = {
                         {black, black, black, black, black},
                         {black, black, white, black, black},
                         {black, black, white, black, black},
                         {black, black, white, black, black},
                         {black, black, black, black, black},
                         {black, black, white, black, black},
                         {black, black, black, black, black},
                        };

letter_t scalable_ex = {
                        .buf = (uint32_t*)buf_ex,
                        .width = 5,
                        .height = 7,
                       };


#endif /* __LETTERS_H__ */
