#ifndef _RESIZE_SCALE_V_H_
#define _RESIZE_SCALE_V_H_

#include "Scale.h"
#include "common.h"

int init_vscale(fs_scale_handle *c, SwsFilterDescriptor *desc, SwsSlice *src, SwsSlice *dst);
int init_vscaletoint(fs_scale_handle *c, SwsFilterDescriptor *desc, SwsSlice *src, SwsSlice *dst);

#endif // !_RESIZE_SCALE_V_H_

