#ifndef _RESIZE_CONVERT_H_
#define _RESIZE_CONVERT_H_

#include "Scale.h"
#include "common.h"

int init_desc_fmt_convert(SwsFilterDescriptor *desc, SwsSlice * src, SwsSlice *dst, uint32_t *pal);

int init_desc_cfmt_convert(SwsFilterDescriptor *desc, SwsSlice * src, SwsSlice *dst, uint32_t *pal);

int ConV(fs_scale_handle *c, SwsFilterDescriptor *desc, SwsSlice * src, SwsSlice *dst, float *x, float *y);
#endif // !_RESIZE_CONVERT_H_

