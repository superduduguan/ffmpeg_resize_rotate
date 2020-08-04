#ifndef _RESIZE_SLICE_H_
#define _RESIZE_SLICE_H_

#include "Scale.h"
#include "common.h"

void free_lines(SwsSlice *s);

int alloc_lines(SwsSlice *s, int size, int width);

void free_slice(SwsSlice *s);

int alloc_slice(SwsSlice *s, int lumLines, int chrLines, int h_sub_sample, int v_sub_sample, int ring);

int init_slice_from_src(SwsSlice * s, uint8_t *src[4], int stride[4], int srcW, int lumY, int lumH, int chrY, int chrH, int relative);
int init_slice_from_dst(SwsSlice * s, float *src[4], int stride[4], int srcW, int lumY, int lumH, int chrY, int chrH, int relative);
int rotate_slice(SwsSlice *s, int lum, int chr);
int alloc_slicex(SwsSlice *s, int lumLines, int chrLines, int h_sub_sample, int v_sub_sample, int ring);
#endif // !_RESIZE_SLICE_H_

