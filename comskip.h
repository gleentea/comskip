#ifndef COMSKIP
#define COMSKIP
#define COMSKIPVERSION "0.81"
#define SUBVERSION "056-TYA Edition"
#endif

#include <limits.h>
#include <sys/stat.h>

#define _UNICODE

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /** autoconf generated config.h */

#define bool            int
#define true            1
#define false           0

// define Sleep
#ifndef _WIN32
#define Sleep(x)      sleep(x)
#define _getcwd(x, y) getcwd(x, y)
#endif

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

#ifndef _WIN32
#if defined(_WIN64)
typedef __int64 LONG_PTR; 
#else
typedef long LONG_PTR;
#endif

#ifdef _WIN32
typedef struct _stati64 __stat;
#else
typedef struct stat __stat;
#endif

FILE*           myfopen(const char * f, char * m);
int             mystat(char * f, __stat * s);
int             myremove(char * f);


#ifdef _WIN32
#if defined(WIN32) && !defined(__MINGW32__) && !defined(__MINGW64__)
#define MAX_PATH _MAX_PATH
#else  // MSVC
#define MAX_PATH FILENAME_MAX
#endif // MinGW32,64
#elif  __unix__
#define MAX_PATH _POSIX_PATH_MAX
#else // Linux/BSD/MacOSX
#error "MAX_PATH is undefined"
#endif

// Define detection methods
#define BLACK_FRAME         1
#define LOGO                2
#define SCENE_CHANGE        4
#define RESOLUTION_CHANGE   8
#define CC                 16
#define AR                 32
#define SILENCE            64
#define CUTSCENE          128

#endif
