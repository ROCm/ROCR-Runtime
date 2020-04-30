#include "inc/hsa.h"
#include "inc/hsa_api_trace.h"
#include "core/inc/hsa_table_interface.h"

#ifdef __cplusplus
extern "C" {
#endif

void HSA_API_EXPORT Load(const ::HsaApiTable* table);
void HSA_API_EXPORT Unload();

// Per library unload callback function. Set by the finalizer or image library
// when needed.
void (*UnloadCallback)() = NULL;

void Load(const ::HsaApiTable* table) {
  // Setup to bypass the runtime intercept layer.
  hsa_table_interface_init(table);
}

void Unload() {
  if (UnloadCallback != NULL) {
    UnloadCallback();
  }
}

#ifdef __cplusplus
}
#endif
