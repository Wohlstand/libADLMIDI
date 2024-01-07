#ifndef __CST_STDTYPE_H__
#define __CST_STDTYPE_H__


// typedefs to use MAME's (U)INTxx types (copied from MAME\src\ods\odscomm.h)
/* 8-bit values */
typedef unsigned char       UINT8;
typedef signed char         INT8;

/* 16-bit values */
typedef unsigned short      UINT16;
typedef signed short        INT16;

/* 32-bit values */
#ifndef _WINDOWS_H
typedef unsigned int        UINT32;
typedef signed int          INT32;
#endif

/* 64-bit values */
#ifndef _WINDOWS_H
#ifdef _MSC_VER
typedef signed __int64      INT64;
typedef unsigned __int64    UINT64;
#else
__extension__ typedef unsigned long long    UINT64;
__extension__ typedef signed long long      INT64;
#endif
#endif

// also, for convenience, the INLINE keyword
#ifndef INLINE
#if defined(_MSC_VER)
#define INLINE  static __inline // __forceinline
#elif defined(__GNUC__)
#define INLINE  static __inline__
#else
#define INLINE  static inline
#endif
#endif



#endif  // __CST_STDTYPE_H__
