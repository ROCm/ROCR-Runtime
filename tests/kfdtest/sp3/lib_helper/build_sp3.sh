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

#!/bin/bash

if [ "$KFDTEST_ROOT" == "" ] || [ "$P4_ROOT" == "" ]; then
	echo "Environment variables should be set before running this script"
	exit 1
fi

cd $KFDTEST_ROOT/sp3/lib_helper

SP3_PROJECT=$P4_ROOT/driver/drivers/sc/Chip/
LIB_OUTPUT=$KFDTEST_ROOT/sp3/

cp CMakeLists_sp3.txt $SP3_PROJECT/CMakeLists.txt

mkdir -p build
echo "Building SP3 lib"
pushd build
cmake $SP3_PROJECT/
make
popd

rsync --progress -a build/libamdsp3.a $LIB_OUTPUT
# Put the intermediate header files in the current folder for further processing
rsync --progress -a $SP3_PROJECT/sp3/sp3.h .

# Remove the build folder and CMakeLists.txt put into SP source folder
rm -r build
rm $SP3_PROJECT/CMakeLists.txt

# Replace the license statement in the header files
{ cat AMD_opensource_license.txt; sed -e '1,/#ifndef/ { /#ifndef/b; d }' sp3.h; } > $LIB_OUTPUT/sp3.h

# Delete the intermediate header files
rm sp3.h
