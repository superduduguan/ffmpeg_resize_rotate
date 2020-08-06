#ifndef _SCALE_H_
#define _SCALE_H_

#include "common.h"
#define FS_SCALE_VERSION_MAJOR	(0)
#define FS_SCALE_VERSION_MINOR	(1)
#define FS_SCALE_VERSION_EPOCH	(0)


typedef enum _FS_IMG_FMT_S_
{
	FS_IMG_TYPE_U8C1 = 0x0,
	FS_IMG_TYPE_S8C1 = 0x1,

	FS_IMG_TYPE_YUV420SP = 0x2, /*YUV420 SemiPlanar*/
	FS_IMG_TYPE_YUV422SP = 0x3, /*YUV422 SemiPlanar*/

	FS_IMG_TYPE_YUV420P = 0x4,  /*YUV420 Planar */
	FS_IMG_TYPE_YUV422P = 0x5,  /*YUV422 planar */

	FS_IMG_TYPE_S8C2_PACKAGE = 0x6,
	FS_IMG_TYPE_S8C2_PLANAR = 0x7,

	FS_IMG_TYPE_S16C1 = 0x8,
	FS_IMG_TYPE_U16C1 = 0x9,

	FS_IMG_TYPE_U8C3_PACKAGE = 0xa,
	FS_IMG_TYPE_U8C3_PLANAR = 0xb,

	FS_IMG_TYPE_S32C1 = 0xc,
	FS_IMG_TYPE_U32C1 = 0xd,

	FS_IMG_TYPE_S64C1 = 0xe,
	FS_IMG_TYPE_U64C1 = 0xf,

	FS_IMG_TYPE_BUTT
}FS_IMG_FMT_S;

struct SwsSlice;
struct SwsFilterDescriptor;

typedef struct _SwsPlane_
{
	int available_lines;    ///< max number of lines that can be hold by this plane
	int sliceY;             ///< index of first line
	int sliceH;             ///< number of lines
	uint8_t **line;         ///< line buffer
	uint8_t **tmp;          ///< Tmp line buffer used by mmx code
} SwsPlane;

typedef struct SwsSlice
{
	int width;              ///< Slice line width
	int h_chr_sub_sample;   ///< horizontal chroma subsampling factor
	int v_chr_sub_sample;   ///< vertical chroma subsampling factor
	int is_ring;            ///< flag to identify if this slice is a ring buffer
	int should_free_lines;  ///< flag to identify if there are dynamic allocated lines
	SwsPlane plane[4];   ///< color planes
} SwsSlice;


typedef struct _FS_SCALE_HANDLE_S_
{
	int srcW;
	int srcH;
	FS_IMG_FMT_S srcFormat;

	int dstW;
	int dstH;
	FS_IMG_FMT_S dstFormat;

	int chrSrcW;                  ///< Width  of source      chroma     planes.
	int chrSrcH;                  ///< Height of source      chroma     planes.
	int chrDstW;                  ///< Width  of destination chroma     planes.
	int chrDstH;                  ///< Height of destination chroma     planes.
	int lumXInc, chrXInc;
	int lumYInc, chrYInc;
	int dstFormatBpp;             ///< Number of bits per pixel of the destination pixel format.
	int srcFormatBpp;             ///< Number of bits per pixel of the source      pixel format.

	int chrSrcHSubSample;         ///< Binary logarithm of horizontal subsampling factor between luma/alpha and chroma planes in source      image.
	int chrSrcVSubSample;         ///< Binary logarithm of vertical   subsampling factor between luma/alpha and chroma planes in source      image.
	int chrDstHSubSample;         ///< Binary logarithm of horizontal subsampling factor between luma/alpha and chroma planes in destination image.
	int chrDstVSubSample;         ///< Binary logarithm of vertical   subsampling factor between luma/alpha and chroma planes in destination image.

	unsigned long long vRounder;
	int dstBpc, srcBpc;



	int16_t *hLumFilter;          ///< Array of horizontal filter coefficients for luma/alpha planes.
	int16_t *hChrFilter;          ///< Array of horizontal filter coefficients for chroma     planes.
	int16_t *vLumFilter;          ///< Array of vertical   filter coefficients for luma/alpha planes.
	int16_t *vChrFilter;          ///< Array of vertical   filter coefficients for chroma     planes.
	int32_t *hLumFilterPos;       ///< Array of horizontal filter starting positions for each dst[i] for luma/alpha planes.
	int32_t *hChrFilterPos;       ///< Array of horizontal filter starting positions for each dst[i] for chroma     planes.
	int32_t *vLumFilterPos;       ///< Array of vertical   filter starting positions for each dst[i] for luma/alpha planes.
	int32_t *vChrFilterPos;       ///< Array of vertical   filter starting positions for each dst[i] for chroma     planes.
	int hLumFilterSize;           ///< Horizontal filter size for luma/alpha pixels.
	int hChrFilterSize;           ///< Horizontal filter size for chroma     pixels.
	int vLumFilterSize;           ///< Vertical   filter size for luma/alpha pixels.
	int vChrFilterSize;           ///< Vertical   filter size for chroma     pixels.

								  //SwsFunc swscale;

	int lastInLumBuf;             ///< Last scaled horizontal luma/alpha line from source in the ring buffer.
	int lastInChrBuf;             ///< Last scaled horizontal chroma     line from source in the ring buffer.
	int lumBufIndex;              ///< Index in ring buffer of the last scaled horizontal luma/alpha line from source.
	int chrBufIndex;              ///< Index in ring buffer of the last scaled horizontal chroma     line from source.

	int numDesc;
	int descIndex[2];
	int numSlice;
	struct SwsSlice *slice;
	struct SwsFilterDescriptor *desc;

	int sliceDir;

	const uint8_t *chrDither8, *lumDither8;

	fs_int64	flag;	// 1 << 0 : fast flag
	int form;

	int rotate;
	int degree;

}fs_scale_handle;





/**
* Struct which holds all necessary data for processing a slice.
* A processing step can be a color conversion or horizontal/vertical scaling.
*/
typedef struct SwsFilterDescriptor
{
	SwsSlice *src;  ///< Source slice
	SwsSlice *dst;  ///< Output slice

	int alpha;      ///< Flag for processing alpha channel
	void *instance; ///< Filter instance data
	int dstH;
					/// Function for processing input slice sliceH lines starting from line sliceY
	int(*process)(struct fs_scale_handle *c, struct SwsFilterDescriptor *desc, int sliceY, int sliceH);
	int(*processx)(struct fs_scale_handle *c, struct SwsFilterDescriptor *desc, int sliceY, int sliceH, int PPQ);
} SwsFilterDescriptor;

int my_sws_scale(fs_scale_handle *c, const uint8_t *const srcSlice[],
	const int srcStride[], int srcSliceY, int srcSliceH,
	float *const dst[], const int dstStride[], int rotate, int degree);

int my_sws_scaletoint(fs_scale_handle *c, const uint8_t *const srcSlice[],
	const int srcStride[], int srcSliceY, int srcSliceH,
	uint8_t *const dst[], const int dstStride[], int rotate, int degree);

int scale(fs_scale_handle *c, const uint8_t *const srcSlice[],
	const int srcStride[], int srcSliceY, int srcSliceH,
	void *const dst[], const int dstStride[], int rotate, int degree);

//typedef int(*SwsFunc)(fs_scale_handle *context, const uint8_t *src[],
//	int srcStride[], int srcSliceY, int srcSliceH,
//	uint8_t *dst[], int dstStride[]);


fs_scale_handle * fs_getScaleHandle(
	int srcW, int srcH, FS_IMG_FMT_S srcFormat,
	int dstW, int dstH, FS_IMG_FMT_S dstFormat, fs_int64 flag, float *x, float *y, int toint);

void fs_freeScaleHandle(fs_scale_handle * p_handle);


#endif // !_SCALE_H_