#include "scale_h.h"
#include <stdlib.h>


typedef struct FilterContext
{
	uint16_t *filter;
	int *filter_pos;
	int filter_size;
	int xInc;
} FilterContext;

static void hScale8To15_c(fs_scale_handle *c, int16_t *dst, int dstW,
	const uint8_t *src, const int16_t *filter,
	const int32_t *filterPos, int filterSize)
{
	int i;
	for (i = 0; i < dstW; i++) 
	{
		int j;
		int srcPos = filterPos[i];
		int val = 0;

		for (j = 0; j < filterSize; j++) 
			val += ((int)src[srcPos + j]) * filter[filterSize * i + j];

		dst[i] = FFMIN(val >> 7, (1 << 15) - 1); // the cubic equation does overflow ...
	}
}

static void ff_hyscale_fast_c(fs_scale_handle *c, int16_t *dst, int dstWidth,
	const uint8_t *src, int srcW, int xInc)
{
	int i;
	unsigned int xpos = ((unsigned int)((0.5 * srcW / dstWidth - 0.5) * 16)) << 12;
	for (i = 0; i < dstWidth; i++) 
	{
		register unsigned int xx = xpos >> 16;
		register unsigned int xalpha = (xpos & 0xFFFF) >> 9;
		dst[i] = (src[xx] << 7) + (src[xx + 1] - src[xx]) * xalpha;
		xpos += xInc;
	}
	for (i = dstWidth - 1; (i*xInc) >> 16 >= srcW - 1; i--)
		dst[i] = src[srcW - 1] * 128;
}

static int lum_h_scale(fs_scale_handle *c, SwsFilterDescriptor *desc, int sliceY, int sliceH)
{
	FilterContext *instance = (FilterContext *)desc->instance;
	int srcW = desc->src->width;
	int dstW = desc->dst->width;
	int xInc = instance->xInc;

	int i;
	for (i = 0; i < sliceH; ++i) 
	{
		uint8_t ** src = desc->src->plane[0].line;
		uint8_t ** dst = desc->dst->plane[0].line;
		int src_pos = sliceY + i - desc->src->plane[0].sliceY;
		int dst_pos = sliceY + i - desc->dst->plane[0].sliceY;

		if (c->flag & 0x1)
		{
			ff_hyscale_fast_c(c, (int16_t*)dst[dst_pos], dstW, (const uint8_t *)src[src_pos], srcW, xInc);
		}
		else
		{
			hScale8To15_c(c, (int16_t*)dst[dst_pos], dstW, (const uint8_t *)src[src_pos], (int16_t *)instance->filter,
				instance->filter_pos, instance->filter_size);
		}
		desc->dst->plane[0].sliceH += 1;
	}
	return sliceH;
}

int init_desc_hscale(SwsFilterDescriptor *desc, SwsSlice *src, SwsSlice *dst, uint16_t *filter, int * filter_pos, int filter_size, int xInc)
{
	FilterContext *li = (FilterContext*)malloc(sizeof(FilterContext));
	if (!li) return -1;

	li->filter = filter;
	li->filter_pos = filter_pos;
	li->filter_size = filter_size;
	li->xInc = xInc;
	desc->instance = li;
	desc->alpha = 0;
	desc->src = src;
	desc->dst = dst;
	desc->process = &lum_h_scale;

	return 0;
}

static void ff_hcscale_fast_c(fs_scale_handle *c, int16_t *dst1, int16_t *dst2,
	int dstWidth, const uint8_t *src1,
	const uint8_t *src2, int srcW, int xInc)
{
	int i;
	unsigned int xpos = ((unsigned int)((0.5 * srcW / dstWidth - 0.5) * 16)) << 11;
	for (i = 0; i < dstWidth; i++) 
	{
		register unsigned int xx = xpos >> 16;
		register unsigned int xalpha = (xpos & 0xFFFF) >> 9;

		dst1[i] = (src1[xx] * 128);// (xalpha ^ 127) + src1[xx + 1] * xalpha);
		dst2[i] = (src2[xx] * 128);// (xalpha ^ 127) + src2[xx + 1] * xalpha);
		xpos += xInc;
	}
	for (i = dstWidth - 1; (i*xInc) >> 16 >= srcW - 1; i--) 
	{
		dst1[i] = src1[srcW - 1] * 128;
		dst2[i] = src2[srcW - 1] * 128;
	}
}

static int chr_h_scale(fs_scale_handle *c, SwsFilterDescriptor *desc, int sliceY, int sliceH)
{
	FilterContext *instance = (FilterContext *)desc->instance;
	int srcW = AV_CEIL_RSHIFT(desc->src->width, desc->src->h_chr_sub_sample);
	int dstW = AV_CEIL_RSHIFT(desc->dst->width, desc->dst->h_chr_sub_sample);
	int xInc = instance->xInc;
	uint8_t ** src1 = desc->src->plane[1].line;
	uint8_t ** dst1 = desc->dst->plane[1].line;
	uint8_t ** src2 = desc->src->plane[2].line;
	uint8_t ** dst2 = desc->dst->plane[2].line;
	int src_pos1 = sliceY - desc->src->plane[1].sliceY;
	int dst_pos1 = sliceY - desc->dst->plane[1].sliceY;
	int src_pos2 = sliceY - desc->src->plane[2].sliceY;
	int dst_pos2 = sliceY - desc->dst->plane[2].sliceY;
	int i;

	for (i = 0; i < sliceH; ++i)
	{
		if (c->flag & 0x1)
		{
			ff_hcscale_fast_c(c, (uint16_t*)dst1[dst_pos1 + i], (uint16_t*)dst2[dst_pos2 + i], dstW, src1[src_pos1 + i], src2[src_pos2 + i], srcW, xInc);
		}
		else
		{
			hScale8To15_c(c, (int16_t*)dst1[dst_pos1 + i], dstW, src1[src_pos1 + i], (int16_t*)instance->filter, instance->filter_pos, instance->filter_size);
			hScale8To15_c(c, (int16_t*)dst2[dst_pos2 + i], dstW, src2[src_pos2 + i], (int16_t*)instance->filter, instance->filter_pos, instance->filter_size);
		}
		desc->dst->plane[1].sliceH += 1;
		desc->dst->plane[2].sliceH += 1;
	}
	return sliceH;
}


int init_desc_chscale(SwsFilterDescriptor *desc, SwsSlice *src, SwsSlice *dst, uint16_t *filter, int * filter_pos, int filter_size, int xInc)
{
	FilterContext *li = (FilterContext *)malloc(sizeof(FilterContext));
	if (!li) return -1;

	li->filter = filter;
	li->filter_pos = filter_pos;
	li->filter_size = filter_size;
	li->xInc = xInc;

	desc->instance = li;
	desc->alpha = 0;
	desc->src = src;
	desc->dst = dst;
	desc->process = &chr_h_scale;

	return 0;
}


