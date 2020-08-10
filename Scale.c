#include "Scale.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "function.h"
#include "filter.h"
#include "slice.h"



DECLARE_ALIGNED(8, static const uint8_t, sws_pb_64)[8] = {
	64, 64, 64, 64, 64, 64, 64, 64
};

typedef struct AVComponentDescriptor {
	/**
	* Which of the 4 planes contains the component.
	*/
	int plane;

	/**
	* Number of elements between 2 horizontally consecutive pixels.
	* Elements are bits for bitstream formats, bytes otherwise.
	*/
	int step;

	/**
	* Number of elements before the component of the first pixel.
	* Elements are bits for bitstream formats, bytes otherwise.
	*/
	int offset;

	/**
	* Number of least significant bits that must be shifted away
	* to get the value.
	*/
	int shift;

	/**
	* Number of bits in the component.
	*/
	int depth;

#if FF_API_PLUS1_MINUS1
	/** deprecated, use step instead */
	attribute_deprecated int step_minus1;

	/** deprecated, use depth instead */
	attribute_deprecated int depth_minus1;

	/** deprecated, use offset instead */
	attribute_deprecated int offset_plus1;
#endif
} AVComponentDescriptor;

/**
* Descriptor that unambiguously describes how the bits of a pixel are
* stored in the up to 4 data planes of an image. It also stores the
* subsampling factors and number of components.
*
* @note This is separate of the colorspace (RGB, YCbCr, YPbPr, JPEG-style YUV
*       and all the YUV variants) AVPixFmtDescriptor just stores how values
*       are stored not what these values represent.
*/
typedef struct AVPixFmtDescriptor {
	const char *name;
	uint8_t nb_components;  ///< The number of components each pixel has, (1-4)

							/**
							* Amount to shift the luma width right to find the chroma width.
							* For YV12 this is 1 for example.
							* chroma_width = AV_CEIL_RSHIFT(luma_width, log2_chroma_w)
							* The note above is needed to ensure rounding up.
							* This value only refers to the chroma components.
							*/
	uint8_t log2_chroma_w;

	/**
	* Amount to shift the luma height right to find the chroma height.
	* For YV12 this is 1 for example.
	* chroma_height= AV_CEIL_RSHIFT(luma_height, log2_chroma_h)
	* The note above is needed to ensure rounding up.
	* This value only refers to the chroma components.
	*/
	uint8_t log2_chroma_h;

	/**
	* Combination of AV_PIX_FMT_FLAG_... flags.
	*/
	//uint64_t flags;

	/**
	* Parameters that describe how pixels are packed.
	* If the format has 1 or 2 components, then luma is 0.
	* If the format has 3 or 4 components:
	*   if the RGB flag is set then 0 is red, 1 is green and 2 is blue;
	*   otherwise 0 is luma, 1 is chroma-U and 2 is chroma-V.
	*
	* If present, the Alpha channel is always the last component.
	*/
	AVComponentDescriptor comp[4];

	/**
	* Alternative comma-separated names.
	*/
	const char *alias;
} AVPixFmtDescriptor;

static AVPixFmtDescriptor * av_pix_fmt_desc_get(FS_IMG_FMT_S format)
{
	AVPixFmtDescriptor * desc = (AVPixFmtDescriptor *)malloc(sizeof(AVPixFmtDescriptor));

	desc->name = "nv21";
	desc->nb_components = 3;
	desc->log2_chroma_h = 1;
	desc->log2_chroma_w = 1;
	//Y
	desc->comp[0].plane = 0;
	desc->comp[0].step = 1;
	desc->comp[0].offset = 0;
	desc->comp[0].shift = 0;
	desc->comp[0].depth = 8;
	//U
	desc->comp[1].plane = 1;
	desc->comp[1].step = 2;
	desc->comp[1].offset = 1;
	desc->comp[1].shift = 0;
	desc->comp[1].depth = 8;
	//V
	desc->comp[2].plane = 1;
	desc->comp[2].step = 2;
	desc->comp[2].offset = 0;
	desc->comp[2].shift = 0;
	desc->comp[2].depth = 8;

	return desc;
}


static int init_handle(fs_scale_handle * p_handle ,float *x, float *y)
{
	int i;
	int usesVFilter, usesHFilter;

	int srcW = p_handle->srcW;
	int srcH = p_handle->srcH;
	int dstW = p_handle->dstW;
	int dstH = p_handle->dstH;

	FS_IMG_FMT_S srcFormat = (FS_IMG_FMT_S)p_handle->srcFormat;/*AV_PIX_FMT_NV21*/
	FS_IMG_FMT_S dstFormat = (FS_IMG_FMT_S)p_handle->dstFormat;/*AV_PIX_FMT_NV21*/
	const AVPixFmtDescriptor *desc_src;
	const AVPixFmtDescriptor *desc_dst;
	int ret = 0;

	static const float float_mult = 1.0f / 255.0f;

	desc_src = av_pix_fmt_desc_get((FS_IMG_FMT_S)srcFormat);
	desc_dst = av_pix_fmt_desc_get((FS_IMG_FMT_S)dstFormat);

	if (srcW < 1 || srcH < 1 || dstW < 1 || dstH < 1) { return -1; }

	p_handle->lumXInc = (((fs_int64)srcW << 16) + (dstW >> 1)) / dstW;
	p_handle->lumYInc = (((fs_int64)srcH << 16) + (dstH >> 1)) / dstH;
	p_handle->dstFormatBpp = 16; //av_get_bits_per_pixel(desc_dst);		// 16
	p_handle->srcFormatBpp = 16; // av_get_bits_per_pixel(desc_src);	// 16
	p_handle->chrSrcHSubSample = 1;
	p_handle->chrSrcVSubSample = 1;
	p_handle->chrDstHSubSample = 1;
	p_handle->chrDstVSubSample = 1;
	p_handle->chrSrcW = srcW / 2;
	p_handle->chrSrcH = srcH / 2;
	p_handle->chrDstW = dstW / 2;
	p_handle->chrDstH = dstH / 2;
	p_handle->srcBpc = desc_src->comp[0].depth;
	p_handle->dstBpc = desc_dst->comp[0].depth;
	p_handle->chrXInc = (((fs_int64)p_handle->chrSrcW << 16) + (p_handle->chrDstW >> 1)) / p_handle->chrDstW;
	p_handle->chrYInc = (((fs_int64)p_handle->chrSrcH << 16) + (p_handle->chrDstH >> 1)) / p_handle->chrDstH;

	/* precalculate horizontal scaler filter coefficients */
	{
		const int filterAlign = 1;	//8
		if (p_handle->flag & 0x1) // fast mode
		{
			if ((ret = initFilter_fast(&p_handle->hLumFilter, &p_handle->hLumFilterPos,
				&p_handle->hLumFilterSize, p_handle->lumXInc,
				srcW, dstW, filterAlign, 1 << 14, 128, 128)) < 0)
				goto fail;
			if ((ret = initFilter_fast(&p_handle->hChrFilter, &p_handle->hChrFilterPos,
				&p_handle->hChrFilterSize, p_handle->chrXInc,
				p_handle->chrSrcW, p_handle->chrDstW, filterAlign, 1 << 14, 64, 64)) < 0)
				goto fail;
		}
		else
		{
			if ((ret = initFilter(&p_handle->hLumFilter, &p_handle->hLumFilterPos,
				&p_handle->hLumFilterSize, p_handle->lumXInc,
				srcW, dstW, filterAlign, 1 << 14, 128, 128)) < 0)
				goto fail;
			if ((ret = initFilter(&p_handle->hChrFilter, &p_handle->hChrFilterPos,
				&p_handle->hChrFilterSize, p_handle->chrXInc,
				p_handle->chrSrcW, p_handle->chrDstW, filterAlign, 1 << 14, 64, 64)) < 0)
				goto fail;
		}
	} // initialize horizontal stuff

	  /* precalculate vertical scaler filter coefficients */
	{
		const int filterAlign = 1;
		if (p_handle->flag & 0x1) // fast mode
		{
			if ((ret = initFilter_fast(&p_handle->vLumFilter, &p_handle->vLumFilterPos, &p_handle->vLumFilterSize,
				p_handle->lumYInc, srcH, dstH, filterAlign, (1 << 12), 128, 128)) < 0)
				goto fail;
			if ((ret = initFilter_fast(&p_handle->vChrFilter, &p_handle->vChrFilterPos, &p_handle->vChrFilterSize,
				p_handle->chrYInc, p_handle->chrSrcH, p_handle->chrDstH, filterAlign, (1 << 12), 64, 64)) < 0)
				goto fail;
			//getchar();
		}
		else
		{
			if ((ret = initFilter(&p_handle->vLumFilter, &p_handle->vLumFilterPos, &p_handle->vLumFilterSize,
				p_handle->lumYInc, srcH, dstH, filterAlign, (1 << 12), 128, 128)) < 0)
				goto fail;
			if ((ret = initFilter(&p_handle->vChrFilter, &p_handle->vChrFilterPos, &p_handle->vChrFilterSize,
				p_handle->chrYInc, p_handle->chrSrcH, p_handle->chrDstH, filterAlign, (1 << 12), 64, 64)) < 0)
				goto fail;
		}
	}

	free(desc_src);
	free(desc_dst);
	return init_filters(p_handle, x, y);
fail: // FIXME replace things by appropriate error codes
	return -1;
}

static int init_handletoint(fs_scale_handle * p_handle ,float *x, float *y)
{
	int i;
	int usesVFilter, usesHFilter;

	int srcW = p_handle->srcW;
	int srcH = p_handle->srcH;
	int dstW = p_handle->dstW;
	int dstH = p_handle->dstH;

	FS_IMG_FMT_S srcFormat = (FS_IMG_FMT_S)p_handle->srcFormat;/*AV_PIX_FMT_NV21*/
	FS_IMG_FMT_S dstFormat = (FS_IMG_FMT_S)p_handle->dstFormat;/*AV_PIX_FMT_NV21*/
	const AVPixFmtDescriptor *desc_src;
	const AVPixFmtDescriptor *desc_dst;
	int ret = 0;

	static const float float_mult = 1.0f / 255.0f;

	desc_src = av_pix_fmt_desc_get((FS_IMG_FMT_S)srcFormat);
	desc_dst = av_pix_fmt_desc_get((FS_IMG_FMT_S)dstFormat);


	/* sanity check */
	if (srcW < 1 || srcH < 1 || dstW < 1 || dstH < 1) { return -1; }

	p_handle->lumXInc = (((fs_int64)srcW << 16) + (dstW >> 1)) / dstW;
	p_handle->lumYInc = (((fs_int64)srcH << 16) + (dstH >> 1)) / dstH;
	p_handle->dstFormatBpp = 16; //av_get_bits_per_pixel(desc_dst);		// 16
	p_handle->srcFormatBpp = 16; // av_get_bits_per_pixel(desc_src);	// 16

	p_handle->chrSrcHSubSample = 1;
	p_handle->chrSrcVSubSample = 1;
	p_handle->chrDstHSubSample = 1;
	p_handle->chrDstVSubSample = 1;

	p_handle->chrSrcW = srcW / 2;
	p_handle->chrSrcH = srcH / 2;
	p_handle->chrDstW = dstW / 2;
	p_handle->chrDstH = dstH / 2;

	p_handle->srcBpc = desc_src->comp[0].depth;
	p_handle->dstBpc = desc_dst->comp[0].depth;

	p_handle->chrXInc = (((fs_int64)p_handle->chrSrcW << 16) + (p_handle->chrDstW >> 1)) / p_handle->chrDstW;
	p_handle->chrYInc = (((fs_int64)p_handle->chrSrcH << 16) + (p_handle->chrDstH >> 1)) / p_handle->chrDstH;

	/* precalculate horizontal scaler filter coefficients */
	{
		const int filterAlign = 1;	//8
		if (p_handle->flag & 0x1) // fast mode
		{
			if ((ret = initFilter_fast(&p_handle->hLumFilter, &p_handle->hLumFilterPos,
				&p_handle->hLumFilterSize, p_handle->lumXInc,
				srcW, dstW, filterAlign, 1 << 14, 128, 128)) < 0)
				goto fail;
			if ((ret = initFilter_fast(&p_handle->hChrFilter, &p_handle->hChrFilterPos,
				&p_handle->hChrFilterSize, p_handle->chrXInc,
				p_handle->chrSrcW, p_handle->chrDstW, filterAlign, 1 << 14, 64, 64)) < 0)
				goto fail;
		}
		else
		{
			if ((ret = initFilter(&p_handle->hLumFilter, &p_handle->hLumFilterPos,
				&p_handle->hLumFilterSize, p_handle->lumXInc,
				srcW, dstW, filterAlign, 1 << 14, 128, 128)) < 0)
				goto fail;
			if ((ret = initFilter(&p_handle->hChrFilter, &p_handle->hChrFilterPos,
				&p_handle->hChrFilterSize, p_handle->chrXInc,
				p_handle->chrSrcW, p_handle->chrDstW, filterAlign, 1 << 14, 64, 64)) < 0)
				goto fail;
		}
	} // initialize horizontal stuff

	  /* precalculate vertical scaler filter coefficients */
	{
		const int filterAlign = 1;
		if (p_handle->flag & 0x1) // fast mode
		{
			if ((ret = initFilter_fast(&p_handle->vLumFilter, &p_handle->vLumFilterPos, &p_handle->vLumFilterSize,
				p_handle->lumYInc, srcH, dstH, filterAlign, (1 << 12), 128, 128)) < 0)
				goto fail;
			if ((ret = initFilter_fast(&p_handle->vChrFilter, &p_handle->vChrFilterPos, &p_handle->vChrFilterSize,
				p_handle->chrYInc, p_handle->chrSrcH, p_handle->chrDstH, filterAlign, (1 << 12), 64, 64)) < 0)
				goto fail;
		}
		else
		{
			if ((ret = initFilter(&p_handle->vLumFilter, &p_handle->vLumFilterPos, &p_handle->vLumFilterSize,
				p_handle->lumYInc, srcH, dstH, filterAlign, (1 << 12), 128, 128)) < 0)
				goto fail;
			if ((ret = initFilter(&p_handle->vChrFilter, &p_handle->vChrFilterPos, &p_handle->vChrFilterSize,
				p_handle->chrYInc, p_handle->chrSrcH, p_handle->chrDstH, filterAlign, (1 << 12), 64, 64)) < 0)
				goto fail;
		}
	}

	free(desc_src);
	free(desc_dst);
	return init_filterstoint(p_handle, x, y);
fail: // FIXME replace things by appropriate error codes
	return -1;
}

int scale(fs_scale_handle *c, const uint8_t *const srcSlice[],
	const int srcStride[], int srcSliceY, int srcSliceH,
	void *const dst[], const int dstStride[], int degree)
	{
		if(degree > -1 && degree < 4)
		{ 
			if(c->form == 0)my_sws_scale(c, srcSlice, srcStride, 0, srcSliceH, (float*)dst, dstStride, degree);//tofloat
			if(c->form == 1)my_sws_scaletoint(c, srcSlice, srcStride, 0, srcSliceH, (uint8_t*)dst, dstStride, degree);//toint
		}
		else
		{
			printf("input valid!\n");
			return -1;
		}
	}


fs_scale_handle * fs_getScaleHandle(int srcW, int srcH, FS_IMG_FMT_S srcFormat, int dstW, int dstH, FS_IMG_FMT_S dstFormat, fs_int64 flag, float *x, float *y, int toint)
{
	int ret = 0;
	fs_scale_handle * p_handle = (fs_scale_handle *)malloc(sizeof(fs_scale_handle));
	if (!p_handle) { printf("cant malloc\n"); return NULL; }

	p_handle->form = toint;
	p_handle->srcH = srcH;
	p_handle->srcW = srcW;
	p_handle->srcFormat = srcFormat;
	p_handle->dstH = dstH;
	p_handle->dstW = dstW;
	p_handle->dstFormat = dstFormat;
	p_handle->flag = flag;

	if(toint == 0)ret = init_handle(p_handle, x, y);
	else if(toint == 1)ret = init_handletoint(p_handle, x, y);
	else printf("toint invalid\n");
	if (ret) { printf("Init scale handle failed.\n"); goto FAIL; }

	return p_handle;
FAIL:
	free(p_handle);
	p_handle = NULL;
	return NULL;
}

void fs_freeScaleHandle(fs_scale_handle * p_handle)
{

	free(p_handle->vLumFilter);
	free(p_handle->vChrFilter);
	free(p_handle->hLumFilter);
	free(p_handle->hChrFilter);

	free(p_handle->vLumFilterPos);
	free(p_handle->vChrFilterPos);
	free(p_handle->hLumFilterPos);
	free(p_handle->hChrFilterPos);

	free_filters(p_handle);
}

static int swscale(fs_scale_handle *c, const uint8_t *src[], int srcStride[], int srcSliceY, int srcSliceH, float *dst[], int dstStride[])
{
	const int dstW = c->dstW;
	const int dstH = c->dstH;
	int32_t *vLumFilterPos = c->vLumFilterPos;
	int32_t *vChrFilterPos = c->vChrFilterPos;
	const int vLumFilterSize = c->vLumFilterSize;
	const int vChrFilterSize = c->vChrFilterSize;
	c->chrDither8 = c->lumDither8 = sws_pb_64;
	yuv2planarX_fn yuv2planeX = yuv2planeX_8_c;// = c->yuv2planeX;
	yuv2interleavedX_fn yuv2nv12cX = yuv2nv12cX_c;// = c->yuv2nv12cX;
	const int chrSrcSliceY = srcSliceY >> c->chrSrcVSubSample;
	const int chrSrcSliceH = AV_CEIL_RSHIFT(srcSliceH, c->chrSrcVSubSample);
	int lastDstY;
	int dstY;// = c->dstY;
	int lumBufIndex;//= c->lumBufIndex;
	int chrBufIndex;//= c->chrBufIndex;
	int lastInLumBuf;// = c->lastInLumBuf;
	int lastInChrBuf;// = c->lastInChrBuf;
	int lumStart = 0;
	int lumEnd = c->descIndex[0];
	int chrStart = lumEnd;
	int chrEnd = c->descIndex[1];
	int vStart = chrEnd;
	int vEnd = c->numDesc - 1;
	SwsSlice *src_slice = &c->slice[lumStart];
	SwsSlice *hout_slice = &c->slice[c->numSlice - 3];
	SwsSlice *out_slice = &c->slice[c->numSlice - 1];
	SwsFilterDescriptor *desc = c->desc;
	int hasLumHoles = 1;
	int hasChrHoles = 1;

	if (srcSliceY == 0) 
	{
		lumBufIndex = -1;
		chrBufIndex = -1;
		dstY = 0;
		lastInLumBuf = -1;
		lastInChrBuf = -1;
	}

	lastDstY = dstY;

	init_vscale_pfn(c, yuv2planeX, yuv2nv12cX);

	init_slice_from_src(src_slice, (uint8_t**)src, srcStride, c->srcW,
		srcSliceY, srcSliceH, chrSrcSliceY, chrSrcSliceH, 1);

	init_slice_from_dst(out_slice, (float**)dst, dstStride, c->dstW,
		dstY, dstH, dstY >> c->chrDstVSubSample, AV_CEIL_RSHIFT(dstH, c->chrDstVSubSample), 0);

	if (srcSliceY == 0) 
	{
		hout_slice->plane[0].sliceY = lastInLumBuf + 1;
		hout_slice->plane[1].sliceY = lastInChrBuf + 1;
		hout_slice->plane[2].sliceY = lastInChrBuf + 1;
		hout_slice->plane[3].sliceY = lastInLumBuf + 1;
		hout_slice->plane[0].sliceH = hout_slice->plane[1].sliceH = hout_slice->plane[2].sliceH = hout_slice->plane[3].sliceH = 0;
		hout_slice->width = dstW;
	}

	int yeah = 0;
	for (; dstY < dstH; dstY++)
	{
		const int chrDstY = dstY >> c->chrDstVSubSample;
		const int firstLumSrcY = FFMAX(1 - vLumFilterSize, vLumFilterPos[dstY]);
		const int firstLumSrcY2 = FFMAX(1 - vLumFilterSize, vLumFilterPos[FFMIN(dstY | ((1 << c->chrDstVSubSample) - 1), dstH - 1)]);
		const int firstChrSrcY = FFMAX(1 - vChrFilterSize, vChrFilterPos[chrDstY]);
		int lastLumSrcY = FFMIN(c->srcH, firstLumSrcY + vLumFilterSize) - 1;
		int lastLumSrcY2 = FFMIN(c->srcH, firstLumSrcY2 + vLumFilterSize) - 1;
		int lastChrSrcY = FFMIN(c->chrSrcH, firstChrSrcY + vChrFilterSize) - 1;
		int enough_lines;
		int i;
		int posY, cPosY, firstPosY, lastPosY, firstCPosY, lastCPosY;

		if (firstLumSrcY > lastInLumBuf)
		{
			hasLumHoles = lastInLumBuf != firstLumSrcY - 1;
			if (hasLumHoles) 
			{
				hout_slice->plane[0].sliceY = firstLumSrcY;
				hout_slice->plane[3].sliceY = firstLumSrcY;
				hout_slice->plane[0].sliceH = hout_slice->plane[3].sliceH = 0;
			}

			lastInLumBuf = firstLumSrcY - 1;
		}
		if (firstChrSrcY > lastInChrBuf) 
		{
			hasChrHoles = lastInChrBuf != firstChrSrcY - 1;
			if (hasChrHoles) 
			{
				hout_slice->plane[1].sliceY = firstChrSrcY;
				hout_slice->plane[2].sliceY = firstChrSrcY;
				hout_slice->plane[1].sliceH = hout_slice->plane[2].sliceH = 0;
			}
			lastInChrBuf = firstChrSrcY - 1;
		}

		enough_lines = lastLumSrcY2 < srcSliceY + srcSliceH && lastChrSrcY < AV_CEIL_RSHIFT(srcSliceY + srcSliceH, c->chrSrcVSubSample);

		if (!enough_lines) 
		{
			lastLumSrcY = srcSliceY + srcSliceH - 1;
			lastChrSrcY = chrSrcSliceY + chrSrcSliceH - 1;
		}

		posY = hout_slice->plane[0].sliceY + hout_slice->plane[0].sliceH;
		if (posY <= lastLumSrcY && !hasLumHoles) 
		{
			firstPosY = FFMAX(firstLumSrcY, posY);
			lastPosY = FFMIN(firstLumSrcY + hout_slice->plane[0].available_lines - 1, srcSliceY + srcSliceH - 1);
		}
		else 
		{
			firstPosY = posY;
			lastPosY = lastLumSrcY;
		}

		cPosY = hout_slice->plane[1].sliceY + hout_slice->plane[1].sliceH;
		if (cPosY <= lastChrSrcY && !hasChrHoles) 
		{
			firstCPosY = FFMAX(firstChrSrcY, cPosY);
			lastCPosY = FFMIN(firstChrSrcY + hout_slice->plane[1].available_lines - 1, AV_CEIL_RSHIFT(srcSliceY + srcSliceH, c->chrSrcVSubSample) - 1);
		}
		else {
			firstCPosY = cPosY;
			lastCPosY = lastChrSrcY;
		}

		rotate_slice(hout_slice, lastPosY, lastCPosY);

		if (posY < lastLumSrcY + 1) 
		{
			for (i = lumStart; i < lumEnd; ++i)
				desc[i].process(c, &desc[i], firstPosY, lastPosY - firstPosY + 1);
		}

		lumBufIndex += lastLumSrcY - lastInLumBuf;
		lastInLumBuf = lastLumSrcY;

		if (cPosY < lastChrSrcY + 1) 
		{
			for (i = chrStart; i < chrEnd; ++i)
				desc[i].process(c, &desc[i], firstCPosY, lastCPosY - firstCPosY + 1);
		}

		chrBufIndex += lastChrSrcY - lastInChrBuf;
		lastInChrBuf = lastChrSrcY;

		if (lumBufIndex >= vLumFilterSize)
			lumBufIndex -= vLumFilterSize;
		if (chrBufIndex >= vChrFilterSize)
			chrBufIndex -= vChrFilterSize;
		if (!enough_lines)
			break; 

		if (dstY >= dstH - 2) init_vscale_pfn(c, yuv2planeX_8_c, yuv2nv12cX_c);
		
		for (i = vStart; i < vEnd; ++i)
			desc[i].process(c, &desc[i], dstY, 1);
		
		desc[5].processx(c, &desc[5], dstY, 1, yeah);
		yeah ++;
	}
	return dstY - lastDstY;
}



static int swscaletoint(fs_scale_handle *c, const uint8_t *src[], int srcStride[], int srcSliceY, int srcSliceH, float *dst[], int dstStride[])
{
	const int dstW = c->dstW;
	const int dstH = c->dstH;
	int32_t *vLumFilterPos = c->vLumFilterPos;
	int32_t *vChrFilterPos = c->vChrFilterPos;
	const int vLumFilterSize = c->vLumFilterSize;
	const int vChrFilterSize = c->vChrFilterSize;
	c->chrDither8 = c->lumDither8 = sws_pb_64;
	yuv2planarX_fn yuv2planeX = yuv2planeX_8_c;// = c->yuv2planeX;
	yuv2interleavedX_fn yuv2nv12cX = yuv2nv12cX_c;// = c->yuv2nv12cX;
	const int chrSrcSliceY = srcSliceY >> c->chrSrcVSubSample;
	const int chrSrcSliceH = AV_CEIL_RSHIFT(srcSliceH, c->chrSrcVSubSample);
	int lastDstY;
	int dstY;// = c->dstY;
	int lumBufIndex;//= c->lumBufIndex;
	int chrBufIndex;//= c->chrBufIndex;
	int lastInLumBuf;// = c->lastInLumBuf;
	int lastInChrBuf;// = c->lastInChrBuf;
	int lumStart = 0;
	int lumEnd = c->descIndex[0];
	int chrStart = lumEnd;
	int chrEnd = c->descIndex[1];
	int vStart = chrEnd;
	int vEnd = c->numDesc;
	SwsSlice *src_slice = &c->slice[lumStart];
	SwsSlice *hout_slice = &c->slice[c->numSlice - 2];
	SwsSlice *vout_slice = &c->slice[c->numSlice - 1];
	SwsFilterDescriptor *desc = c->desc;
	int hasLumHoles = 1;
	int hasChrHoles = 1;

	if (srcSliceY == 0) 
	{
		lumBufIndex = -1;
		chrBufIndex = -1;
		dstY = 0;
		lastInLumBuf = -1;
		lastInChrBuf = -1;
	}

	lastDstY = dstY;
	init_vscale_pfntoint(c, yuv2planeX, yuv2nv12cX);
	init_slice_from_src(src_slice, (uint8_t**)src, srcStride, c->srcW, srcSliceY, srcSliceH, chrSrcSliceY, chrSrcSliceH, 1);
	init_slice_from_src(vout_slice, (uint8_t**)dst, dstStride, c->dstW, dstY, dstH, dstY >> c->chrDstVSubSample, AV_CEIL_RSHIFT(dstH, c->chrDstVSubSample), 0);
	
	if (srcSliceY == 0) 
	{
		hout_slice->plane[0].sliceY = lastInLumBuf + 1;
		hout_slice->plane[1].sliceY = lastInChrBuf + 1;
		hout_slice->plane[2].sliceY = lastInChrBuf + 1;
		hout_slice->plane[3].sliceY = lastInLumBuf + 1;
		hout_slice->plane[0].sliceH = hout_slice->plane[1].sliceH = hout_slice->plane[2].sliceH = hout_slice->plane[3].sliceH = 0;
		hout_slice->width = dstW;
	}

	for (; dstY < dstH; dstY++)
	{
		const int chrDstY = dstY >> c->chrDstVSubSample;
		const int firstLumSrcY = FFMAX(1 - vLumFilterSize, vLumFilterPos[dstY]);
		const int firstLumSrcY2 = FFMAX(1 - vLumFilterSize, vLumFilterPos[FFMIN(dstY | ((1 << c->chrDstVSubSample) - 1), dstH - 1)]);
		const int firstChrSrcY = FFMAX(1 - vChrFilterSize, vChrFilterPos[chrDstY]);
		int lastLumSrcY = FFMIN(c->srcH, firstLumSrcY + vLumFilterSize) - 1;
		int lastLumSrcY2 = FFMIN(c->srcH, firstLumSrcY2 + vLumFilterSize) - 1;
		int lastChrSrcY = FFMIN(c->chrSrcH, firstChrSrcY + vChrFilterSize) - 1;
		int enough_lines;
		int i;
		int posY, cPosY, firstPosY, lastPosY, firstCPosY, lastCPosY;

		if (firstLumSrcY > lastInLumBuf)
		{
			hasLumHoles = lastInLumBuf != firstLumSrcY - 1;
			if (hasLumHoles) 
			{
				hout_slice->plane[0].sliceY = firstLumSrcY;
				hout_slice->plane[3].sliceY = firstLumSrcY;
				hout_slice->plane[0].sliceH = hout_slice->plane[3].sliceH = 0;
			}
			lastInLumBuf = firstLumSrcY - 1;
		}

		if (firstChrSrcY > lastInChrBuf) 
		{
			hasChrHoles = lastInChrBuf != firstChrSrcY - 1;
			if (hasChrHoles) 
			{
				hout_slice->plane[1].sliceY = firstChrSrcY;
				hout_slice->plane[2].sliceY = firstChrSrcY;
				hout_slice->plane[1].sliceH = hout_slice->plane[2].sliceH = 0;
			}
			lastInChrBuf = firstChrSrcY - 1;
		}


		enough_lines = lastLumSrcY2 < srcSliceY + srcSliceH && lastChrSrcY < AV_CEIL_RSHIFT(srcSliceY + srcSliceH, c->chrSrcVSubSample);

		if (!enough_lines) 
		{
			lastLumSrcY = srcSliceY + srcSliceH - 1;
			lastChrSrcY = chrSrcSliceY + chrSrcSliceH - 1;
		}

		posY = hout_slice->plane[0].sliceY + hout_slice->plane[0].sliceH;
		if (posY <= lastLumSrcY && !hasLumHoles)  
		{
			firstPosY = FFMAX(firstLumSrcY, posY);
			lastPosY = FFMIN(firstLumSrcY + hout_slice->plane[0].available_lines - 1, srcSliceY + srcSliceH - 1);
		}
		else 
		{
			firstPosY = posY;
			lastPosY = lastLumSrcY;
		}

		cPosY = hout_slice->plane[1].sliceY + hout_slice->plane[1].sliceH;
		if (cPosY <= lastChrSrcY && !hasChrHoles) 
		{
			firstCPosY = FFMAX(firstChrSrcY, cPosY);
			lastCPosY = FFMIN(firstChrSrcY + hout_slice->plane[1].available_lines - 1, AV_CEIL_RSHIFT(srcSliceY + srcSliceH, c->chrSrcVSubSample) - 1);
		}
		else 
		{
			firstCPosY = cPosY;
			lastCPosY = lastChrSrcY;
		}

		rotate_slice(hout_slice, lastPosY, lastCPosY);

		if (posY < lastLumSrcY + 1) 
		{
			for (i = lumStart; i < lumEnd; ++i)
				desc[i].process(c, &desc[i], firstPosY, lastPosY - firstPosY + 1);
		}

		lumBufIndex += lastLumSrcY - lastInLumBuf;
		lastInLumBuf = lastLumSrcY;

		if (cPosY < lastChrSrcY + 1) 
		{
			for (i = chrStart; i < chrEnd; ++i)
				desc[i].process(c, &desc[i], firstCPosY, lastCPosY - firstCPosY + 1);
		}

		chrBufIndex += lastChrSrcY - lastInChrBuf;
		lastInChrBuf = lastChrSrcY;

		if (lumBufIndex >= vLumFilterSize)
			lumBufIndex -= vLumFilterSize;
		if (chrBufIndex >= vChrFilterSize)
			chrBufIndex -= vChrFilterSize;
		if (!enough_lines) break;  
		if (dstY >= dstH - 2) init_vscale_pfntoint(c, yuv2planeX_8_c, yuv2nv12cX_c);

		for (i = vStart; i < vEnd; ++i)
			desc[i].process(c, &desc[i], dstY, 1);
	}
	return dstY - lastDstY;
}




int  my_sws_scale(fs_scale_handle *c, const uint8_t * const srcSlice[], const int srcStride[], int srcSliceY, int srcSliceH, float *const dst[], const int dstStride[], int degree)
{
	int i, ret = 0;
	const uint8_t *src2[4];
	float *dst2[4];
	int macro_height = 1 << c->chrSrcVSubSample;
	int srcStride2[4];
	int dstStride2[4];
	int srcSliceY_internal = srcSliceY;

	if (!srcStride || !dstStride || !dst || !srcSlice) return -1;

	for (i = 0; i < 4; i++) 
	{
		srcStride2[i] = srcStride[i];
		dstStride2[i] = dstStride[i];
	}

	if ((srcSliceY & (macro_height - 1)) ||
		((srcSliceH& (macro_height - 1)) && srcSliceY + srcSliceH != c->srcH) ||
		srcSliceY + srcSliceH > c->srcH) return -2;
	
	memcpy(src2, srcSlice, sizeof(src2));
	memcpy(dst2, dst, sizeof(dst2));

	if (srcSliceH == 0) return 0;

	c->sliceDir = 1;
	c->degree = degree;
	src2[3] = src2[2] = NULL;
	dst2[3] = dst2[2] = NULL;
	ret = swscale(c, src2, srcStride2, srcSliceY_internal, srcSliceH, dst2, dstStride2);

	return ret;
}


int  my_sws_scaletoint(fs_scale_handle *c, const uint8_t * const srcSlice[], const int srcStride[], int srcSliceY, int srcSliceH, uint8_t *const dst[], const int dstStride[], int degree)
{
	
	int i, ret = 0;
	const uint8_t *src2[4];
	uint8_t *dst2[4];
	int macro_height = 1 << c->chrSrcVSubSample;
	int srcStride2[4];
	int dstStride2[4];
	int srcSliceY_internal = srcSliceY;

	if (!srcStride || !dstStride || !dst || !srcSlice) return -1;
	for (i = 0; i < 4; i++) 
	{
		srcStride2[i] = srcStride[i];
		dstStride2[i] = dstStride[i];
	}

	if ((srcSliceY & (macro_height - 1)) ||
		((srcSliceH& (macro_height - 1)) && srcSliceY + srcSliceH != c->srcH) ||
		srcSliceY + srcSliceH > c->srcH) return -2;
	
	memcpy(src2, srcSlice, sizeof(src2));
	memcpy(dst2, dst, sizeof(dst2));

	if (srcSliceH == 0) return 0;

	c->sliceDir = 1;
	c->degree = degree;
	src2[3] = src2[2] = NULL;
	dst2[3] = dst2[2] = NULL;
	ret = swscaletoint(c, src2, srcStride2, srcSliceY_internal, srcSliceH, dst2, dstStride2);

	return ret;
}

