#!/bin/sh

#
# Interpret TS file by comskip, and cut CM file by ffmpeg.
#
# $1 = comskip.ini
# $2 = ts file
#

if test $# -ne 2; then
    echo "usage: comskip_wrapper.sh [comskip.ini] [TS file]" 1>&2
    exit 1
fi

if ! test -f $1; then
    echo ".ini file $1 does not exists."
    exit 1
fi

if ! test -f $2; then
    echo "TS file $2 does not exists."
    exit 1
fi

COMSKIP=/usr/local/bin/comskip
OPTIONS=--csvout
INIFILE=$1
TS_FILE=$2

exec LD_LIBRARY_PATH=/usr/local/lib ${COMSKIP} ${OPTIONS} --ini=${INIFILE} ${TS_FILE}

if test $? eq 1; then
    echo "comskip failed CM detect...exit" 1>&2
    exit 1
fi

FILE_NAME=`basename ${TS_FILE}`

if ! test -f `pwd`/${FILE_NAME}.vdr; then
    echo ".vdr file does not exists...exit" 1>&2
    exit 1
fi

VDR_FILE=`pwd`/${FILE_NAME}.vdr



# while read [変数名]; do [処理...] ; done < [入力ファイル]
# ffmpeg -y -i ./3027-12-20130924-0135.m2t -c copy -ss 00:01:02.56 -t 00:00:36.72 -sn ./3027-12-20130924-0135_0001.ts