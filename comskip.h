#ifndef COMSKIP
#define COMSKIP
#define COMSKIPVERSION "0.81"
#define SUBVERSION "056"
#endif

#define _UNICODE

typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

#ifdef __GNUC__
   /* gcc */
   typedef long long __int64;
#else
   /* msvc or clang */
   typedef signed __int64 int64_t;
   typedef unsigned __int64 uint64_t;
#endif
