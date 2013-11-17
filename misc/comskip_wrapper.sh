#!/bin/bash

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
FFMPEG=ffmpeg
OPTIONS=--csvout
INIFILE=$1
TS_FILE=$2

export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:/usr/local/lib 
${COMSKIP} ${OPTIONS} --ini=${INIFILE} ${TS_FILE}

FEXT="${TS_FILE##*.}"
FILE_NAME=`basename ${TS_FILE} ${FEXT}`

if ! test -f "`pwd`/${FILE_NAME}vdr"; then
    echo ".vdr file does not exists...exit" 1>&2
    exit 1
fi

#
# split TS file and concat by ffmpeg
#
VDR_FILE="`pwd`/${FILE_NAME}vdr"
LINE_NO=1

# 'CM' begin time
BEGIN_TIME=""
# 'CM' end time
_END__TIME=""

# main knitting begin time array
BEGIN_TIME_ARRAY=()
# main knitting end time array
_END__TIME_ARRAY=()

for LINE in `cat ${VDR_FILE} | awk {'print $1'}`;
do
    if test ${LINE_NO} = 1; then
        # skip first line
	LINE_NO=`expr ${LINE_NO} + 1`
	continue
    fi

    if `expr \( ${LINE_NO} % 2 \) = 1 > /dev/null`; then
	BEGIN_TIME="0${LINE}"
	_END__TIME_ARRAY+=(${BEGIN_TIME})
    else
	_END__TIME="0${LINE}"
	BEGIN_TIME_ARRAY+=(${_END__TIME})
    fi
    LINE_NO=`expr ${LINE_NO} + 1`
done

i=0
CUT_FILE_LIST=()

for _END__TIME in ${_END__TIME_ARRAY[@]}; do

    BEGIN_TIME=${BEGIN_TIME_ARRAY[${i}]}
    let i++
    FILE_PARTS=${i}
    echo "${FFMPEG} -y -i ${TS_FILE} -c copy -ss ${_END__TIME} -t ${BEGIN_TIME} -sn `pwd`/${FILE_NAME}-${FILE_PARTS}.ts"
    CUT_FILE_LIST+=(`pwd`/${FILE_NAME}-${FILE_PARTS}.ts)

    # convert time from 1970-01-01 00:00:00(UNIX Time) 
    DATE_YMD=`date '+%Y/%m/%d'`
    TIME_BEGIN=`date -d "${DATE_YMD} ${BEGIN_TIME}" "+%s"`
    TIME__END_=`date -d "${DATE_YMD} ${_END__TIME}" "+%s"`
    DIFF_SEC=`expr ${TIME__END_} - ${TIME_BEGIN}`
    PLAY_TIME=`echo | awk -v D=${DIFF_SEC} '{printf "%02d:%02d:%02d",D/(60*60),D%(60*60)/60,D%60}'`
    # ffmpeg -i <input_data> -ss <start_sec> -t <play_time> <output_data>
    echo "${FFMPEG} -y -i ${TS_FILE} -c copy -ss ${BEGIN_TIME} -t ${PLAY_TIME} -sn `pwd`/${FILE_NAME}-${FILE_PARTS}.ts"
    ${FFMPEG} -y -i ${TS_FILE} -c copy -ss ${BEGIN_TIME} -t ${PLAY_TIME} -sn `pwd`/${FILE_NAME}-${FILE_PARTS}.ts
done

#
# concat CM cut files
#
FFMPEG_CONCAT_STR="concat:"
i=0
for FILE in ${CUT_FILE_LIST[@]}; do
    if test ${i} != 0; then
        FFMPEG_CONCAT_STR=${FFMPEG_CONCAT_STR}"|"
    fi
    FFMPEG_CONCAT_STR=${FFMPEG_CONCAT_STR}${CUT_FILE_LIST[${i}]}
    let i++
done

OUTPUT_FILE="`pwd`/CUT-${FILE_NAME}ts"
echo "${FFMPEG} -i \"${FFMPEG_CONCAT_STR}\" -c copy ${OUTPUT_FILE}"
${FFMPEG} -i "${FFMPEG_CONCAT_STR}" -c copy ${OUTPUT_FILE}