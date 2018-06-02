#!/bin/bash
BASEDIR=$(cd `dirname $0`; pwd)
echo basedir ${BASEDIR}

BIN=overshadow
if [ ! "$1" == "" ]; then
    BIN=$1
fi

LOG_FILE=test.log
if [ ! "$2" = "" ]; then
    LOG_FILE=$2
fi

truncate -s 0 ${BASEDIR}/${LOG_FILE}
echo start testing...
echo
# create a random data file with 32K bytes if theare has not one
if [ ! -e "${BASEDIR}/f32kbytes" ];then
    dd bs=1024 count=32 if=/dev/urandom of="${BASEDIR}/f32kbytes"
fi

if [ ! -e "${BASEDIR}/f1mbytes" ]
then
    dd bs=1024 count=1024 if=/dev/urandom of="${BASEDIR}/f1mbytes"
fi

for file in f4bytes f8bytes f16bytes f32kbytes f1mbytes
do
    rm -rf crypt_${file}
    rm -rf de_${file}

    ${BASEDIR}/${BIN} -e -i ${BASEDIR}/${file} &>>${BASEDIR}/${LOG_FILE}
    if [ -e "${BASEDIR}/crypt_${file}" ]
    then
        echo encrypt ${file} ok! >>${BASEDIR}/${LOG_FILE}
    else
        echo "encrypt ${file} failed!" |tee -a ${BASEDIR}/${LOG_FILE}
        echo "==>failed!"|tee -a ${BASEDIR}/${LOG_FILE}
        exit 0
    fi

    ${BASEDIR}/${BIN} -d -i ${BASEDIR}/crypt_${file} -o ${BASEDIR}/de_${file} 2>&1 1>>${BASEDIR}/${LOG_FILE}
    if [ -e "${BASEDIR}/de_${file}" ]
    then
        echo decrypt crypt_${file} to de_${file} ok! >>${BASEDIR}/${LOG_FILE}
    else
        echo "decrypt crypt_${file} failed! "|tee -a ${BASEDIR}/${LOG_FILE}
        echo "==>failed!"|tee -a ${BASEDIR}/${LOG_FILE}
        exit 0
    fi

    diff ${BASEDIR}/${file} ${BASEDIR}/de_${file}
    if [ 0 -ne $? ]
    then
        echo "test failed for ${file}!!!" |tee -a ${BASEDIR}/${LOG_FILE}
        echo "==failed!"|tee -a ${BASEDIR}/${LOG_FILE}
        exit 0
    fi
    echo "">>${BASEDIR}/${LOG_FILE}
    echo "==> test ${file} OK!"|tee -a ${BASEDIR}/${LOG_FILE}
    echo "">>${BASEDIR}/${LOG_FILE}
    echo "">>${BASEDIR}/${LOG_FILE}
done
echo "==> test success!!! \^-^/"|tee -a ${BASEDIR}/${LOG_FILE}
echo $count

# clean
rm -rf ${BASEDIR}/crypt_* ${BASEDIR}/de_*
