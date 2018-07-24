#!/bin/bash
#
# Copyright (C) 2018 Advanced Micro Devices, Inc. All Rights Reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
# OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
# ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
# OTHER DEALINGS IN THE SOFTWARE.
#
#


if [ "$BIN_DIR" == "" ]; then
    BIN_DIR="$(pwd)/$(dirname $0)"
fi

PLATFORM=""
GDB=""
NODE=""
MULTI_GPU=""
FORCE_HIGH=""

printUsage() {
    echo
    echo "Usage: $(basename $0) [options ...] [gtest arguments]"
    echo
    echo "Options:"
    echo "  -p <platform> , --platform <platform>    Only run tests that"\
                                           "pass on the specified platform"
    echo "  -g            , --gdb                    Run in debugger"
    echo "  -n            , --node                   NodeId to test. If"\
                               "not specified test will be run on all nodes"
    echo "  -l            , --list                   List available nodes"
    echo "  --high                                   Force clocks to high for test execution"
    echo "  -h            , --help                   Prints this help"
    echo
    echo "Gtest arguments will be forwarded to the app"
    echo
    echo "Valid platform options: cz, kv, tg, fj, hi, pl/el, plb/bf, vg10, all"
    echo "'all' option runs all tests"

    return 0
}
# Print gtest_filter for the given Platform
# If MULTI_GPU flag is set, then Multi-GPU Tests are selected. Once all tests
# pass in Multi GPU environment, this flag can be removed
#    param - Platform.
getFilter() {
    local platform=$1;
    case "$platform" in
        cz ) FILTER="--gtest_filter=$CZ_TESTS_BLACKLIST" ;;
        hi ) FILTER="--gtest_filter=$HI_TESTS_BLACKLIST" ;;
        kv ) FILTER="--gtest_filter=$KV_TESTS_BLACKLIST" ;;
        tg ) FILTER="--gtest_filter=$TONGA_TESTS_BLACKLIST" ;;
        fj ) FILTER="--gtest_filter=$FIJI_TESTS_BLACKLIST" ;;
        pl | el ) FILTER="--gtest_filter=$ELLESMERE_TESTS_BLACKLIST" ;;
        plb | bf ) FILTER="--gtest_filter=$BAFFIN_TESTS_BLACKLIST" ;;
        vg10 ) FILTER="--gtest_filter=$VEGA10_TESTS_BLACKLIST" ;;
        vg20 ) FILTER="--gtest_filter=$VEGA20_TESTS_BLACKLIST" ;;
        rv ) FILTER="--gtest_filter=$RAVEN_TESTS_BLACKLIST" ;;
        all ) FILTER="" ;;
        *) die "Unsupported platform $PLATFORM or node $NODE. Exiting" ;;
    esac
    echo "$FILTER"
}

TOPOLOGY_SYSFS_DIR=/sys/devices/virtual/kfd/kfd/topology/nodes

# Prints list of HSA Nodes. HSA Nodes are identified from sysfs KFD topology. The nodes
# should have valid SIMD count
getHsaNodes() {
    for i in $(find $TOPOLOGY_SYSFS_DIR  -maxdepth 1 -mindepth 1 -type d); do
        simdcount=$(cat $i/properties | grep simd_count | awk '{print $2}')
        if [ $simdcount != 0 ]; then
            hsaNodeList+="$(basename $i) "
        fi
    done
    echo "$hsaNodeList"
}


# Prints GPU name for a given Device ID
#   param - Device ID.
deviceIdToGpuName() {
    local deviceId=$1; shift;
    case $deviceId in
        1304 | 1305 | 1306 | 1307 | 1309 | 130a | 130b | 130c | 130d | 130e | 130f | \
        1310 | 1311 | 1312 | 1313 | 1315 | 1316 | 1317 | 1318 | 131b | 131c | 131d )
            platformName="kv" ;;
        67a0 | 67a1 | 67a2 | 67a8 | 67a9 | 67aa | 67b0 | 67b1 | 67b8 | 67b9 | 67ba | 67be )
            platformName="hi" ;;
        9870 | 9874 | 9875 | 9876 | 9877 )
            platformName="cz" ;;
        6920 | 6921 | 6928 | 6929 | 692b | 692f | 6930 | 6938 | 6939 )
            platformName="tg" ;;
        7300 | 730f)
            platformName="fj" ;;
        67c0 | 67c1 | 67c2 | 67c4 | 67c7 | 67c8 | 67c9 | 67ca | 67cc | 67cf | 67d0 | 67df )
            platformName="pl" ;;
        67e0 | 67e1 | 67e3 | 67e7 | 67e8 | 67e9 | 67eb | 67ef | 67ff )
            platformName="plb" ;;
        6860 | 6861 | 6862 | 6863 | 6864 | 6867 | 6868 | 6869 | 686a | 686b | 686c | 687f)
            platformName="vg10" ;;
        66a0 | 66a1 | 66a2 | 66a3 |66a7 | 66af)
            platformName="vg20" ;;
        15dd )
            platformName="rv" ;;
        * )
            return ;;
    esac
    echo "$platformName"
}


# Prints GPU Name for the given Node ID
#   param - Node ID
getNodeName() {
    local nodeId=$1; shift;
    local gpuIdInDec=$(cat $TOPOLOGY_SYSFS_DIR/$nodeId/properties | grep device_id | awk '{print $2}')
    printf -v gpuIdInHex "%x" "$gpuIdInDec"
    local gpuName=$(deviceIdToGpuName $gpuIdInHex)
    echo "$gpuName"
}

# Run KfdTest independently. Two global variables set by command-line
# will influence the tests as indicated below
#   PLATFORM - If set all tests will run with this platform filter
#   NODE - If set tests will be run only on this NODE, else it will be
#           run on all available HSA Nodes
runKfdTest() {
    if [ "$NODE" == "" ]; then
        hsaNodes=$(getHsaNodes)
    else
        hsaNodes=$NODE
    fi

    source $BIN_DIR/kfdtest.exclude

    for hsaNode in $hsaNodes; do
        nodeName=$(getNodeName $hsaNode)
        if [ "$PLATFORM" != "" ] && [ "$PLATFORM" != "$nodeName" ]; then
            echo "WARNING: Actual ASIC $nodeName treated as $PLATFORM"
            nodeName="$PLATFORM"
        fi

        gtestFilter=$(getFilter $nodeName)

        echo ""
        echo "++++ Starting testing node $hsaNode ($nodeName) ++++"
        $GDB $KFDTEST "--node=$hsaNode" $gtestFilter $GTEST_ARGS
        echo "---- Finished testing node $hsaNode ($nodeName) ----"
    done

}

# Prints number of GPUs present in the system
getGPUCount() {
    gNodes=$(getHsaNodes)
    gNodes=( $gNodes )
    gpuCount=${#gNodes[@]}
    echo "$gpuCount"
}

# set $MULTI_GPU flag if the system has more than 1 GPU
setMultiGPUFlag() {
    gpuCount=$(getGPUCount)
    if [ $gpuCount -gt 1 ]; then
        MULTI_GPU=1
    else
        MULTI_GPU=0
    fi
}

while [ "$1" != "" ]; do
    case "$1" in
        -p  | --platform )
            shift 1; PLATFORM=$1 ;;
        -g  | --gdb )
            GDB="gdb --args" ;;
        -l  | --list )
            printGpuNodelist; exit 0 ;;
        -n  | --node )
            shift 1; NODE=$1 ;;
        --high)
            FORCE_HIGH="true" ;;
        -h  | --help )
            printUsage; exit 0 ;;
        *)
            GTEST_ARGS=$@; break;;
    esac
    shift 1
done

KFDTEST="$BIN_DIR/kfdtest"

if [ "$FORCE_HIGH" == "true" ]; then
    pushGpuDpmState high
    pushTrap "popGpuDpmState" EXIT
fi

setMultiGPUFlag
# Set HSA_DEBUG env to run KFDMemoryTest.PtraceAccessInvisibleVram
export HSA_DEBUG=1
runKfdTest
