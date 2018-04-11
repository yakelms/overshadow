#!/bin/bash

LOG_FILE=test_log
truncate -s 0 $LOG_FILE
echo cat $LOG_FILE
cat $LOG_FILE
echo start testing...
echo
# create a random data file with 32K bytes if theare has not one
if [ ! -e ./f32kbytes ];then
    dd bs=1024 count=32 if=/dev/urandom of=./f32kbytes
fi

if [ ! -e "./f1mbytes" ]
then
    dd bs=1024 count=1024 if=/dev/urandom of=./f1mbytes
else
    ls -alh ./f1mbytes
fi

for file in f4bytes f8bytes f16bytes f32kbytes f1mbytes
do
    rm -rf crypt_${file}
    rm -rf de_${file}

    ./overshadow -e -i ${file} &>>$LOG_FILE
    if [ -e "./crypt_${file}" ]
    then
        echo encrypt ${file} ok! >>$LOG_FILE
    else
        echo "encrypt ${file} failed!" |tee -a $LOG_FILE
        echo "==>failed!"|tee -a $LOG_FILE
        exit 0
    fi

    ./overshadow -d -i crypt_${file} -o de_${file} 2>&1 1>>$LOG_FILE
    if [ -e "./de_${file}" ]
    then
        echo decrypt crypt_${file} to de_${file} ok! >>$LOG_FILE
    else
        echo "decrypt crypt_${file} failed! "|tee -a $LOG_FILE
        echo "==>failed!"|tee -a $LOG_FILE
        exit 0
    fi

    diff ${file} de_${file}
    if [ 0 -ne $? ]
    then
        echo "test failed for ${file}!!!" |tee -a $LOG_FILE
        echo "==failed!"|tee -a $LOG_FILE
        exit 0
    fi
    echo "">>$LOG_FILE
    echo "==> test ${file} OK!"|tee -a $LOG_FILE
    echo "">>$LOG_FILE
    echo "">>$LOG_FILE
done
echo "==> test success!!! \^-^/"|tee -a $LOG_FILE
echo $count
