#include "filter.h"
#include <stdio.h>
#include <stdlib.h>



const uint8_t ff_log2_tab[256] = {
	0,0,1,1,2,2,2,2,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
	5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
	6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
	6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7
};
#define SWS_MAX_REDUCE_CUTOFF 0.002

int initFilter(int16_t **outFilter, int32_t **filterPos,
	int *outFilterSize, int xInc, int srcW,
	int dstW, int filterAlign, int one, int srcPos, int dstPos)
{
	int i;
	int filterSize;
	int filter2Size;
	int minFilterSize;
	fs_int64 *filter = NULL;
	fs_int64 *filter2 = NULL;
	const fs_int64 fone = 1LL << (54 - FFMIN(ff_log2_tab[srcW / dstW], 8));	// 1 << 52
	int ret = -1;

	*filterPos = (int32_t *)malloc(sizeof(int32_t) * (dstW + 3));
	if (*filterPos == NULL) { printf("can't malloc\n"); return -1; }

	fs_int64 xDstInSrc;
	int sizeFactor = 2;

	if (xInc <= 1 << 16)	
		filterSize = 1 + sizeFactor;    // upscale
	else
		filterSize = 1 + (sizeFactor * srcW + dstW - 1) / dstW;

	filterSize = FFMIN(filterSize, srcW - 2);
	filterSize = FFMAX(filterSize, 1);

	filter = (fs_int64 *)malloc(sizeof(fs_int64) * dstW * filterSize);
	if (filter == NULL) { printf("can't malloc.\n"); free(*filterPos); return -2; }

	xDstInSrc = ((dstPos*(fs_int64)xInc) >> 7) - ((srcPos * 0x10000LL) >> 7);
	for (i = 0; i < dstW; i++)
	{
		int xx = (xDstInSrc - (filterSize - 2) * (1LL << 16)) / (1 << 17);
		int j;
		(*filterPos)[i] = xx;
		for (j = 0; j < filterSize; j++)
		{
			fs_int64 d = (FFABS(((fs_int64)xx * (1 << 17)) - xDstInSrc)) << 13; 
			double floatd;
			fs_int64 coeff;

			if (xInc > 1 << 16)
				d = d * dstW / srcW; 
			floatd = d * (1.0 / (1 << 30)); 

			coeff = (1 << 30) - d;
			if (coeff < 0)
				coeff = 0;
			coeff *= fone >> 30;

			filter[i * filterSize + j] = coeff;
			xx++;
		}
		xDstInSrc += 2 * xInc;
	}

	filter2Size = filterSize; 

	// TODO : filter2 not used in our case. Delete it.
	filter2 = (fs_int64 *)malloc(sizeof(fs_int64) * dstW * filter2Size);
	if (filter2 == NULL) { printf("can't malloc.\n"); free(filter); free(*filterPos); return -3; }

	for (i = 0; i < dstW; i++)
	{
		int j;

		for (j = 0; j < filterSize; j++)
			filter2[i * filter2Size + j] = filter[i * filterSize + j];

		// FIXME dstFilter
		(*filterPos)[i] += (filterSize - 1) / 2 - (filter2Size - 1) / 2;
	}
	free(filter);

	/* try to reduce the filter-size (step1 find size and shift left) */
	// Assume it is near normalized (*0.5 or *2.0 is OK but * 0.001 is not).
	minFilterSize = 0;
	for (i = dstW - 1; i >= 0; i--)
	{
		int min = filter2Size;
		int j;
		fs_int64 cutOff = 0.0;

		/* get rid of near zero elements on the left by shifting left */
		for (j = 0; j < filter2Size; j++) 
		{
			int k;
			cutOff += FFABS(filter2[i * filter2Size]);

			if (cutOff > SWS_MAX_REDUCE_CUTOFF * fone)
				break;

			/* preserve monotonicity because the core can't handle the
			* filter otherwise */
			if (i < dstW - 1 && (*filterPos)[i] >= (*filterPos)[i + 1])
				break;

			// move filter coefficients left
			for (k = 1; k < filter2Size; k++)
				filter2[i * filter2Size + k - 1] = filter2[i * filter2Size + k];
			filter2[i * filter2Size + k - 1] = 0;
			(*filterPos)[i]++;
		}

		cutOff = 0;
		/* count near zeros on the right */
		for (j = filter2Size - 1; j > 0; j--) 
		{
			cutOff += FFABS(filter2[i * filter2Size + j]);

			if (cutOff > SWS_MAX_REDUCE_CUTOFF * fone)
				break;
			min--;
		}

		if (min > minFilterSize)
			minFilterSize = min;
	}

	filterSize = (minFilterSize + (filterAlign - 1)) & (~(filterAlign - 1));

	filter = (fs_int64 *)malloc(sizeof(fs_int64) * dstW * filterSize);
	if (filter == NULL) { printf("can't malloc.\n"); free(filter2); free(*filterPos); return -4; }

	*outFilterSize = filterSize;

	/* try to reduce the filter-size (step2 reduce it) */
	for (i = 0; i < dstW; i++)
	{
		int j;
		for (j = 0; j < filterSize; j++)
		{
			if (j >= filter2Size)
				filter[i * filterSize + j] = 0;
			else
				filter[i * filterSize + j] = filter2[i * filter2Size + j];
		}
	}
	free(filter2);

	// fix borders
	for (i = 0; i < dstW; i++)
	{
		int j;
		if ((*filterPos)[i] < 0)
		{
			// move filter coefficients left to compensate for filterPos
			for (j = 1; j < filterSize; j++)
			{
				int left = FFMAX(j + (*filterPos)[i], 0);
				filter[i * filterSize + left] += filter[i * filterSize + j];
				filter[i * filterSize + j] = 0;
			}
			(*filterPos)[i] = 0;
		}

		if ((*filterPos)[i] + filterSize > srcW)
		{
			int shift = (*filterPos)[i] + FFMIN(filterSize - srcW, 0);
			fs_int64 acc = 0;

			for (j = filterSize - 1; j >= 0; j--) 
			{
				if ((*filterPos)[i] + j >= srcW) 
				{
					acc += filter[i * filterSize + j];
					filter[i * filterSize + j] = 0;
				}
			}
			for (j = filterSize - 1; j >= 0; j--) 
			{
				if (j < shift) 
					filter[i * filterSize + j] = 0;
				else 
					filter[i * filterSize + j] = filter[i * filterSize + j - shift];
			}

			(*filterPos)[i] -= shift;
			filter[i * filterSize + srcW - 1 - (*filterPos)[i]] += acc;
		}
	}

	// Note the +1 is for the MMX scaler which reads over the end
	/* align at 16 for AltiVec (needed by hScale_altivec_real) */
	//FF_ALLOCZ_ARRAY_OR_GOTO(NULL, *outFilter,
	//	(dstW + 3), *outFilterSize * sizeof(int16_t), fail);
	*outFilter = (int16_t *)malloc(sizeof(int16_t) * (*outFilterSize) * (dstW + 3));
	if (*outFilter == NULL) { printf("can't malloc.\n"); free(filter); free(*filterPos); return -5; }

	/* normalize & store in outFilter */
	for (i = 0; i < dstW; i++)
	{
		int j;
		fs_int64 error = 0;
		fs_int64 sum = 0;

		for (j = 0; j < filterSize; j++) 
		{
			sum += filter[i * filterSize + j];
		}
		sum = (sum + one / 2) / one;
		if (!sum) 
		{
			printf("SwScaler: zero vector in scaling\n");
			sum = 1;
		}
		for (j = 0; j < *outFilterSize; j++)
		{
			fs_int64 v = filter[i * filterSize + j] + error;
			int intV = ROUNDED_DIV(v, sum);
			(*outFilter)[i * (*outFilterSize) + j] = intV;
			error = v - intV * sum;
		}
	}

	(*filterPos)[dstW + 0] = (*filterPos)[dstW + 1] = (*filterPos)[dstW + 2] = (*filterPos)[dstW - 1]; /* the MMX/SSE scaler will
														 * read over the end */
	for (i = 0; i < *outFilterSize; i++)
	{
		int k = (dstW - 1) * (*outFilterSize) + i;
		(*outFilter)[k + 1 * (*outFilterSize)] = (*outFilter)[k + 2 * (*outFilterSize)] = (*outFilter)[k + 3 * (*outFilterSize)] = (*outFilter)[k];
	}
	ret = 0;
	free(filter);
	return ret;
}

int initFilter_fast(int16_t **outFilter, int32_t **filterPos,
	int *outFilterSize, int xInc, int srcW,
	int dstW, int filterAlign, int one, int srcPos, int dstPos)
{

	int i;
	int filterSize;
	int filter2Size;
	int minFilterSize;
	fs_int64 *filter = NULL;
	fs_int64 *filter2 = NULL;
	const fs_int64 fone = 1LL << (54 - FFMIN(ff_log2_tab[srcW / dstW], 8));
	int ret = -1;

	*filterPos = (int32_t *)malloc(sizeof(int32_t) * (dstW + 3));
	if (*filterPos == NULL) { printf("can't malloc\n"); return -1; }


	fs_int64 xDstInSrc;
	filterSize = 2;
	if (one == 1 << 12 && srcPos == 64)
		filterSize = 1;

	filter = (fs_int64 *)malloc(sizeof(fs_int64) * dstW * filterSize);
	if (filter == NULL) { printf("can't malloc.\n"); free(*filterPos); return -2; }

	xDstInSrc = ((dstPos*(fs_int64)xInc) >> 8) - ((srcPos * 0x8000LL) >> 7);
	for (i = 0; i < dstW; i++) {
		int xx = (xDstInSrc - ((filterSize - 1) << 15) + (1 << 15)) >> 16;
		int j;

		(*filterPos)[i] = xx;
		// bilinear upscale / linear interpolate / area averaging
		if (one == 1 << 12 && srcPos == 64)
		{
			filter[i * filterSize] = fone;
			for (j = 1; j < filterSize; j++)
			{
				filter[i * filterSize + j] = 0;
			}
		}
		else
		{
			for (j = 0; j < filterSize; j++)
			{
				fs_int64 coeff = fone - FFABS(((fs_int64)xx << 16) - xDstInSrc)*(fone >> 16);
				if (coeff < 0)
					coeff = 0;
				filter[i * filterSize + j] = coeff;
				xx++;
			}
		}
		xDstInSrc += xInc;
	}


	/* apply src & dst Filter to filter -> filter2
	* av_free(filter);
	*/
	filter2Size = filterSize;
	// TODO : filter2 not used in our case. Delete it.
	filter2 = (fs_int64 *)malloc(sizeof(fs_int64) * dstW * filter2Size);
	if (filter2 == NULL) { printf("can't malloc.\n"); free(filter); free(*filterPos); return -3; }

	for (i = 0; i < dstW; i++)
	{
		int j, k;

		for (j = 0; j < filterSize; j++)
			filter2[i * filter2Size + j] = filter[i * filterSize + j];
		// FIXME dstFilter
		(*filterPos)[i] += (filterSize - 1) / 2 - (filter2Size - 1) / 2;
	}
	free(filter);

	/* try to reduce the filter-size (step1 find size and shift left) */
	// Assume it is near normalized (*0.5 or *2.0 is OK but * 0.001 is not).
	minFilterSize = 0;
	for (i = dstW - 1; i >= 0; i--)
	{
		int min = filter2Size;
		int j;
		fs_int64 cutOff = 0.0;

		/* get rid of near zero elements on the left by shifting left */
		for (j = 0; j < filter2Size; j++)
		{
			int k;
			cutOff += FFABS(filter2[i * filter2Size]);

			if (cutOff > SWS_MAX_REDUCE_CUTOFF * fone)
				break;

			/* preserve monotonicity because the core can't handle the
			* filter otherwise */
			if (i < dstW - 1 && (*filterPos)[i] >= (*filterPos)[i + 1])
				break;

			// move filter coefficients left
			for (k = 1; k < filter2Size; k++)
				filter2[i * filter2Size + k - 1] = filter2[i * filter2Size + k];

			filter2[i * filter2Size + k - 1] = 0;
			(*filterPos)[i]++;
		}

		cutOff = 0;
		/* count near zeros on the right */
		for (j = filter2Size - 1; j > 0; j--) 
		{
			cutOff += FFABS(filter2[i * filter2Size + j]);
			if (cutOff > SWS_MAX_REDUCE_CUTOFF * fone)
				break;
			min--;
		}

		if (min > minFilterSize)
			minFilterSize = min;
	}

	filterSize = (minFilterSize + (filterAlign - 1)) & (~(filterAlign - 1));
	filter = (fs_int64 *)malloc(sizeof(fs_int64) * dstW * filterSize);
	if (filter == NULL) 
	{ 
		printf("can't malloc.\n"); 
		free(filter2); 
		free(*filterPos); 
		return -4; 
	}

	*outFilterSize = filterSize;

	/* try to reduce the filter-size (step2 reduce it) */
	for (i = 0; i < dstW; i++) 
	{
		int j;
		for (j = 0; j < filterSize; j++) 
		{
			if (j >= filter2Size)
				filter[i * filterSize + j] = 0;
			else
				filter[i * filterSize + j] = filter2[i * filter2Size + j];
		}
	}

	// FIXME try to align filterPos if possible
	// fix borders
	for (i = 0; i < dstW; i++) 
	{
		int j;
		if ((*filterPos)[i] < 0) 
		{
			// move filter coefficients left to compensate for filterPos
			for (j = 1; j < filterSize; j++) 
			{
				int left = FFMAX(j + (*filterPos)[i], 0);
				filter[i * filterSize + left] += filter[i * filterSize + j];
				filter[i * filterSize + j] = 0;
			}
			(*filterPos)[i] = 0;
		}

		if ((*filterPos)[i] + filterSize > srcW) 
		{
			int shift = (*filterPos)[i] + FFMIN(filterSize - srcW, 0);
			fs_int64 acc = 0;

			for (j = filterSize - 1; j >= 0; j--) 
			{
				if ((*filterPos)[i] + j >= srcW) 
				{
					acc += filter[i * filterSize + j];
					filter[i * filterSize + j] = 0;
				}
			}
			for (j = filterSize - 1; j >= 0; j--)
			{
				if (j < shift) 
					filter[i * filterSize + j] = 0;
				else 
					filter[i * filterSize + j] = filter[i * filterSize + j - shift];
			}

			(*filterPos)[i] -= shift;
			filter[i * filterSize + srcW - 1 - (*filterPos)[i]] += acc;
		}
	}

	// Note the +1 is for the MMX scaler which reads over the end
	/* align at 16 for AltiVec (needed by hScale_altivec_real) */
	//FF_ALLOCZ_ARRAY_OR_GOTO(NULL, *outFilter,
	//	(dstW + 3), *outFilterSize * sizeof(int16_t), fail);

	*outFilter = (int16_t *)malloc(sizeof(int16_t) * (*outFilterSize) * (dstW + 3));
	if (*outFilter == NULL) { printf("can't malloc.\n"); printf("can't malloc.\n"); free(filter); free(*filterPos); return -5; }
	//else{printf("success\n");}
	/* normalize & store in outFilter */
	for (i = 0; i < dstW; i++) {
		int j;
		fs_int64 error = 0;
		fs_int64 sum = 0;

		for (j = 0; j < filterSize; j++) 
			sum += filter[i * filterSize + j];
		sum = (sum + one / 2) / one;
		if (!sum) 
			sum = 1;
		for (j = 0; j < *outFilterSize; j++) 
		{
			fs_int64 v = filter[i * filterSize + j] + error;
			int intV = ROUNDED_DIV(v, sum);
			(*outFilter)[i * (*outFilterSize) + j] = intV;
			error = v - intV * sum;
		}
	}

	(*filterPos)[dstW + 0] = (*filterPos)[dstW + 1] = (*filterPos)[dstW + 2] = (*filterPos)[dstW - 1]; 
	for (i = 0; i < *outFilterSize; i++) 
	{
		int k = (dstW - 1) * (*outFilterSize) + i;
		(*outFilter)[k + 1 * (*outFilterSize)] = (*outFilter)[k + 2 * (*outFilterSize)] = (*outFilter)[k + 3 * (*outFilterSize)] = (*outFilter)[k];
	}

	ret = 0;

	free(filter);
	free(filter2);
	return ret;
}

static void fill_ones(SwsSlice *s, int n, int is16bit)
{
	int i;
	for (i = 0; i < 4; ++i) {
		int j;
		int size = s->plane[i].available_lines;
		for (j = 0; j < size; ++j) {
			int k;
			int end = is16bit ? n >> 1 : n;
			// fill also one extra element
			end += 1;
			if (is16bit)
				for (k = 0; k < end; ++k)
					((int32_t*)(s->plane[i].line[j]))[k] = 1 << 18;
			else
				for (k = 0; k < end; ++k)
					((int16_t*)(s->plane[i].line[j]))[k] = 1 << 14;
		}
	}
}

static void get_min_buffer_size(fs_scale_handle *c, int *out_lum_size, int *out_chr_size)
{
	int lumY;
	int dstH = c->dstH;
	int chrDstH = c->chrDstH;
	int *lumFilterPos = c->vLumFilterPos;
	int *chrFilterPos = c->vChrFilterPos;
	int lumFilterSize = c->vLumFilterSize;
	int chrFilterSize = c->vChrFilterSize;
	int chrSubSample = c->chrSrcVSubSample;

	*out_lum_size = lumFilterSize;
	*out_chr_size = chrFilterSize;

	for (lumY = 0; lumY < dstH; lumY++) {
		int chrY = (fs_int64)lumY * chrDstH / dstH;
		int nextSlice = FFMAX(lumFilterPos[lumY] + lumFilterSize - 1,
			((chrFilterPos[chrY] + chrFilterSize - 1)
				<< chrSubSample));

		nextSlice >>= chrSubSample;
		nextSlice <<= chrSubSample;
		(*out_lum_size) = FFMAX((*out_lum_size), nextSlice - lumFilterPos[lumY]);
		(*out_chr_size) = FFMAX((*out_chr_size), (nextSlice >> chrSubSample) - chrFilterPos[chrY]);
	}
}

int init_filters(fs_scale_handle * c, float *x, float * y)
{
	int res = 0;
	int dst_stride = FFALIGN(c->dstW * sizeof(int16_t) + 66, 16);

	int lumBufSize = 6;
	int chrBufSize = 5;
	int i;
	// TODO : !
	//get_min_buffer_size(c, &lumBufSize, &chrBufSize);
	//lumBufSize = FFMAX(lumBufSize, c->vLumFilterSize + 4);
	//chrBufSize = FFMAX(chrBufSize, c->vChrFilterSize + 4);

	/// 4 slices
	/// slice 0 : input(src)
	/// slice 1 : storage for chr convert output
	/// slice 2 : storage for hscale output
	/// slice 3 : storage for vscale output(dst)
	/// slice 0&3 uses input&output memory. slice 1&2 need extra memory allocated.
	c->numSlice = 5;
	

	/// 5 Descs
	/// Descs 0 : (slice 0) lum part hscale to (slice 2)
	/// Descs 1 : (slice 0) convert chr part to (slice 1). (nv21 to yuv420p)
	/// Descs 2 : (slice 1) chr part hscale to (slice 2)
	/// Descs 3 : (slice 2) lum part vscale to (slice 3)
	/// Descs 4 : (slice 2) chr part vscale to (slice 3)
	c->numDesc = 6;

	c->descIndex[0] = 1;
	c->descIndex[1] = 3;

	c->desc = (SwsFilterDescriptor *)malloc(c->numDesc * sizeof(SwsFilterDescriptor));
	c->slice = (SwsSlice *)malloc(sizeof(SwsSlice) * c->numSlice);

	/// slice 0 : input(src)
	res = alloc_slice(&c->slice[0], c->srcH, c->chrSrcH, c->chrSrcHSubSample, c->chrSrcVSubSample, 0);
	if (res < 0) goto cleanup;

	/// slice 1 : storage for chr convert output
	res = alloc_slice(&c->slice[1], lumBufSize, chrBufSize, c->chrSrcHSubSample, c->chrSrcVSubSample, 0);
	if (res < 0) goto cleanup;
	res = alloc_lines(&c->slice[1], FFALIGN(c->srcW * 2 + 78, 16), c->srcW);
	if (res < 0) goto cleanup;
	
	/// slice 2 : storage for hscale output
	res = alloc_slice(&c->slice[2], lumBufSize, chrBufSize, c->chrDstHSubSample, c->chrDstVSubSample, 1);
	if (res < 0) goto cleanup;
	res = alloc_lines(&c->slice[2], dst_stride, c->dstW);
	if (res < 0) goto cleanup;

	fill_ones(&c->slice[2], dst_stride >> 1, c->dstBpc == 16);

	/// slice 3 : storage for vscale output(dst)
	res = alloc_slice(&c->slice[3], c->dstH, c->chrDstH, c->chrDstHSubSample, c->chrDstVSubSample, 0);
	if (res < 0) goto cleanup;
	res = alloc_lines(&c->slice[3], dst_stride, c->dstW);
	if (res < 0) goto cleanup;
	
	// ///slice 4 : storage for final output
	{
	res = alloc_slicex(&c -> slice[4], c -> dstH, c->chrDstH, c->chrDstHSubSample, c->chrDstVSubSample, 0);
	if (res < 0) goto cleanup;}
	
	///Descs 0 : (slice 0) lum part hscale to(slice 2)
	res = init_desc_hscale(&c->desc[0], &c->slice[0], &c->slice[2], (uint16_t *)c->hLumFilter, c->hLumFilterPos, c->hLumFilterSize, c->lumXInc);
	if (res < 0) goto cleanup;

	/// Descs 1 : (slice 0) convert chr part to (slice 1). (nv21 to yuv420p)
	res = init_desc_cfmt_convert(&c->desc[1], &c->slice[0], &c->slice[1], 0);
	if (res < 0) goto cleanup;

	/// Descs 2 : (slice 1) chr part hscale to (slice 2)
	res = init_desc_chscale(&c->desc[2], &c->slice[1], &c->slice[2], (uint16_t *)c->hChrFilter, c->hChrFilterPos, c->hChrFilterSize, c->chrXInc);
	if (res < 0) goto cleanup;

	/// Descs 3 : (slice 2) lum part vscale to (slice 3)
	/// Descs 4 : (slice 2) chr part vscale to (slice 3)
	res = init_vscale(c, c->desc+3, c->slice+2, c->slice+3);
	if (res < 0) goto cleanup;

	/// Descs 5 : (slice 3) convert to (float)(slice 4)
	res = ConV(c, &c->desc[5], &c->slice[3], &c->slice[4], x, y);
	if (res < 0) goto cleanup;

	return 0;

cleanup:
	// TODO !
	
	if (c->desc) 
	{
		for (i = 0; i < c->numDesc; ++i)
		{
			if(c->desc[i].instance)
			free(c->desc[i].instance);
		}
		free(c->desc);
		c->desc = NULL;
	}

	if (c->slice) 
	{
		for (i = 0; i < c->numSlice ; ++i)
		{
			if(&c->slice[i])
			free_slice(&c->slice[i]);//////
		}	
		free(c->slice);
		c->slice = NULL;
	}
	return res;
}


int init_filterstoint(fs_scale_handle * c, float *x, float * y)
{
	int res = 0;
	int dst_stride = FFALIGN(c->dstW * sizeof(int16_t) + 66, 16);

	int lumBufSize = 6;
	int chrBufSize = 5;
	

	// TODO : !
	//get_min_buffer_size(c, &lumBufSize, &chrBufSize);
	//lumBufSize = FFMAX(lumBufSize, c->vLumFilterSize + 4);
	//chrBufSize = FFMAX(chrBufSize, c->vChrFilterSize + 4);

	/// 4 slices
	/// slice 0 : input(src)
	/// slice 1 : storage for chr convert output
	/// slice 2 : storage for hscale output
	/// slice 3 : storage for vscale output(dst)
	/// slice 0&3 uses input&output memory. slice 1&2 need extra memory allocated.
	c->numSlice = 4;
	int i;
	/// 5 Descs
	/// Descs 0 : (slice 0) lum part hscale to (slice 2)
	/// Descs 1 : (slice 0) convert chr part to (slice 1). (nv21 to yuv420p)
	/// Descs 2 : (slice 1) chr part hscale to (slice 2)
	/// Descs 3 : (slice 2) lum part vscale to (slice 3)
	/// Descs 4 : (slice 2) chr part vscale to (slice 3)
	c->numDesc = 5;
	c->descIndex[0] = 1;
	c->descIndex[1] = 3;

	c->desc = (SwsFilterDescriptor *)malloc(c->numDesc * sizeof(SwsFilterDescriptor));
	c->slice = (SwsSlice *)malloc(sizeof(SwsSlice) * c->numSlice);

	/// slice 0 : input(src)
	res = alloc_slice(&c->slice[0], c->srcH, c->chrSrcH, c->chrSrcHSubSample, c->chrSrcVSubSample, 0);
	if (res < 0) goto cleanup;

	/// slice 1 : storage for chr convert output
	res = alloc_slice(&c->slice[1], lumBufSize, chrBufSize, c->chrSrcHSubSample, c->chrSrcVSubSample, 0);
	if (res < 0) goto cleanup;
	res = alloc_lines(&c->slice[1], FFALIGN(c->srcW * 2 + 78, 16), c->srcW);
	if (res < 0) goto cleanup;
	
	/// slice 2 : storage for hscale output
	res = alloc_slice(&c->slice[2], lumBufSize, chrBufSize, c->chrDstHSubSample, c->chrDstVSubSample, 1);
	if (res < 0) goto cleanup;
	res = alloc_lines(&c->slice[2], dst_stride, c->dstW);
	if (res < 0) goto cleanup;

	fill_ones(&c->slice[2], dst_stride >> 1, c->dstBpc == 16);

	/// slice 3 : storage for vscale output(dst)
	res = alloc_slice(&c->slice[3], c->dstH, c->chrDstH, c->chrDstHSubSample, c->chrDstVSubSample, 0);
	if (res < 0) goto cleanup;

	///Descs 0 : (slice 0) lum part hscale to(slice 2)
	res = init_desc_hscale(&c->desc[0], &c->slice[0], &c->slice[2], (uint16_t *)c->hLumFilter, c->hLumFilterPos, c->hLumFilterSize, c->lumXInc);
	if (res < 0) goto cleanup;

	/// Descs 1 : (slice 0) convert chr part to (slice 1). (nv21 to yuv420p)
	res = init_desc_cfmt_convert(&c->desc[1], &c->slice[0], &c->slice[1], 0);
	if (res < 0) goto cleanup;

	/// Descs 2 : (slice 1) chr part hscale to (slice 2)
	res = init_desc_chscale(&c->desc[2], &c->slice[1], &c->slice[2], (uint16_t *)c->hChrFilter, c->hChrFilterPos, c->hChrFilterSize, c->chrXInc);
	if (res < 0) goto cleanup;

	/// Descs 3 : (slice 2) lum part vscale to (slice 3)
	/// Descs 4 : (slice 2) chr part vscale to (slice 3)
	res = init_vscaletoint(c, c->desc + 3, c->slice + 2, c->slice + 3);
	if (res < 0) goto cleanup;

	return 0;

cleanup:
	// TODO !
	
	if (c->desc) 
	{
		for (i = 0; i < c->numDesc; ++i)
		{
			if(c->desc[i].instance) free(c->desc[i].instance);
		}
		free(c->desc);
		c->desc = NULL;
	}

	if (c->slice) 
	{
		for (i = 0; i < c->numSlice ; ++i)
		{
			if(&c->slice[i]) free_slice(&c->slice[i]);
		}	
		free(c->slice);
		c->slice = NULL;
	}
	return res;
}



int free_filters(fs_scale_handle *c)
{
	int i;
	if (c->desc) 
	{
		for (i = 0; i < c->numDesc; ++i)
		{
			free(c->desc[i].instance);
		}
		free(c->desc);
		c->desc = NULL;
	}

	if (c->slice) 
	{
		for (i = 0; i < c->numSlice ; ++i)
		{
			free_slice(&c->slice[i]);//////
		}	
		free(c->slice);
		c->slice = NULL;
	}
	return 0;
}