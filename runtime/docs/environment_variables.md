# Environment Variables

## HSA_ENABLE_SDMA

Possible values:

* 0:Disabled
* 1:Enabled (Default Value)

This will enable or disable the use of DMA engines in all copy directions (Host-to-Device, Device-to-Host, Device-to-Device) when using the following APIs:
`hsa_memory_copy`, `hsa_amd_memory_fill`, `hsa_amd_memory_async_copy`, `hsa_amd_memory_async_copy_on_engine`

## HSA_ENABLE_PEER_SDMA

Possible values:

* 0:Disabled
* 1:Enabled (Default Value)

This will enable or disable the use of DMA engines for Device-to-Device copies when using the following APIs:
`hsa_memory_copy`, `hsa_amd_memory_async_copy`, `hsa_amd_memory_async_copy_on_engine`

The value of `HSA_ENABLE_PEER_SDMA` is ignored if `HSA_ENABLE_SDMA` is used to disable the use of DMA engines.
