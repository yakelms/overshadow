#!/bin/bash
# usage performance_test.sh [number of process] [avx]
F1G=f1G
NB_PROCESS=4
if [ ! "$1" = "" ]; then
    NB_PROCESS=$1
fi

dd bs=2M count=512 if=/dev/urandom of=./${F1G}

# AVX=""
# if [ "$2" = "avx" ]; then
#     AVX="avx=1"
# fi
if [ "${WORKER_MODE}" = "thread" ];then
    MODE="-m t"
else
    MODE="-m p"
fi

make clean
# make ${AVX}
make $2 $3
echo time ./overshadow -e -n ${NB_PROCESS} ${MODE} -i ./${F1G}
time ./overshadow -e -n ${NB_PROCESS} ${MODE} -i ./${F1G}
echo time ./overshadow -d -n ${NB_PROCESS} ${MODE} -i ./crypt_${F1G} -o ./de_${F1G}
time ./overshadow -d -n ${NB_PROCESS} ${MODE} -i ./crypt_${F1G} -o ./de_${F1G}
echo diff --speed-large-files ./${F1G} ./de_${F1G}
diff --speed-large-files ./${F1G} ./de_${F1G}
echo rm -rf ./*${F1G}
rm -rf ./*${F1G}
exit 0
