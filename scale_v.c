#include "scale_v.h"
#include "function.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct VScalerContext
{
	uint16_t *filter[2];
	int32_t  *filter_pos;
	int filter_size;
	void *pfn;
} VScalerContext;

static int lum_planar_vscale(fs_scale_handle *c, SwsFilterDescriptor *desc, int sliceY, int sliceH)
{
	VScalerContext *inst = (VScalerContext *)desc->instance;
	int dstW = desc->dst->width;

	int first = FFMAX(1 - inst->filter_size, inst->filter_pos[sliceY]);
	int sp = first - desc->src->plane[0].sliceY;
	int dp = sliceY - desc->dst->plane[0].sliceY;
	uint8_t **src = desc->src->plane[0].line + sp;
	uint8_t **dst = desc->dst->plane[0].line + dp;
	uint16_t *filter = inst->filter[0] + sliceY * inst->filter_size;


	if (inst->filter_size == 1)
		((yuv2planar1_fn)inst->pfn)((const int16_t*)src[0], dst[0], dstW, c->lumDither8, 0);
	else
		((yuv2planarX_fn)inst->pfn)((int16_t *)filter, inst->filter_size, (const int16_t**)src, dst[0], dstW, c->lumDither8, 0);

	if (desc->alpha) {
		int sp = first - desc->src->plane[3].sliceY;
		int dp = sliceY - desc->dst->plane[3].sliceY;
		uint8_t **src = desc->src->plane[3].line + sp;
		uint8_t **dst = desc->dst->plane[3].line + dp;
		uint16_t *filter = inst->filter[1] + sliceY * inst->filter_size;

		if (inst->filter_size == 1)
			((yuv2planar1_fn)inst->pfn)((const int16_t*)src[0], dst[0], dstW, c->lumDither8/*{64...}*/, 0);
		else
			((yuv2planarX_fn)inst->pfn)((int16_t *)filter, inst->filter_size, (const int16_t**)src, dst[0], dstW, c->lumDither8, 0);
	}

	
		 //uint8_t dst1 = *(*(desc->src->plane[0].line + 8) + 174);
		


		 //printf("---------------------%hu-----------------------\n", dst1);
	return 1;
}


static int lum_planar_vscaletoint(fs_scale_handle *c, SwsFilterDescriptor *desc, int sliceY, int sliceH)
{
	VScalerContext *inst = (VScalerContext *)desc->instance;
	int dstW = desc->dst->width;

	int first = FFMAX(1 - inst->filter_size, inst->filter_pos[sliceY]);
	int sp = first - desc->src->plane[0].sliceY;
	int dp = sliceY - desc->dst->plane[0].sliceY;
	uint8_t **src = desc->src->plane[0].line + sp;
	uint8_t **dst = desc->dst->plane[0].line + dp;
	uint16_t *filter = inst->filter[0] + sliceY * inst->filter_size;


	if (inst->filter_size == 1)
		((yuv2planar1_fn)inst->pfn)((const int16_t*)src[0], dst[0], dstW, c->lumDither8, 0);
	else
		((yuv2planarX_fn)inst->pfn)((int16_t *)filter, inst->filter_size, (const int16_t**)src, dst[0], dstW, c->lumDither8, 0);

	if (desc->alpha) {
		int sp = first - desc->src->plane[3].sliceY;
		int dp = sliceY - desc->dst->plane[3].sliceY;
		uint8_t **src = desc->src->plane[3].line + sp;
		uint8_t **dst = desc->dst->plane[3].line + dp;
		uint16_t *filter = inst->filter[1] + sliceY * inst->filter_size;

		if (inst->filter_size == 1)
			((yuv2planar1_fn)inst->pfn)((const int16_t*)src[0], dst[0], dstW, c->lumDither8/*{64...}*/, 0);
		else
			((yuv2planarX_fn)inst->pfn)((int16_t *)filter, inst->filter_size, (const int16_t**)src, dst[0], dstW, c->lumDither8, 0);
	}

	return 1;
}




static int chr_planar_vscale(fs_scale_handle *c, SwsFilterDescriptor *desc, int sliceY, int sliceH)
{
	const int chrSkipMask = (1 << desc->dst->v_chr_sub_sample) - 1;
	//printf("asdadadsad\n");
	if (sliceY & chrSkipMask){
		//printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
		return 0;
	}
		
	else {
		VScalerContext *inst = (VScalerContext *)desc->instance;
		int dstW = AV_CEIL_RSHIFT(desc->dst->width, desc->dst->h_chr_sub_sample);
		int chrSliceY = sliceY >> desc->dst->v_chr_sub_sample;

		int first = FFMAX(1 - inst->filter_size, inst->filter_pos[chrSliceY]);
		int sp1 = first - desc->src->plane[1].sliceY;
		int sp2 = first - desc->src->plane[2].sliceY;
		int dp1 = chrSliceY - desc->dst->plane[1].sliceY;
		int dp2 = chrSliceY - desc->dst->plane[2].sliceY;
		uint8_t **src1 = desc->src->plane[1].line + sp1;
		uint8_t **src2 = desc->src->plane[2].line + sp2;
		uint8_t **dst1 = desc->dst->plane[1].line + dp1;
		uint8_t **dst2 = desc->dst->plane[2].line + dp2;
		uint16_t *filter = inst->filter[0] + chrSliceY * inst->filter_size;
		
		((yuv2interleavedX_fn)inst->pfn)(c, (const int16_t*)filter, inst->filter_size, (const int16_t**)src1, (const int16_t**)src2, dst1[0], dstW);
	}

	// uint8_t dst1 = *(*(desc->dst->plane[1].line + dp1)+1);
	// printf("%hu", dst1);

	return 1;
}




static int chr_planar_vscaletoint(fs_scale_handle *c, SwsFilterDescriptor *desc, int sliceY, int sliceH)
{
	const int chrSkipMask = (1 << desc->dst->v_chr_sub_sample) - 1;
	//printf("asdadadsad\n");
	if (sliceY & chrSkipMask)
		return 0;
	else {
		//printf("asdadadsad\n");
		VScalerContext *inst = (VScalerContext *)desc->instance;
		int dstW = AV_CEIL_RSHIFT(desc->dst->width, desc->dst->h_chr_sub_sample);
		int chrSliceY = sliceY >> desc->dst->v_chr_sub_sample;

		int first = FFMAX(1 - inst->filter_size, inst->filter_pos[chrSliceY]);
		int sp1 = first - desc->src->plane[1].sliceY;
		int sp2 = first - desc->src->plane[2].sliceY;
		int dp1 = chrSliceY - desc->dst->plane[1].sliceY;
		int dp2 = chrSliceY - desc->dst->plane[2].sliceY;
		printf("%d, %d\n", dp1, dp2);
		uint8_t **src1 = desc->src->plane[1].line + sp1;
		uint8_t **src2 = desc->src->plane[2].line + sp2;
		uint8_t **dst1 = desc->dst->plane[1].line + dp1;
		uint8_t **dst2 = desc->dst->plane[2].line + dp2;
		uint16_t *filter = inst->filter[0] + chrSliceY * inst->filter_size;

		((yuv2interleavedX_fn)inst->pfn)(c, (const int16_t*)filter, inst->filter_size, (const int16_t**)src1, (const int16_t**)src2, dst1[0], dstW);
	}

	return 1;
}




void init_vscale_pfn(fs_scale_handle *c,
	yuv2planarX_fn yuv2planeX,
	yuv2interleavedX_fn yuv2nv12cX)
{
	VScalerContext *lumCtx = NULL;
	VScalerContext *chrCtx = NULL;
	int idx = c->numDesc - 2; //FIXME avoid hardcoding indexes

	chrCtx = (VScalerContext *)c->desc[idx].instance;
	chrCtx->filter[0] = (uint16_t *)c->vChrFilter;
	chrCtx->filter_size = c->vChrFilterSize;
	chrCtx->filter_pos = c->vChrFilterPos;

	--idx;
	chrCtx->pfn = (void *)yuv2nv12cX;


	lumCtx = (VScalerContext *)c->desc[idx].instance;
	lumCtx->filter[0] = (uint16_t *)c->vLumFilter;
	lumCtx->filter[1] = (uint16_t *)c->vLumFilter;
	lumCtx->filter_size = c->vLumFilterSize;
	lumCtx->filter_pos = c->vLumFilterPos;
	lumCtx->pfn = (void *)yuv2planeX;
}



void init_vscale_pfntoint(fs_scale_handle *c,
	yuv2planarX_fn yuv2planeX,
	yuv2interleavedX_fn yuv2nv12cX)
{
	VScalerContext *lumCtx = NULL;
	VScalerContext *chrCtx = NULL;
	int idx = c->numDesc - 1; //FIXME avoid hardcoding indexes

	chrCtx = (VScalerContext *)c->desc[idx].instance;
	chrCtx->filter[0] = (uint16_t *)c->vChrFilter;
	chrCtx->filter_size = c->vChrFilterSize;
	chrCtx->filter_pos = c->vChrFilterPos;

	--idx;
	chrCtx->pfn = (void *)yuv2nv12cX;


	lumCtx = (VScalerContext *)c->desc[idx].instance;
	lumCtx->filter[0] = (uint16_t *)c->vLumFilter;
	lumCtx->filter[1] = (uint16_t *)c->vLumFilter;
	lumCtx->filter_size = c->vLumFilterSize;
	lumCtx->filter_pos = c->vLumFilterPos;
	lumCtx->pfn = (void *)yuv2planeX;
}


int init_vscale(fs_scale_handle *c, SwsFilterDescriptor *desc, SwsSlice *src, SwsSlice *dst)
{
	VScalerContext *lumCtx = NULL;
	VScalerContext *chrCtx = NULL;

	lumCtx = (VScalerContext *)malloc(sizeof(VScalerContext));
	if (!lumCtx) { return -1; }

	desc[0].process = lum_planar_vscale;
	desc[0].instance = lumCtx;
	desc[0].src = src;
	desc[0].dst = dst;
	desc[0].alpha = 0;


	chrCtx = (VScalerContext *)malloc(sizeof(VScalerContext));
	if (!chrCtx) { return -1; }
	desc[1].process = chr_planar_vscale;
	desc[1].instance = chrCtx;
	desc[1].src = src;
	desc[1].dst = dst;

	// TODO : cancel this function
	init_vscale_pfn(c, yuv2planeX_8_c, yuv2nv12cX_c);

	
	return 0;
}



int init_vscaletoint(fs_scale_handle *c, SwsFilterDescriptor *desc, SwsSlice *src, SwsSlice *dst)
{
	VScalerContext *lumCtx = NULL;
	VScalerContext *chrCtx = NULL;

	lumCtx = (VScalerContext *)malloc(sizeof(VScalerContext));
	if (!lumCtx) { return -1; }

	desc[0].process = lum_planar_vscaletoint;
	desc[0].instance = lumCtx;
	desc[0].src = src;
	desc[0].dst = dst;
	desc[0].alpha = 0;


	chrCtx = (VScalerContext *)malloc(sizeof(VScalerContext));
	if (!chrCtx) { return -1; }
	desc[1].process = chr_planar_vscale;
	desc[1].instance = chrCtx;
	desc[1].src = src;
	desc[1].dst = dst;

	// TODO : cancel this function
	init_vscale_pfntoint(c, yuv2planeX_8_c, yuv2nv12cX_c);

	
	return 0;
}

