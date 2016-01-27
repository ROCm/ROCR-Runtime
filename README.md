### HSA Runtime API and runtime for Boltzmann

This repository includes the user-mode API interfaces and libraries necessary for host applications to launch compute kernels to available HSA Boltzmann kernel agents. Currently supported agents include only the AMD/ATI Fiji(c) family of discreet GPUs. Reference source code for the core runtime is also available.

#### Initial Target Platform Requirements

* CPU: Intel(c) Haswell or newer, Core i5, Core i7, Xeon E3 v4 & v5; Xeon E5 v3
* GPU: Fiji ASIC (AMD R9 Nano, R9 Fury and R9 Fury X)

#### Source code

The HSA core runtime source code for Boltzmann is located in the src subdirectory. Please consult the associated README.md file for contents and build instructions.

#### Binaries for Ubuntu & Fedora

The packages subdirectory contains the Debian and rpm packages for installing the runtime on Ubuntu and Fedora platforms. These files contain:

* HSA, libHSAIL and AMD internal include files to support application development on the HSA runtime for Boltzmann
* A 64-bit version of AMD's HSA core runtime for Boltzmann
* A 64-bit version of AMD's finalizer extension for Boltzmann
* A 64-bit version of AMD's runtime tools library

To install the package on Ubuntu execute the following command:

	`dpkg -i hsa-runtime-dev-1.0.0-amd64.deb`

For Fedora execute the following command:

        `rpm -i hsa-runtime-1.0-0.fc22.x86_64.rpm`

The contents are installed in /opt/hsa by default.
 
#### Infrastructure

The HSA runtime is a thin, user-mode API that exposes the necessary interfaces to access and interact with graphics hardware driven by the AMDGPU driver set and the Boltzmann HSA kernel driver. Together they enable programmers to directly harness the power of AMD discrete graphics devices by allowing host applications to launch compute kernels directly to the graphics hardware.

The capabilities expressed by the HSA Runtime API are:

* Error handling
* Runtime initialization and shutdown
* System and agent information
* Signals and synchronization
* Architected dispatch
* Memory management
* HSA runtime fits into a typical software architecture stack.

The HSA runtime provides direct access to the graphics hardware to give the programmer more control of the execution. Some examples of low level hardware access  is  the support of one or more user mode queues provides programmers with a low-latency kernel dispatch interface, allowing them to develop customized dispatch algorithms specific to their application.

The HSA Architected Queuing Language is an open standard, defined by the HSA Foundation, specifying the packet syntax used to control supported AMD/ATI Radeon (c) graphics devices. The AQL language supports several packet types, including packets that can command the hardware to automatically resolve inter-packet dependencies (barrier AND & barrier OR packet), kernel dispatch packets and agent dispatch packets.

In addition to user mode queues and AQL, the HSA runtime exposes various virtual address ranges that can be accessed by one or more of the system's graphics devices, and possibly the host. The exposed virtual address ranges either support a fine grained or a coarse grained access. Updates to memory in a fine grained region are immediately visible to all devices that can access it, but only one device can have access to a coarse grained allocation at a time. Ownership of a coarse grained region can be changed using the HSA runtime memory APIs, but this transfer of ownership must be explicitly done by the host application.

Programmers should consult the HSA Runtime Programmer's Reference Manual for a full description of the HSA Runtime APIs, AQL and the HSA memory policy.

#### Known Issues

* The image extension is currently not supported for discrete GPUs. An image extension library is not provided in the binary package. The standard hsa_ext_image.h extension include file is provided for reference. 
* Targeted platforms only support SDMA for 2 Boltzmann processes. Running additional Boltzmann processes may fail. Disable SDMA by setting the HSA_ENABLE_SDMA environment variable to 0.
 
#### Disclaimer

The information contained herein is for informational purposes only, and is subject to change without notice. While every precaution has been taken in the preparation of this document, it may contain technical inaccuracies, omissions and typographical errors, and AMD is under no obligation to update or otherwise correct this information. Advanced Micro Devices, Inc. makes no representations or warranties with respect to the accuracy or completeness of the contents of this document, and assumes no liability of any kind, including the implied warranties of noninfringement, merchantability or fitness for particular purposes, with respect to the operation or use of AMD hardware, software or other products described herein. No license, including implied or arising by estoppel, to any intellectual property rights is granted by this document. Terms and limitations applicable to the purchase or use of AMD's products are as set forth in a signed agreement between the parties or in AMD's Standard Terms and Conditions of Sale.

AMD, the AMD Arrow logo, and combinations thereof are trademarks of Advanced Micro Devices, Inc. Other product names used in this publication are for identification purposes only and may be trademarks of their respective companies.

Copyright (c) 2014-2015 Advanced Micro Devices, Inc. All rights reserved.
