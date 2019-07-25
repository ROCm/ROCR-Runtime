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
RUN_IN_DOCKER=""

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
    echo "  -d            , --docker                 Run in docker container"
    echo "  -h            , --help                   Prints this help"
    echo
    echo "Gtest arguments will be forwarded to the app"
    echo
    echo "Valid platform options: polaris10, vega10, vega20, all, and so on"
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
        carrizo ) FILTER="--gtest_filter=$CZ_TESTS_BLACKLIST" ;;
        hawaii ) FILTER="--gtest_filter=$HI_TESTS_BLACKLIST" ;;
        kaveri ) FILTER="--gtest_filter=$KV_TESTS_BLACKLIST" ;;
        tonga ) FILTER="--gtest_filter=$TONGA_TESTS_BLACKLIST" ;;
        fiji ) FILTER="--gtest_filter=$FIJI_TESTS_BLACKLIST" ;;
        polaris10 | polaris11 | polaris12 ) FILTER="--gtest_filter=$POLARIS_TESTS_BLACKLIST" ;;
        vega10 ) FILTER="--gtest_filter=$VEGA10_TESTS_BLACKLIST" ;;
        vega12 ) FILTER="--gtest_filter=$VEGA12_TESTS_BLACKLIST" ;;
        vega20 ) FILTER="--gtest_filter=$VEGA20_TESTS_BLACKLIST" ;;
        raven ) FILTER="--gtest_filter=$RAVEN_TESTS_BLACKLIST" ;;
        arcturus ) FILTER="--gtest_filter=$ARCT_TESTS_BLACKLIST" ;;
        navi10 ) FILTER="--gtest_filter=$NAVI10_TESTS_BLACKLIST" ;;
        core ) FILTER="--gtest_filter=$CORE_TESTS" ;;
        all ) FILTER="" ;;
        *) die "Unsupported platform $platform. Exiting" ;;
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


# Prints GPU Name for the given Node ID
#   param - Node ID
getNodeName() {
    local nodeId=$1; shift;
    local gpuName=$(cat $TOPOLOGY_SYSFS_DIR/$nodeId/name)
    echo "$gpuName"
}

# Run KfdTest independently. Two global variables set by command-line
# will influence the tests as indicated below
#   PLATFORM - If set all tests will run with this platform filter
#   NODE - If set tests will be run only on this NODE, else it will be
#           run on all available HSA Nodes
runKfdTest() {
    if [ "$RUN_IN_DOCKER" == "true" ]; then
        if [ `sudo systemctl is-active docker` != "active" ]; then
            echo "docker isn't active, install and setup docker first!!!!"
            exit 0
        fi
        PKG_ROOT="$(getPackageRoot)"
    fi

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

        if [ "$RUN_IN_DOCKER" == "true" ]; then
            if [ "$NODE" == "" ]; then
                DEVICE_NODE="/dev/dri"
            else
                RENDER_NODE=$(($hsaNode + 127))
                DEVICE_NODE="/dev/dri/renderD${RENDER_NODE}"
            fi

            echo "Starting testing node $hsaNode ($nodeName) in docker container"
            sudo docker run -it --name kfdtest_docker --user="jenkins" --network=host \
            --device=/dev/kfd --device=${DEVICE_NODE} --group-add video --cap-add=SYS_PTRACE \
            --security-opt seccomp=unconfined -v $PKG_ROOT:/home/jenkins/rocm \
            compute-artifactory.amd.com:5000/yuho/tianli-ubuntu1604-kfdtest:01 \
            /home/jenkins/rocm/utils/run_kfdtest.sh -n $hsaNode $gtestFilter $GTEST_ARGS
            if [ "$?" = "0" ]; then
                echo "Finished node $hsaNode ($nodeName) successfully in docker container"
            else
                echo "Testing failed for node $hsaNode ($nodeName) in docker container"
            fi
            sudo docker rm kfdtest_docker
        else
            echo ""
            echo "++++ Starting testing node $hsaNode ($nodeName) ++++"
            $GDB $KFDTEST "--node=$hsaNode" $gtestFilter $GTEST_ARGS
            echo "---- Finished testing node $hsaNode ($nodeName) ----"
        fi


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
        -d  | --docker )
            RUN_IN_DOCKER="true" ;;
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
