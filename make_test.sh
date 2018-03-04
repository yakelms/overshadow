#!/bin/bash

LOG_FILE=test_log
for file in f4bytes f8bytes f16bytes
do
    rm -rf crypt_${file}
    rm -rf de_${file}

    ./overshadow -e ${file} &>>$LOG_FILE
    if [ -f "./crypt_${file}" ]
    then
        echo encrypt ${file} ok! >>$LOG_FILE
    else
        echo encrypt ${file} failed! >>$LOG_FILE
        echo "==>failed!"|tee -a $LOG_FILE
        exit 0
    fi

    ./overshadow -d crypt_${file} de_${file} 2>&1 1>>$LOG_FILE
    if [ -f "./de_${file}" ]
    then
        echo decrypt crypt_${file} to de_${file} ok! >>$LOG_FILE
    else
        echo decrypt crypt_${file} failed! >>$LOG_FILE
        echo "==>failed!"|tee -a $LOG_FILE
        exit 0
    fi

    diff ${file} de_${file}
    if [ 0 -ne $? ]
    then
        echo test failed for ${file}!!! >>$LOG_FILE
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
