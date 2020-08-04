#ifndef _RESIZE_FILTER_H_
#define _RESIZE_FILTER_H_

#include "Scale.h"
#include "common.h"

int initFilter(int16_t **outFilter, int32_t **filterPos,
	int *outFilterSize, int xInc, int srcW,
	int dstW, int filterAlign, int one,
	int srcPos, int dstPos);

int initFilter_fast(int16_t **outFilter, int32_t **filterPos,
	int *outFilterSize, int xInc, int srcW,
	int dstW, int filterAlign, int one,
	int srcPos, int dstPos);

int init_filters(fs_scale_handle * c, float *x, float * y);
int init_filterstoint(fs_scale_handle * c, float *x, float * y);
int free_filters(fs_scale_handle *c);


#endif // !_RESIZE_FILTER_H_

