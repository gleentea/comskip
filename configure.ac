#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.67])
AC_INIT([comskip], [0.8.1], [http://www.kaashoek.com/comskip/])
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE([])
AC_CONFIG_SRCDIR([mpeg2dec.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O

# Checks for HostOS
case "$host" in
        *-*-mingw*|*-*-cygwin*)
                AC_DEFINE(PLATFORM_WIN32, 1, [Platform is Win32])
                ;;
        *-*-linux*)
                AC_DEFINE(PLATFORM_LINUX, 1, [Platform is Linux])
                ;;
        *-*-darwin*)
                AC_DEFINE(PLATFORM_APPLE, 1, [Platform is Apple])
                ;;
        *-*-*bsd*)
                AC_DEFINE(PLATFORM_BSD, 1, [Platform is BSD])
                ;;
        *)
                AC_MSG_WARN([*** Please add $host to configure.ac checks!])
                ;;
esac

#
# Checks for libraries.
#

# Checks for argtable2
AC_CHECK_LIB(argtable2,arg_print_option,has_argtable2=1,has_argtable2=0)
if test $has_argtable2 = 0; then
  AC_MSG_ERROR("ERROR: cannot find argtable2.")
fi

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h inttypes.h locale.h memory.h stdlib.h string.h sys/timeb.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT64_T
AC_TYPE_SIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_FSEEKO
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_STRTOD
AC_CHECK_FUNCS([gettimeofday memset sqrt strchr strerror strrchr strstr strtol])

# Set install paths.
if test "x$prefix" = "xNONE" ; then
  prefix="/usr/local"
fi

if test "x$exec_prefix" = "xNONE" ; then
  exec_prefix=$prefix
fi

# Checks for ffmpeg
if test -d "ffmpeg"; then
   ac_configure_args="$ac_configure_args --enable-gpl --enable-version3 --disable-stripping --enable-libx264"
   AC_CONFIG_SUBDIRS([ffmpeg])
else
   AC_MSG_ERROR([ffmpeg directory is not found. please git submodule init; git submodule update;])
fi

# Setting for generate Makefile location
AC_CONFIG_FILES([Makefile])

AC_OUTPUT
