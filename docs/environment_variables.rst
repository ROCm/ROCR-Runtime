.. meta::
   :description: HSA runtime implementation
   :keywords: ROCR, ROCm, library, tool, runtime

.. _environment-variables:

Environment variables
========================

The following table lists the most often used environment variables.

.. list-table:: ROCR environment variables
    :header-rows: 1
    
    * - Environment variable
      - Possible values
      - Description

    * - HSA_ENABLE_SDMA
      - 
        * 0: Disabled
        * 1: Enabled (default)
      - This controls the use of DMA engines in all copy directions (Host-to-Device, Device-to-Host, Device-to-Device) when using the
        ``hsa_memory_copy``, ``hsa_amd_memory_fill``, ``hsa_amd_memory_async_copy``, ``hsa_amd_memory_async_copy_on_engine`` APIs

    * - HSA_ENABLE_PEER_SDMA
      -
        * 0: Disabled
        * 1: Enabled (default)
      - This controls the use of DMA engines for Device-to-Device copies when using the ``hsa_memory_copy``, ``hsa_amd_memory_async_copy``, ``hsa_amd_memory_async_copy_on_engine`` APIs

.. note::
    
    The value of ``HSA_ENABLE_PEER_SDMA`` is ignored if ``HSA_ENABLE_SDMA`` is used to disable the use of DMA engines.
