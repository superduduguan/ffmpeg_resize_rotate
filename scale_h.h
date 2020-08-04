#ifndef _RESIZE_SCALE_H_H_
#define _RESIZE_SCALE_H_H_

#include "Scale.h"
#include "common.h"

int init_desc_hscale(SwsFilterDescriptor *desc, SwsSlice *src, SwsSlice *dst, uint16_t *filter, int * filter_pos, int filter_size, int xInc);

int init_desc_chscale(SwsFilterDescriptor *desc, SwsSlice *src, SwsSlice *dst, uint16_t *filter, int * filter_pos, int filter_size, int xInc);

#endif // !_RESIZE_SCALE_H_H_

