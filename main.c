#include "Scale.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

#define SRC_W	1920
#define SRC_H	1080

#define DST_W	920
#define DST_H	180

#define add	0.0
#define mul	1.0



const char * f_in = "/mnt/data/GuanZH/resize/0.nv21";

typedef struct __tag_fs_rect
{
	int left;
	int top;
	int right;
	int bottom;
}FS_Rect;


void main()
{
	
	uint8_t * src_data[3];
	uint8_t * dst_data[3];
	int src_stride[3], dst_stride[3];

	src_data[0] = (uint8_t *)malloc(SRC_W * SRC_H * 3 / 2);
	src_data[2] = src_data[1] = src_data[0] + SRC_W * SRC_H;
	src_stride[0] = SRC_W;
	src_stride[1] = SRC_W;
	src_stride[2] = SRC_W;

	dst_stride[0] = DST_W;
	dst_stride[1] = DST_W;
	dst_stride[2] = DST_W;

	FILE * fin = fopen(f_in, "rb");
	fread(src_data[0], 1, SRC_W * SRC_H * 3 / 2, fin);
	fclose(fin);

	float a = add;
	float b = mul;

	//最后三个参数设定输出格式:a、b表示加数和乘数， 1/0表示输出为int/float格式 
	fs_scale_handle * handle2 = fs_getScaleHandle(SRC_W, SRC_H, (FS_IMG_FMT_S)FS_IMG_TYPE_YUV420SP, DST_W, DST_H, (FS_IMG_FMT_S)FS_IMG_TYPE_YUV420SP, 1, &a, &b, 1);
	fs_scale_handle * handle1 = fs_getScaleHandle(SRC_W, SRC_H, (FS_IMG_FMT_S)FS_IMG_TYPE_YUV420SP, DST_W, DST_H, (FS_IMG_FMT_S)FS_IMG_TYPE_YUV420SP, 1, &a, &b, 0);

	uint8_t * dst_data2[3];
	dst_data2[0] = (uint8_t *)malloc(DST_W * DST_H * sizeof(uint8_t)* 3 / 2);
	dst_data2[2] = dst_data2[1] = dst_data2[0] + DST_W * DST_H;

	//最后一个参数设定旋转情况: 0->0°, 1->90°, 2->180°, 3->270°
	scale(handle2, src_data, src_stride, 0, SRC_H, dst_data2, dst_stride, 0);
	
	float * dst_data1[3];
	dst_data1[0] = (float *)malloc(DST_W * DST_H * sizeof(float)* 3 / 2);
	dst_data1[2] = dst_data1[1] = dst_data1[0] + DST_W * DST_H;
	
	clock_t start,finish; //定义开始，结束变量
	start = clock();//初始化
	int P = 0;
	for(; P<1;P++)scale(handle1, src_data, src_stride, 0, SRC_H, dst_data1, dst_stride, 0);
	finish = clock();//初始化结束时间
	double duration = (double)(finish - start) / CLOCKS_PER_SEC;//转换浮点型
	printf( "%lf seconds\n", duration );
	
	//printf("%d\n", dst_data2[0][0]);
	//printf("%d\n", dst_data2[0][DST_H*DST_W*3/2-1]);

	int check = 0;
	int QW = 0;
	for(;check<DST_H*DST_W*3/2-1;check++)
	{	
		float a = dst_data1[0][check];
		uint8_t b =  dst_data2[0][check];
		if(a != b) 
		{
			QW ++;
			printf("%d<->%f\n", dst_data2[0][check], dst_data1[0][check]);
		}
	}
	printf("Wrong num: %d\n", QW);
	printf("tOTAL num: %d\n", DST_H * DST_W * 3 / 2);


	fs_freeScaleHandle(handle1);
	fs_freeScaleHandle(handle2);
	free(dst_data1[0]);
	free(src_data[0]);
	free(dst_data2[0]);
	free(handle1);
	free(handle2);
	printf("finish\n");
	
	return 0;
}

