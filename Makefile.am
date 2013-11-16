# ChangeLog
dist-hook: changelog

# configure.ac
changelog: configure.ac
        git log --stat --name-only --date=short --abbrev-commit > ChangeLog

# install directory
comskipexecdir         = $(datarootdir)/@PACKAGE@
comskipexec_PROGRAMS   = comskip
comskipdir             = $(pkglibdir)

# sources
comskip_SOURCES        = comskip.c mpeg2dec.c video_out_dx.c comskip.h

# common build rule
comskip_CFLAGS         = @CFLAGS@ -Wno-deprecated-declarations
comskip_LDFLAGS        = @LDFLAGS@ -lavformat -lavcodec -lavutil
ACLOCAL_AMFLAGS        = -I m4
