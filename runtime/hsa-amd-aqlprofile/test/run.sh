#/bin/sh
set -x

tbin=./test/ctrl

CDIR=`pwd`
export LD_LIBRARY_PATH=$CDIR

export HSA_ENABLE_SDMA=0
export HSA_EMULATE_AQL=1

echo
echo "Run simple convolution kernel"
unset ROCR_ENABLE_PMC
unset ROCR_ENABLE_SQTT
eval $tbin

echo
echo "Run with PMC"
export ROCR_ENABLE_PMC=1
unset ROCR_ENABLE_SQTT
eval $tbin

echo
echo "Run with SQTT"
unset ROCR_ENABLE_PMC
export ROCR_ENABLE_SQTT=1
eval $tbin


