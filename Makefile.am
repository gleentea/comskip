# ChangeLog
dist-hook: changelog

# configure.ac
changelog: configure.ac
	git log --stat --name-only --date=short --abbrev-commit > ChangeLog

# install directory
bin_PROGRAMS           = comskip

# sources
comskip_SOURCES        = comskip.c mpeg2dec.c video_out_dx.c comskip.h

# common build rule
comskip_CFLAGS         = @CFLAGS@ -Wno-deprecated-declarations -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64
comskip_LDADD          = -lavformat -lavcodec -lavutil
ACLOCAL_AMFLAGS        = -I m4
