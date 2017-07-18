### ROCt Library

This repository includes the user-mode API interfaces used to interact with the ROCk driver. Currently supported agents include only the AMD/ATI Fiji family of discrete GPUs.

#### ROCk Driver

The ROCt library is not a standalone product and requires that you have the correct ROCk driver set installed. We recommend reading the full compatibility and installation details which are available in the ROCk github:

https://github.com/RadeonOpenCompute/ROCK-Radeon-Open-Compute-Kernel-Driver

#### Building the Thunk

A simple cmake-based system is available for building thunk. To build the runtime from the the ROCT-Thunk-Interface directory, execute:

```bash
    mkdir -p build
    cd build
    cmake ..
    make
```

If the hsakmt-roct and hsakmt-roct-dev packages are desired:

```bash
    mkdir -p build
    cd build
    cmake ..
    make package
    make package-dev
```

#### Disclaimer

The information contained herein is for informational purposes only, and is subject to change without notice. While every precaution has been taken in the preparation of this document, it may contain technical inaccuracies, omissions and typographical errors, and AMD is under no obligation to update or otherwise correct this information. Advanced Micro Devices, Inc. makes no representations or warranties with respect to the accuracy or completeness of the contents of this document, and assumes no liability of any kind, including the implied warranties of noninfringement, merchantability or fitness for particular purposes, with respect to the operation or use of AMD hardware, software or other products described herein. No license, including implied or arising by estoppel, to any intellectual property rights is granted by this document. Terms and limitations applicable to the purchase or use of AMD's products are as set forth in a signed agreement between the parties or in AMD's Standard Terms and Conditions of Sale.

AMD, the AMD Arrow logo, and combinations thereof are trademarks of Advanced Micro Devices, Inc. Other product names used in this publication are for identification purposes only and may be trademarks of their respective companies.

Copyright (c) 2014-2017 Advanced Micro Devices, Inc. All rights reserved.
