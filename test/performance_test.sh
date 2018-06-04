#!/bin/bash
# usage performance_test.sh [number of process] [avx]
F1G=f1G
NB_PROCESS=2
if [ ! "$1" = "" ]; then
    NB_PROCESS=$1
fi

dd bs=2M count=512 if=/dev/urandom of=./${F1G}

AVX=""
if [ "$2" = "avx" ]; then
    AVX="avx=1"
fi
make clean
make ${AVX}
echo time ./overshadow -e -p ${NB_PROCESS} -i ./${F1G}
time ./overshadow -e -p ${NB_PROCESS} -i ./${F1G}
echo time ./overshadow -d -p ${NB_PROCESS} -i ./crypt_${F1G} -o ./de_${F1G}
time ./overshadow -d -p ${NB_PROCESS} -i ./crypt_${F1G} -o ./de_${F1G}
diff --speed-large-files ./${F1G} ./de_${F1G}
rm -rf ./${F1G}
exit 0
