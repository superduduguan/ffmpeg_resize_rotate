#ifndef _SCALE_COMMON_H_
#define _SCALE_COMMON_H_

#define FFALIGN(a, b) ((a + b - 1) & -b)
#define FFMIN(a, b) (a < b ? a : b)
#define FFMAX(a, b) (a > b ? a : b)
#define FFABS(a) ((a) < 0 ? -(a) : (a))


#define ROUNDED_DIV(a,b) (((a)>0 ? (a) + ((b)>>1) : (a) - ((b)>>1))/(b))

#if defined(__GNUC__) || defined(__clang__)
#    define av_builtin_constant_p __builtin_constant_p
#    define av_printf_format(fmtpos, attrpos) __attribute__((__format__(__printf__, fmtpos, attrpos)))
#else
#    define av_builtin_constant_p(x) 0
#    define av_printf_format(fmtpos, attrpos)
#endif

#define AV_CEIL_RSHIFT(a,b) (!av_builtin_constant_p(b) ? -((-(a)) >> (b)) \
                                                       : ((a) + (1<<(b)) - 1) >> (b))

typedef unsigned char uint8_t;
typedef signed short int16_t;
typedef unsigned short uint16_t;
typedef signed int int32_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef signed long long fs_int64;

#if defined(__INTEL_COMPILER) && __INTEL_COMPILER < 1110 || defined(__SUNPRO_C)
#define DECLARE_ALIGNED(n,t,v)      t __attribute__ ((aligned (n))) v
#define DECLARE_ASM_ALIGNED(n,t,v)  t __attribute__ ((aligned (n))) v
#define DECLARE_ASM_CONST(n,t,v)    const t __attribute__ ((aligned (n))) v
#elif defined(__DJGPP__)
#define DECLARE_ALIGNED(n,t,v)      t __attribute__ ((aligned (FFMIN(n, 16)))) v
#define DECLARE_ASM_ALIGNED(n,t,v)  t av_used __attribute__ ((aligned (FFMIN(n, 16)))) v
#define DECLARE_ASM_CONST(n,t,v)    static const t av_used __attribute__ ((aligned (FFMIN(n, 16)))) v
#elif defined(__GNUC__) || defined(__clang__)
#define DECLARE_ALIGNED(n,t,v)      t __attribute__ ((aligned (n))) v
#define DECLARE_ASM_ALIGNED(n,t,v)  t av_used __attribute__ ((aligned (n))) v
#define DECLARE_ASM_CONST(n,t,v)    static const t av_used __attribute__ ((aligned (n))) v
#elif defined(_MSC_VER)
#define DECLARE_ALIGNED(n,t,v)      __declspec(align(n)) t v
#define DECLARE_ASM_ALIGNED(n,t,v)  __declspec(align(n)) t v
#define DECLARE_ASM_CONST(n,t,v)    __declspec(align(n)) static const t v
#else
#define DECLARE_ALIGNED(n,t,v)      t v
#define DECLARE_ASM_ALIGNED(n,t,v)  t v
#define DECLARE_ASM_CONST(n,t,v)    static const t v
#endif


#endif // !_SCALE_COMMON_H_