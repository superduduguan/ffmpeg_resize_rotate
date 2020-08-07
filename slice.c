#include "slice.h"
#include <stdio.h>
#include <stdlib.h>

void free_lines(SwsSlice *s)
{
	int i;
	for (i = 0; i < 2; ++i) {
		int n = s->plane[i].available_lines;
		int j;
		for (j = 0; j < n; ++j) {
			free(s->plane[i].line[j]);
			if (s->is_ring)
				s->plane[i].line[j + n] = NULL;
		}
	}

	for (i = 0; i < 4; ++i)
		memset(s->plane[i].line, 0, sizeof(uint8_t*) * s->plane[i].available_lines * (s->is_ring ? 3 : 1));
	s->should_free_lines = 0;
}

int alloc_lines(SwsSlice *s, int size, int width)
{
	int i;
	int idx[2] = { 3, 2 };

	s->should_free_lines = 1;
	s->width = width;

	for (i = 0; i < 2; ++i) 
	{
		int n = s->plane[i].available_lines;
		int j;
		int ii = idx[i];

		for (j = 0; j < n; ++j) 
		{
			// chroma plane line U and V are expected to be contiguous in memory
			// by mmx vertical scaler code
			s->plane[i].line[j] = (uint8_t *)malloc(size * 2 + 32);//plane[0] or plane[1] 每一行malloc
			s->plane[ii].line[j] = s->plane[i].line[j] + size + 16;//plane[3] or plane[2] follows
			if (s->is_ring) 
			{
				s->plane[i].line[j + n] = s->plane[i].line[j];
				s->plane[ii].line[j + n] = s->plane[ii].line[j];
			}
		}
	}

	return 0;
}

void free_slice(SwsSlice *s)
{
	int i;
	if (s) 
	{
		if (s->should_free_lines)
		
			free_lines(s);
		for (i = 0; i < 4; ++i) 
		{
			free(s->plane[i].line);
		
			s->plane[i].tmp = NULL;
		}
	}
}

int alloc_slice(SwsSlice *s, int lumLines, int chrLines, int h_sub_sample, int v_sub_sample, int ring)
{
	int i;
	int size[4] = { lumLines,
		chrLines,
		chrLines,
		lumLines };

	s->h_chr_sub_sample = h_sub_sample;
	s->v_chr_sub_sample = v_sub_sample;
	s->is_ring = ring;
	s->should_free_lines = 0;

	for (i = 0; i < 4; ++i)
	{
		int n = size[i] * (ring == 0 ? 1 : 3);
		//printf("n = %d", n);
		s->plane[i].line = (uint8_t **)malloc(sizeof(uint8_t*) * n);//有n行
		s->plane[i].tmp = ring ? s->plane[i].line + size[i] * 2 : NULL;
		s->plane[i].available_lines = size[i];
		//printf("__%d__", size[i]);
		s->plane[i].sliceY = 0;
		s->plane[i].sliceH = 0;
	}
	//printf("\n");
	return 0;
}

int alloc_slicex(SwsSlice *s, int lumLines, int chrLines, int h_sub_sample, int v_sub_sample, int ring)
{
	int i;
	int size[4] = { lumLines,
		chrLines,
		chrLines,
		lumLines };

	s->h_chr_sub_sample = h_sub_sample;
	s->v_chr_sub_sample = v_sub_sample;
	s->is_ring = ring;
	s->should_free_lines = 0;

	for (i = 0; i < 4; ++i)
	{
		int n = size[i] * (ring == 0 ? 1 : 3);
		
		s->plane[i].line = (float **)malloc(sizeof(float*) * n);//有n行
		s->plane[i].tmp = ring ? s->plane[i].line + size[i] * 2 : NULL;
		s->plane[i].available_lines = size[i];
		s->plane[i].sliceY = 0;
		s->plane[i].sliceH = 0;
	}
	//printf("\n");
	return 0;
}

int init_slice_from_src(SwsSlice * s, uint8_t *src[4], int stride[4], int srcW, int lumY, int lumH, int chrY, int chrH, int relative)
{
	int i = 0;

	const int start[4] = { lumY,
		chrY,
		chrY,
		lumY };

	const int end[4] = { lumY + lumH,
		chrY + chrH,
		chrY + chrH,
		lumY + lumH };

	uint8_t *const src_[4] = { src[0] + (relative ? 0 : start[0]) * stride[0],
		src[1] + (relative ? 0 : start[1]) * stride[1],
		src[2] + (relative ? 0 : start[2]) * stride[2],
		src[3] + (relative ? 0 : start[3]) * stride[3] };

	s->width = srcW;

	for (i = 0; i < 4; ++i)
	{
		int j;
		int first = s->plane[i].sliceY;
		int n = s->plane[i].available_lines;
		int lines = end[i] - start[i];
		int tot_lines = end[i] - first;

		if (start[i] >= first && n >= tot_lines)
		{
			s->plane[i].sliceH = FFMAX(tot_lines, s->plane[i].sliceH);
			for (j = 0; j < lines; j += 1)
				s->plane[i].line[start[i] - first + j] = src_[i] + j * stride[i];
		}
		else
		{
			s->plane[i].sliceY = start[i];
			lines = lines > n ? n : lines;
			s->plane[i].sliceH = lines;
			for (j = 0; j < lines; j += 1)
				s->plane[i].line[j] = src_[i] + j * stride[i];//dst[i]存着slice第i平面的起始地址；dst[i]+ j * stride[i]存着slice的第i平面第j行的起始地址（输出只需要dst[0]就行了）
		}

	}
	//free(src);
	return 0;
}


int init_slice_from_dst(SwsSlice * s, float *src[4], int stride[4], int srcW, int lumY, int lumH, int chrY, int chrH, int relative)
{
	int i = 0;
	
	const int start[4] = { lumY,
		chrY,
		chrY,
		lumY };

	const int end[4] = { lumY + lumH,
		chrY + chrH,
		chrY + chrH,
		lumY + lumH };
	
	float *const src_[4] = { src[0] + (relative ? 0 : start[0]) * stride[0],
		src[1] + (relative ? 0 : start[1]) * stride[1],
		src[2] + (relative ? 0 : start[2]) * stride[2],
		src[3] + (relative ? 0 : start[3]) * stride[3] };

	s->width = srcW;

	for (i = 0; i < 4; ++i)
	{
		int j;
		int first = s->plane[i].sliceY;
		int n = s->plane[i].available_lines;
		int lines = end[i] - start[i];
		int tot_lines = end[i] - first;
		
		if (start[i] >= first && n >= tot_lines)
		{
			
			s->plane[i].sliceH = FFMAX(tot_lines, s->plane[i].sliceH);
			for (j = 0; j < lines; j += 1)
				s->plane[i].line[start[i] - first + j] = src_[i] + j * stride[i];
				
		}
		
		else
		{
			s->plane[i].sliceY = start[i];
			lines = lines > n ? n : lines;
			s->plane[i].sliceH = lines;
			for (j = 0; j < lines; j += 1)
				s->plane[i].line[j] = src_[i] + j * stride[i];//dst[i]存着slice第i平面的起始地址；用该起始地址表示了slice的每一line（每一平面所有line里的第一个的地址才是dst[i]的地址）
				
		}
		
	}

	return 0;
}


// int init_slice_from_dst(SwsSlice * s, float *src[4], int stride[4], int srcW, int lumY, int lumH, int chrY, int chrH, int relative)
// {
// 	int i = 0;

// 	const int start[4] = { lumY,
// 		chrY,
// 		chrY,
// 		lumY };

// 	const int end[4] = { lumY + lumH,
// 		chrY + chrH,
// 		chrY + chrH,
// 		lumY + lumH };

// 	float *const src_[4] = { src[0] + (relative ? 0 : start[0]) * stride[0],
// 		src[1] + (relative ? 0 : start[1]) * stride[1],
// 		src[2] + (relative ? 0 : start[2]) * stride[2],
// 		src[3] + (relative ? 0 : start[3]) * stride[3] };

// 	s->width = srcW;

// 	for (i = 0; i < 4; ++i)
// 	{
// 		int j;
// 		int first = s->plane[i].sliceY;
// 		int n = s->plane[i].available_lines;
// 		int lines = end[i] - start[i];
// 		int tot_lines = end[i] - first;

// 		if (start[i] >= first && n >= tot_lines)
// 		{
// 			s->plane[i].sliceH = FFMAX(tot_lines, s->plane[i].sliceH);
// 			for (j = 0; j < lines; j += 1)
// 				s->plane[i].line[start[i] - first + j] = src_[i] + j * stride[i];
// 		}
// 		else
// 		{
// 			s->plane[i].sliceY = start[i];
// 			lines = lines > n ? n : lines;
// 			s->plane[i].sliceH = lines;
// 			for (j = 0; j < lines; j += 1)
// 				s->plane[i].line[j] = src_[i] + j * stride[i];//dst[i]存着slice第i平面的起始地址；dst[i]+ j * stride[i]存着slice的第i平面第j行的起始地址（输出只需要dst[0]就行了）
// 		}

// 	}
// 	//free(src);
// 	return 0;
// }

int rotate_slice(SwsSlice *s, int lum, int chr)
{
	int i;
	if (lum) {
		for (i = 0; i < 4; i += 3) {
			int n = s->plane[i].available_lines;
			
			int l = lum - s->plane[i].sliceY;

			if (l >= n * 2) {
				s->plane[i].sliceY += n;
				s->plane[i].sliceH -= n;
			}
		}
	}
	if (chr) {
		for (i = 1; i < 3; ++i) {
			int n = s->plane[i].available_lines;
			int l = chr - s->plane[i].sliceY;

			if (l >= n * 2) {
				s->plane[i].sliceY += n;
				s->plane[i].sliceH -= n;
			}
		}
	}
	return 0;
}
