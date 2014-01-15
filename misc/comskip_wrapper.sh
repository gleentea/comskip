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

if ! test -f "$1"; then
    echo ".ini file $1 does not exists."
    exit 1
fi

if ! test -f "$2"; then
    echo "TS file $2 does not exists."
    exit 1
fi

COMSKIP=/usr/local/bin/comskip
FFMPEG=ffmpeg
OPTIONS=--csvout
INIFILE="$1"
TS_FILE="$2"

export LD_LIBRARY_PATH=${LD_LIBRARY_PATH}:/usr/local/lib 
${COMSKIP} ${OPTIONS} --ini="${INIFILE}" "${TS_FILE}"

FEXT="${TS_FILE##*.}"
FILE_NAME="$(basename "${TS_FILE}" "${FEXT}")"

if ! test -f "`pwd`/${FILE_NAME}vdr"; then
    echo ".vdr file does not exists...exit" 1>&2
    exit 1
fi

#
# split TS file and concat by ffmpeg
#
VDR_FILE="`pwd`/${FILE_NAME}vdr"
LINE_NO=1

# 'CM' begin centisec time
BEGIN_CSEC=""
# 'CM' end centisec time
_END__CSEC=""

# main knitting begin centisec time array
BEGIN_CSEC_ARRAY=()
# main knitting end centisec time array
_END__CSEC_ARRAY=()

_IFS="${IFS}"
IFS=":. "
exec 9<"${VDR_FILE}"
# <hour>:<min>:<sec>.<centisec> (start|end)
while read -u9 hour min sec centisec ignore;
do
    if test ${LINE_NO} = 1; then
        # skip first line
	LINE_NO=`expr ${LINE_NO} + 1`
	continue
    fi

    # calculate centisecond time
    _CSEC=`expr \( \( ${hour} \* 60 + ${min} \) \* 60 + ${sec} \) \* 100 + ${centisec}`
    #echo "DEBUG: $hour:$min:$sec.$centisec -> ${_CSEC}"

    if `expr \( ${LINE_NO} % 2 \) = 1 > /dev/null`; then
	_END__CSEC_ARRAY+=(${_CSEC})
    else
	BEGIN_CSEC_ARRAY+=(${_CSEC})
    fi
    LINE_NO=`expr ${LINE_NO} + 1`
done
IFS="${_IFS}"
exec 9<&-

if test ${#_END__CSEC_ARRAY[@]} -eq 0; then
    echo "It seems no commercials is found."
    exit 2
fi

i=0
CUT_FILE_LIST=()

for _END__CSEC in ${_END__CSEC_ARRAY[@]}; do
    # _END__CSEC and BEGIN_CSEC is centisec!!
    BEGIN_CSEC=${BEGIN_CSEC_ARRAY[${i}]}
    # calculate original time for beginning
    BEGIN_SEC=`expr ${BEGIN_CSEC} / 100`
    BEGIN_CENTISEC=`expr ${BEGIN_CSEC} % 100`
    BEGIN_TIME="${BEGIN_SEC}.${BEGIN_CENTISEC}"

    let i++
    FILE_PARTS=${i}
    CUT_FILE_LIST+=(`pwd`/"${FILE_NAME}-${FILE_PARTS}.ts")

    DIFF_TIME=`expr ${_END__CSEC} - ${BEGIN_CSEC}`
    DIFF_SEC=`expr ${DIFF_TIME} / 100`
    DIFF_CENTISEC=`expr ${DIFF_TIME} % 100`
    PLAY_TIME="${DIFF_SEC}.${DIFF_CENTISEC}"
    #echo "DEBUG: $_END__CSEC - $BEGIN_CSEC = ${DIFF_SEC}.${DIFF_CENTISEC}"
    #mkfifo "$(pwd)/${FILE_NAME}-${FILE_PARTS}.ts"
    # ffmpeg -i <input_data> -ss <start_sec> -t <play_time> <output_data>
    echo "${FFMPEG} -y -i ${TS_FILE} -c copy -ss ${BEGIN_TIME} -t ${PLAY_TIME} -sn `pwd`/${FILE_NAME}-${FILE_PARTS}.ts"
    # if you use with mkfifo, run FFMPEG command IN BACKGROUND (just append &)
    ${FFMPEG} -y -i "${TS_FILE}" -c copy -ss ${BEGIN_TIME} -t ${PLAY_TIME} -sn `pwd`/"${FILE_NAME}-${FILE_PARTS}.ts"
done

#
# concat CM cut files
#
FFMPEG_CONCAT_STR="concat:"
i=0
for FILE in ${CUT_FILE_LIST[@]}; do
    if test ${i} != 0; then
        FFMPEG_CONCAT_STR="${FFMPEG_CONCAT_STR}|"
    fi
    FFMPEG_CONCAT_STR="${FFMPEG_CONCAT_STR}${CUT_FILE_LIST[${i}]}"
    let i++
done

OUTPUT_FILE="`pwd`/CUT-${FILE_NAME}ts"
echo "${FFMPEG} -i \"${FFMPEG_CONCAT_STR}\" -c copy ${OUTPUT_FILE}"
${FFMPEG} -i "${FFMPEG_CONCAT_STR}" -c copy "${OUTPUT_FILE}"
