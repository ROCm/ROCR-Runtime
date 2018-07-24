1. Note on building kfdtest

To build this kfdtest application, the following libraries should be already
installed on the building machine:
libdrm libdrm_amdgpu libhsakmt

If libhsakmt is not installed, but the headers and libraries are present
locally, you can specify its directory by
export LIBHSAKMT_PATH=/*your local libhsakmt folder*/
With that, the headers and libraries are searched under
LIBHSAKMT_PATH/include and LIBHSAKMT_PATH/lib respectively.


2. How to run kfdtest

Just run "./run_kfdtest.sh" under the building output folder. You may need
to specify library path through:
export LD_LIBRARY_PATH=/*your library path containing libhsakmt*/

Note: you can use "run_kfdtest.sh -h" to see more options.
