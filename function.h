#include "Scale.h"

/**
* Write one line of horizontally scaled data to planar output
* without any additional vertical scaling (or point-scaling).
*
* @param src     scaled source data, 15 bits for 8-10-bit output,
*                19 bits for 16-bit output (in int32_t)
* @param dest    pointer to the output plane. For >8-bit
*                output, this is in uint16_t
* @param dstW    width of destination in pixels
* @param dither  ordered dither array of type int16_t and size 8
* @param offset  Dither offset
*/
typedef void(*yuv2planar1_fn)(const int16_t *src, uint8_t *dest, int dstW,
	const uint8_t *dither, int offset);

/**
* Write one line of horizontally scaled data to planar output
* with multi-point vertical scaling between input pixels.
*
* @param filter        vertical luma/alpha scaling coefficients, 12 bits [0,4096]
* @param src           scaled luma (Y) or alpha (A) source data, 15 bits for
*                      8-10-bit output, 19 bits for 16-bit output (in int32_t)
* @param filterSize    number of vertical input lines to scale
* @param dest          pointer to output plane. For >8-bit
*                      output, this is in uint16_t
* @param dstW          width of destination pixels
* @param offset        Dither offset
*/
typedef void(*yuv2planarX_fn)(const int16_t *filter, int filterSize,
	const int16_t **src, uint8_t *dest, int dstW,
	const uint8_t *dither, int offset);

/**
* Write one line of horizontally scaled chroma to interleaved output
* with multi-point vertical scaling between input pixels.
*
* @param c             SWS scaling context
* @param chrFilter     vertical chroma scaling coefficients, 12 bits [0,4096]
* @param chrUSrc       scaled chroma (U) source data, 15 bits for 8-10-bit
*                      output, 19 bits for 16-bit output (in int32_t)
* @param chrVSrc       scaled chroma (V) source data, 15 bits for 8-10-bit
*                      output, 19 bits for 16-bit output (in int32_t)
* @param chrFilterSize number of vertical chroma input lines to scale
* @param dest          pointer to the output plane. For >8-bit
*                      output, this is in uint16_t
* @param dstW          width of chroma planes
*/
typedef void(*yuv2interleavedX_fn)(fs_scale_handle *c,
	const int16_t *chrFilter,
	int chrFilterSize,
	const int16_t **chrUSrc,
	const int16_t **chrVSrc,
	uint8_t *dest, int dstW);

/**
* Write one line of horizontally scaled Y/U/V/A to packed-pixel YUV/RGB
* output without any additional vertical scaling (or point-scaling). Note
* that this function may do chroma scaling, see the "uvalpha" argument.
*
* @param c       SWS scaling context
* @param lumSrc  scaled luma (Y) source data, 15 bits for 8-10-bit output,
*                19 bits for 16-bit output (in int32_t)
* @param chrUSrc scaled chroma (U) source data, 15 bits for 8-10-bit output,
*                19 bits for 16-bit output (in int32_t)
* @param chrVSrc scaled chroma (V) source data, 15 bits for 8-10-bit output,
*                19 bits for 16-bit output (in int32_t)
* @param alpSrc  scaled alpha (A) source data, 15 bits for 8-10-bit output,
*                19 bits for 16-bit output (in int32_t)
* @param dest    pointer to the output plane. For 16-bit output, this is
*                uint16_t
* @param dstW    width of lumSrc and alpSrc in pixels, number of pixels
*                to write into dest[]
* @param uvalpha chroma scaling coefficient for the second line of chroma
*                pixels, either 2048 or 0. If 0, one chroma input is used
*                for 2 output pixels (or if the SWS_FLAG_FULL_CHR_INT flag
*                is set, it generates 1 output pixel). If 2048, two chroma
*                input pixels should be averaged for 2 output pixels (this
*                only happens if SWS_FLAG_FULL_CHR_INT is not set)
* @param y       vertical line number for this output. This does not need
*                to be used to calculate the offset in the destination,
*                but can be used to generate comfort noise using dithering
*                for some output formats.
*/
typedef void(*yuv2packed1_fn)(fs_scale_handle *c, const int16_t *lumSrc,
	const int16_t *chrUSrc[2],
	const int16_t *chrVSrc[2],
	const int16_t *alpSrc, uint8_t *dest,
	int dstW, int uvalpha, int y);
/**
* Write one line of horizontally scaled Y/U/V/A to packed-pixel YUV/RGB
* output by doing bilinear scaling between two input lines.
*
* @param c       SWS scaling context
* @param lumSrc  scaled luma (Y) source data, 15 bits for 8-10-bit output,
*                19 bits for 16-bit output (in int32_t)
* @param chrUSrc scaled chroma (U) source data, 15 bits for 8-10-bit output,
*                19 bits for 16-bit output (in int32_t)
* @param chrVSrc scaled chroma (V) source data, 15 bits for 8-10-bit output,
*                19 bits for 16-bit output (in int32_t)
* @param alpSrc  scaled alpha (A) source data, 15 bits for 8-10-bit output,
*                19 bits for 16-bit output (in int32_t)
* @param dest    pointer to the output plane. For 16-bit output, this is
*                uint16_t
* @param dstW    width of lumSrc and alpSrc in pixels, number of pixels
*                to write into dest[]
* @param yalpha  luma/alpha scaling coefficients for the second input line.
*                The first line's coefficients can be calculated by using
*                4096 - yalpha
* @param uvalpha chroma scaling coefficient for the second input line. The
*                first line's coefficients can be calculated by using
*                4096 - uvalpha
* @param y       vertical line number for this output. This does not need
*                to be used to calculate the offset in the destination,
*                but can be used to generate comfort noise using dithering
*                for some output formats.
*/
typedef void(*yuv2packed2_fn)(fs_scale_handle *c, const int16_t *lumSrc[2],
	const int16_t *chrUSrc[2],
	const int16_t *chrVSrc[2],
	const int16_t *alpSrc[2],
	uint8_t *dest,
	int dstW, int yalpha, int uvalpha, int y);
/**
* Write one line of horizontally scaled Y/U/V/A to packed-pixel YUV/RGB
* output by doing multi-point vertical scaling between input pixels.
*
* @param c             SWS scaling context
* @param lumFilter     vertical luma/alpha scaling coefficients, 12 bits [0,4096]
* @param lumSrc        scaled luma (Y) source data, 15 bits for 8-10-bit output,
*                      19 bits for 16-bit output (in int32_t)
* @param lumFilterSize number of vertical luma/alpha input lines to scale
* @param chrFilter     vertical chroma scaling coefficients, 12 bits [0,4096]
* @param chrUSrc       scaled chroma (U) source data, 15 bits for 8-10-bit output,
*                      19 bits for 16-bit output (in int32_t)
* @param chrVSrc       scaled chroma (V) source data, 15 bits for 8-10-bit output,
*                      19 bits for 16-bit output (in int32_t)
* @param chrFilterSize number of vertical chroma input lines to scale
* @param alpSrc        scaled alpha (A) source data, 15 bits for 8-10-bit output,
*                      19 bits for 16-bit output (in int32_t)
* @param dest          pointer to the output plane. For 16-bit output, this is
*                      uint16_t
* @param dstW          width of lumSrc and alpSrc in pixels, number of pixels
*                      to write into dest[]
* @param y             vertical line number for this output. This does not need
*                      to be used to calculate the offset in the destination,
*                      but can be used to generate comfort noise using dithering
*                      or some output formats.
*/
typedef void(*yuv2packedX_fn)(fs_scale_handle *c, const int16_t *lumFilter,
	const int16_t **lumSrc, int lumFilterSize,
	const int16_t *chrFilter,
	const int16_t **chrUSrc,
	const int16_t **chrVSrc, int chrFilterSize,
	const int16_t **alpSrc, uint8_t *dest,
	int dstW, int y);

/**
* Write one line of horizontally scaled Y/U/V/A to YUV/RGB
* output by doing multi-point vertical scaling between input pixels.
*
* @param c             SWS scaling context
* @param lumFilter     vertical luma/alpha scaling coefficients, 12 bits [0,4096]
* @param lumSrc        scaled luma (Y) source data, 15 bits for 8-10-bit output,
*                      19 bits for 16-bit output (in int32_t)
* @param lumFilterSize number of vertical luma/alpha input lines to scale
* @param chrFilter     vertical chroma scaling coefficients, 12 bits [0,4096]
* @param chrUSrc       scaled chroma (U) source data, 15 bits for 8-10-bit output,
*                      19 bits for 16-bit output (in int32_t)
* @param chrVSrc       scaled chroma (V) source data, 15 bits for 8-10-bit output,
*                      19 bits for 16-bit output (in int32_t)
* @param chrFilterSize number of vertical chroma input lines to scale
* @param alpSrc        scaled alpha (A) source data, 15 bits for 8-10-bit output,
*                      19 bits for 16-bit output (in int32_t)
* @param dest          pointer to the output planes. For 16-bit output, this is
*                      uint16_t
* @param dstW          width of lumSrc and alpSrc in pixels, number of pixels
*                      to write into dest[]
* @param y             vertical line number for this output. This does not need
*                      to be used to calculate the offset in the destination,
*                      but can be used to generate comfort noise using dithering
*                      or some output formats.
*/
typedef void(*yuv2anyX_fn)(fs_scale_handle *c, const int16_t *lumFilter,
	const int16_t **lumSrc, int lumFilterSize,
	const int16_t *chrFilter,
	const int16_t **chrUSrc,
	const int16_t **chrVSrc, int chrFilterSize,
	const int16_t **alpSrc, uint8_t **dest,
	int dstW, int y);



uint8_t av_clip_uint8_c(int a);


void yuv2plane1_8_c(const int16_t *src, uint8_t *dest, int dstW,
	const uint8_t *dither, int offset);

void yuv2planeX_8_c(const int16_t *filter, int filterSize,
	const int16_t **src, uint8_t *dest, int dstW,
	const uint8_t *dither, int offset);

void yuv2nv12cX_c(fs_scale_handle *c, const int16_t *chrFilter, int chrFilterSize,
	const int16_t **chrUSrc, const int16_t **chrVSrc,
	uint8_t *dest, int chrDstW);

void nvXXtoUV_c(uint8_t *dst1, uint8_t *dst2,
	const uint8_t *src, int width);

void nv12ToUV_c(uint8_t *dstU, uint8_t *dstV,
	const uint8_t *unused0, const uint8_t *src1, const uint8_t *src2,
	int width, uint32_t *unused);

//void EXTERN_ASMff_hscale_8_to_15_neon(fs_scale_handle *c, int16_t *dst, int dstW,
//	const uint8_t *src, const int16_t *filter,
//	const int32_t *filterPos, int filterSize);
