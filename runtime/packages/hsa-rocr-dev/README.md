### HSA Runtime API and runtime for Boltzmann

This repository includes the user-mode API interfaces and libraries necessary for host applications to launch compute kernels to available HSA Boltzmann kernel agents. Reference source code for the core runtime is also available.

Only the AMD/ATI Fiji(c) family of discrete GPUs are currently supported.

#### Initial Target Platform Requirements

* CPU: Intel(c) Haswell or newer, Core i5, Core i7, Xeon E3 v4 & v5; Xeon E5 v3
* GPU: Fiji ASIC (AMD R9 Nano, R9 Fury and R9 Fury X)

#### Source code

The HSA core runtime source code for Boltzmann is located in the src subdirectory. Please consult the associated README.md file for contents and build instructions.

#### Binaries for Ubuntu & Fedora and Installation Instructions

Pre-built binaries are available for installation from the ROCm package repository. For ROCR, they include:

Core runtime package:

* HSA include files to support application development on the HSA runtime for Boltzmann
* A 64-bit version of AMD's HSA core runtime for Boltzmann

Runtime extension package:

* A 64-bit version of AMD's finalizer extension for Boltzmann
* A 64-bit version of AMD's runtime tools library

The contents of these packages are installed in /opt/rocm/hsa and /opt/rocm by default. 
The core runtime package depends on the hsakmt-roct-dev package

Installation instructions can be found in the ROCm manifest repository README.md:

https://github.com/RadeonOpenCompute/ROCm
 
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

#### Sample

The simplest way to check if the kernel, runtime and base development environment are installed correctly is to run a simple sample. A modified version of the vector_copy sample was taken from the HSA-Runtime-AMD repository and added to the ROCR repository to facilitate this. Build the sample and run it, using this series of commands:

cd ROCR-Runtime/sample && make && ./vector_copy

If the sample runs without generating errors, the installation is complete.

#### Known Issues

* The image extension is currently not supported for discrete GPUs. An image extension library is not provided in the binary package. The standard hsa_ext_image.h extension include file is provided for reference. 
* Each HSA process creates and internal DMA queue, but there is a system-wide limit of four DMA queues. The fifths simultaneous HSA process will fail hsa_init() with HSA_STATUS_ERROR_OUT_OF_RESOURCES. To run an unlimited number of simultaneous HSA processes, set the environment variable HSA_ENABLE_SDMA=0.
 
#### Disclaimer

The information contained herein is for informational purposes only, and is subject to change without notice. While every precaution has been taken in the preparation of this document, it may contain technical inaccuracies, omissions and typographical errors, and AMD is under no obligation to update or otherwise correct this information. Advanced Micro Devices, Inc. makes no representations or warranties with respect to the accuracy or completeness of the contents of this document, and assumes no liability of any kind, including the implied warranties of noninfringement, merchantability or fitness for particular purposes, with respect to the operation or use of AMD hardware, software or other products described herein. No license, including implied or arising by estoppel, to any intellectual property rights is granted by this document. Terms and limitations applicable to the purchase or use of AMD's products are as set forth in a signed agreement between the parties or in AMD's Standard Terms and Conditions of Sale.

AMD, the AMD Arrow logo, and combinations thereof are trademarks of Advanced Micro Devices, Inc. Other product names used in this publication are for identification purposes only and may be trademarks of their respective companies.

Copyright (c) 2014-2016 Advanced Micro Devices, Inc. All rights reserved.
