#include "function.h"

uint8_t av_clip_uint8_c(int a)
{
	if (a&(~0xFF)) return (~a) >> 31;
	else           return a;
}

void yuv2plane1_8_c(const int16_t * src, uint8_t * dest, int dstW, const uint8_t * dither, int offset)
{
	int i;
	for (i = 0; i<dstW; i++) {
		int val = (src[i] + dither[(i + offset) & 7]) >> 7;
		dest[i] = av_clip_uint8_c(val);
	}
}

void yuv2planeX_8_c(const int16_t * filter, int filterSize, const int16_t ** src, uint8_t * dest, int dstW, const uint8_t * dither, int offset)
{
	int i;
	for (i = 0; i<dstW; i++) {
		int val = 0;//dither[(i + offset) & 7] << 12;
		int j;
		for (j = 0; j<filterSize; j++)
		
			val += src[j][i] * filter[j];

		dest[i] = av_clip_uint8_c(val >> 19);
	}
}

void yuv2nv12cX_c(fs_scale_handle * c, const int16_t * chrFilter, int chrFilterSize, const int16_t ** chrUSrc, const int16_t ** chrVSrc, uint8_t * dest, int chrDstW)
{
	
	const uint8_t *chrDither = c->chrDither8;
	int i;


	for (i = 0; i < chrDstW; i++)
	{
		int u = 0;
		int v = 0;
		int j;

		u += chrUSrc[0][i] * (0x1000);
		v += chrVSrc[0][i] * (0x1000);

		dest[2 * i + 1] = av_clip_uint8_c(v >> 19);
		dest[2 * i] = av_clip_uint8_c(u >> 19);
	}
}

void nvXXtoUV_c(uint8_t * dst1, uint8_t * dst2, const uint8_t * src, int width)
{
	int i;
	for (i = 0; i < width; i++) {
		dst1[i] = src[2 * i + 0];
		dst2[i] = src[2 * i + 1];
	}
}

void nv12ToUV_c(uint8_t * dstU, uint8_t * dstV, const uint8_t * unused0, const uint8_t * src1, const uint8_t * src2, int width, uint32_t * unused)
{
	nvXXtoUV_c(dstU, dstV, src1, width);
}

void ch2floatnow(float *x, float *y, uint8_t * dst1, uint8_t * dst2, const uint8_t * src, int width)
{
	float *tmp_dst1;
	float *tmp_dst2;
	int i;
	for (i = 0; i < width; i++) {
		dst1[i] = src[2 * i + 0];
		tmp_dst1[i] = (*y) * ((*x) + dst1[i]) ;
		dst1[i] = (float)((*x) + dst1[i]) * (*y);

		dst2[i] = src[2 * i + 1];
		tmp_dst2[i] = ((*x) + dst2[i])*(*y);
		dst2[i] = (float)((*x) + dst2[i]) * (*y);
	}
}

void ch2float(float *x, float *y, uint8_t *dstU, uint8_t *dstV, const uint8_t *unused0, const uint8_t *src1, const uint8_t *src2, int width, uint32_t *unused)
{
	ch2floatnow(x, y, dstU, dstV, src1, width);
}