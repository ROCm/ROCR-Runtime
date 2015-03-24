#ifndef ASSEMBLE_HPP_
#define ASSEMBLE_HPP_

#include <cstdint>
#include "hsa.h"
#include "hsa_ext_finalize.h"

#if defined(_MSC_VER)
  #ifdef __cplusplus
  extern "C" {
  #endif
#endif

hsa_status_t ModuleCreateFromHsailTextFile(
  const char *hsail_text_filename,
  hsa_ext_module_t *module
);

hsa_status_t ModuleCreateFromBrigFile(
  const char *hsail_text_filename,
  hsa_ext_module_t *module
);

hsa_status_t ModuleCreateFromHsailString(
  const char *hsail_string,
  hsa_ext_module_t *module
);

hsa_status_t ModuleDestroy(
  hsa_ext_module_t module
);

hsa_status_t ModuleValidate(
  hsa_ext_module_t module,
  uint32_t *result
);

hsa_status_t ModuleDisassemble(
  hsa_ext_module_t module,
  const char *hsail_text_filename
);

#if defined(_MSC_VER)
  #ifdef __cplusplus
  }
  #endif
#endif


#endif // ASSEMBLE_HPP_
