#include "convert.h"
#include<stdio.h>


int helper = 0;







typedef struct ColorContext
{
	uint32_t *pal;
} ColorContext;

static int lum_convert(fs_scale_handle *c, SwsFilterDescriptor *desc, int sliceY, int sliceH)
{
	int srcW = desc->src->width;
	ColorContext * instance = (ColorContext *)desc->instance;
	uint32_t * pal = instance->pal;
	int i;

	desc->dst->plane[0].sliceY = sliceY;
	desc->dst->plane[0].sliceH = sliceH;
	desc->dst->plane[3].sliceY = sliceY;
	desc->dst->plane[3].sliceH = sliceH;

	for (i = 0; i < sliceH; ++i) {
		int sp0 = sliceY + i - desc->src->plane[0].sliceY;
		int sp1 = ((sliceY + i) >> desc->src->v_chr_sub_sample) - desc->src->plane[1].sliceY;
		const uint8_t * src[4] = { desc->src->plane[0].line[sp0],
			desc->src->plane[1].line[sp1],
			desc->src->plane[2].line[sp1],
			desc->src->plane[3].line[sp0] };
		uint8_t * dst = desc->dst->plane[0].line[i];

	}

	return sliceH;
}

static int chr_convert(fs_scale_handle *c, SwsFilterDescriptor *desc, int sliceY, int sliceH)
{
	int srcW = AV_CEIL_RSHIFT(desc->src->width, desc->src->h_chr_sub_sample);
	ColorContext * instance = (ColorContext *)desc->instance;
	//uint32_t * pal = instance->pal;

	int sp0 = (sliceY - (desc->src->plane[0].sliceY >> desc->src->v_chr_sub_sample)) << desc->src->v_chr_sub_sample;
	int sp1 = sliceY - desc->src->plane[1].sliceY;

	int i;

	desc->dst->plane[1].sliceY = sliceY;
	desc->dst->plane[1].sliceH = sliceH;
	desc->dst->plane[2].sliceY = sliceY;
	desc->dst->plane[2].sliceH = sliceH;

	for (i = 0; i < sliceH; ++i) {
		const uint8_t * src[4] = { desc->src->plane[0].line[sp0 + i],
			desc->src->plane[1].line[sp1 + i],
			desc->src->plane[2].line[sp1 + i],
			desc->src->plane[3].line[sp0 + i] };

		uint8_t * dst1 = desc->dst->plane[1].line[i];
		uint8_t * dst2 = desc->dst->plane[2].line[i];

		nv12ToUV_c(dst1, dst2, src[0], src[1], src[2], srcW, 0);

	}
	return sliceH;
}

int init_desc_fmt_convert(SwsFilterDescriptor *desc, SwsSlice * src, SwsSlice *dst, uint32_t *pal)
{
	ColorContext * li = (ColorContext *)malloc(sizeof(ColorContext));
	if (!li)
		return -1;
	li->pal = pal;
	desc->instance = li;

	desc->alpha = 0;
	desc->src = src;
	desc->dst = dst;
	desc->process = &lum_convert;

	return 0;
}

int init_desc_cfmt_convert(SwsFilterDescriptor *desc, SwsSlice * src, SwsSlice *dst, uint32_t *pal)
{
	ColorContext * li = (ColorContext *)malloc(sizeof(ColorContext));
	if (!li)
		{
			return -1;
		}
	desc->instance = li;

	desc->src = src;
	desc->dst = dst;
	desc->process = &chr_convert;

	return 0;
}

typedef struct outPut
{
	float *x;
	float *y;

} outPut;

static int DDG(fs_scale_handle *c, SwsFilterDescriptor *desc, int sliceY, int sliceH, int PPQ)
{



		int DST_W = c->dstW;
		int DST_H = c->dstH;

		int srcW = AV_CEIL_RSHIFT(desc->src->width, desc->src->h_chr_sub_sample);
		outPut * instance = (outPut *)desc->instance;
		float x = *(instance->x);
		float y = *(instance->y);



		int sp0 = (sliceY - (desc->src->plane[0].sliceY >> desc->src->v_chr_sub_sample)) << desc->src->v_chr_sub_sample;
		int sp1 = sliceY - desc->src->plane[1].sliceY;



		desc->dst->plane[1].sliceY = sliceY;
		desc->dst->plane[1].sliceH = sliceH;
		desc->dst->plane[2].sliceY = sliceY;
		desc->dst->plane[2].sliceH = sliceH;

		int BBQ;
		
		uint8_t dst1;
		uint8_t dst2;
		
		//printf("----desc->dst->plane[0].line[0]: %x\n", desc->dst->plane[0].line[0]);
		//printf("----desc->dst->plane[0]: %x\n", desc->dst->plane[0]);

		float * final = desc->dst->plane[0].line[0];
		//printf("address for final is %x\n", final);
		int Y_size = DST_H * DST_W;
		int YUV_size = DST_H * DST_W * 3 / 2;


		if(c->rotate == 0)//0
		{
			for(BBQ = 0; BBQ < DST_W; BBQ++)
			{
				dst1 = *(*(desc->src->plane[0].line + sp0/2) + BBQ);//Y
				*(final + BBQ + PPQ * DST_W) = (float)(dst1 + x)* y;

				//if(PPQ % 2 == 0)
				if(~PPQ & 0x0001)
				{
					dst2 = *(*(desc->src->plane[1].line + sp1/2) + BBQ);
					*(final + BBQ + PPQ * DST_W / 2 + DST_H * DST_W)  = (float)(dst2 + x)* y;
				}
			}
		}

		//printf("%d,  %d\n", c->rotate, c->degree);

		if(c->rotate == 1)
		{
			switch (c->degree)
			{
			case 1://90
			
				for(BBQ = 0; BBQ < DST_W; BBQ++)
					{
						//printf("o\n");
						dst1 = *(*(desc->src->plane[0].line + sp0/2) + BBQ);//新图中Y的第PPQ行第BBQ个
						*(final + (BBQ + 1) * DST_H - 1 - PPQ) = (float)(dst1 + x)* y;
						//printf("%d\n", (BBQ + 1) * DST_H - 1 - PPQ);

						
						if(~PPQ & 0x0001)
						{
							
							dst2 = *(*(desc->src->plane[1].line + sp1/2) + BBQ);//新图中UV的第PPQ/2行第BBQ个
							if(~BBQ & 0x0001)//U
							{	//printf("%d\n", Y_size);
								//printf("%d\n", Y_size + (BBQ + 1) * DST_H - 2- PPQ);
								//printf("%d, %d, %d\n", PPQ, BBQ,(BBQ + 1) * DST_H - 2- PPQ);
								//printf("%d\n",DST_H);
								//printf("PPQ:%d, BBQ:%d, old: %d, new:%d\n", PPQ, BBQ, BBQ + PPQ * DST_W / 2 + DST_H * DST_W, Y_size + (BBQ/2 + 1) * DST_H - 2- PPQ);
								*(final + Y_size + (BBQ/2 + 1) * DST_H - 2- PPQ)  = (float)(dst2 + x)* y;
							}
							if(BBQ & 0x0001)//V
							{
								//printf("q\n");
								//printf("PPQ:%d, BBQ:%d, old: %d, new:%d\n", PPQ, BBQ, BBQ + PPQ * DST_W / 2 + DST_H * DST_W, Y_size + (BBQ/2 + 1) * DST_H - 1- PPQ);
								*(final + Y_size + (BBQ/2 + 1) * DST_H - 1- PPQ)  = (float)(dst2 + x)* y;
							}
						}
					}
				break;
			

			case 2://180 ok
				{

					for(BBQ = 0; BBQ < DST_W; BBQ++)
					{
						dst1 = *(*(desc->src->plane[0].line + sp0/2) + BBQ);//新图中Y的第PPQ行第BBQ个
						*(final + Y_size - 1 - BBQ - PPQ * DST_W) = (float)(dst1 + x)* y;

						//if(PPQ % 2 == 0)
						if(~PPQ & 0x0001)
						{
							dst2 = *(*(desc->src->plane[1].line + sp1/2) + BBQ);//新图中UV的第PPQ/2行第BBQ个
							if(~BBQ & 0x0001)//u
							{
								*(final + YUV_size - 2 - BBQ - PPQ * DST_W / 2 )  = (float)(dst2 + x)* y;
							}
							if(BBQ & 0x0001)//v
							{
								*(final + YUV_size  - BBQ - PPQ * DST_W / 2 )  = (float)(dst2 + x)* y;
							}
						}
					}
					break;
				}
			case 3://270
				for(BBQ = 0; BBQ < DST_W; BBQ++)
					{
						dst1 = *(*(desc->src->plane[0].line + sp0/2) + BBQ);//新图中Y的第PPQ行第BBQ个
						*(final + (DST_W - BBQ - 1) * DST_H + PPQ) = (float)(dst1 + x)* y;//ok
						
						//if(PPQ % 2 == 0)
						if(~PPQ & 0x0001)
						{
							dst2 = *(*(desc->src->plane[1].line + sp1/2) + BBQ);//新图中UV的第PPQ/2行第BBQ个
							if(~BBQ & 0x0001)//u
							{
								//printf("PPQ:%d, BBQ:%d, old: %d, new:%d\n", PPQ, BBQ, BBQ + PPQ * DST_W / 2 + DST_H * DST_W, Y_size + (DST_W - BBQ/2 - 1) * DST_H + PPQ);
								*(final + Y_size + (DST_W / 2 - BBQ/2 - 1) * DST_H + PPQ)  = (float)(dst2 + x)* y;
							}
							if(BBQ & 0x0001)//v
							{
								//printf("PPQ:%d, BBQ:%d, old: %d, new:%d\n", PPQ, BBQ, BBQ + PPQ * DST_W / 2 + DST_H * DST_W, Y_size + (BBQ/2 + 1) * DST_H + 1 + PPQ);
								*(final + Y_size + (DST_W / 2 - BBQ/2 - 1) * DST_H + PPQ + 1)  = (float)(dst2 + x)* y;
							}
						}
					}
				break;
			}
		}
		

		


		return sliceH;
	

}


int ConV(fs_scale_handle *c, SwsFilterDescriptor *desc, SwsSlice * src, SwsSlice *dst, float *x, float *y)  //src = slice[3], dst = slice[4]
{

	int DST_W = c->dstW;
	int DST_H = c->dstH;

	outPut * li = (outPut *)malloc(sizeof(outPut));
	if (!li)
		return -1;

	li->x = x;
	li->y = y;

	desc->instance = li;
	desc->src = src;
	desc->dst = dst;
	desc->processx = &DDG;

	return 0;
}

