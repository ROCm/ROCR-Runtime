#include <cassert>
#include <cstdint>
#include <fstream>
#include <new>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include "assemble.hpp"
#include "hsa.h"
#include "hsa_ext_finalize.h"
#include "HSAILDisassembler.h"
#include "HSAILParser.h"
#include "HSAILScanner.h"
#include "HSAILValidator.h"
#include "HSAILBrigObjectFile.h"

namespace {
  std::unordered_map<BrigModule_t, uint64_t> mod2con;
} // namespace anonymous

hsa_status_t ModuleCreateFromHsailTextFile(
  const char *hsail_text_filename,
  hsa_ext_module_t *module
) {
  if (!hsail_text_filename) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  std::ifstream hsail_text_file(hsail_text_filename);
  if (!hsail_text_file.is_open()) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  std::string hsail_string;
  hsail_text_file.seekg(0, std::ios::end);
  hsail_string.resize(hsail_text_file.tellg());
  hsail_text_file.seekg(0, std::ios::beg);
  hsail_text_file.read(&hsail_string[0], hsail_string.size());
  hsail_text_file.close();

  return ModuleCreateFromHsailString(hsail_string.c_str(), module);
}

hsa_status_t ModuleCreateFromBrigFile(
  const char *filename,
  hsa_ext_module_t *module
) {
  HSAIL_ASM::BrigContainer *brig_container = new HSAIL_ASM::BrigContainer;
  std::stringstream ss;
  int rc = HSAIL_ASM::BrigIO::load(*brig_container, HSAIL_ASM::FILE_FORMAT_AUTO, HSAIL_ASM::BrigIO::fileReadingAdapter(filename, ss));
  if (rc != 0) { return static_cast<hsa_status_t>(HSA_EXT_STATUS_ERROR_INVALID_MODULE); }
  auto insert_status = mod2con.insert(
    std::make_pair<BrigModule_t, uint64_t>(
      brig_container->getBrigModule(),
      reinterpret_cast<uint64_t>(brig_container)
    )
  );
  assert(insert_status.second);
  *module = brig_container->getBrigModule();
  return HSA_STATUS_SUCCESS;
}

hsa_status_t ModuleCreateFromHsailString(
  const char *hsail_string,
  hsa_ext_module_t *module
) {
  if (!hsail_string || !module) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  HSAIL_ASM::BrigContainer *brig_container = NULL;
  try {
    brig_container = new HSAIL_ASM::BrigContainer;
  } catch (const std::bad_alloc) {
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  std::istringstream hsail_stream(hsail_string);
  try {
    HSAIL_ASM::Scanner hsail_scanner(hsail_stream);
    HSAIL_ASM::Parser hsail_parser(hsail_scanner, *brig_container);
    hsail_parser.parseSource();
  } catch (const SyntaxError) {
    delete brig_container;
    return static_cast<hsa_status_t>(HSA_EXT_STATUS_ERROR_INVALID_MODULE);
  }

  try {
    auto insert_status = mod2con.insert(
      std::make_pair<BrigModule_t, uint64_t>(
        brig_container->getBrigModule(),
        reinterpret_cast<uint64_t>(brig_container)
      )
    );
    assert(insert_status.second);
  } catch (const std::bad_alloc) {
    delete brig_container;
    return HSA_STATUS_ERROR_OUT_OF_RESOURCES;
  }

  *module = brig_container->getBrigModule();
  return HSA_STATUS_SUCCESS;
}

hsa_status_t ModuleDestroy(
  hsa_ext_module_t module
) {
  auto find_status = mod2con.find(module);
  if (find_status == mod2con.end()) {
    return static_cast<hsa_status_t>(HSA_EXT_STATUS_ERROR_INVALID_MODULE);
  }

  HSAIL_ASM::BrigContainer *brig_container =
    reinterpret_cast<HSAIL_ASM::BrigContainer*>(find_status->second);
  assert(brig_container);
  delete brig_container;
  mod2con.erase(find_status);

  return HSA_STATUS_SUCCESS;
}

hsa_status_t ModuleValidate(
  hsa_ext_module_t module,
  uint32_t *result
) {
  if (!result) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  auto find_status = mod2con.find(module);
  if (find_status == mod2con.end()) {
    return static_cast<hsa_status_t>(HSA_EXT_STATUS_ERROR_INVALID_MODULE);
  }

  HSAIL_ASM::BrigContainer *brig_container =
    reinterpret_cast<HSAIL_ASM::BrigContainer*>(find_status->second);
  assert(brig_container);
  HSAIL_ASM::Validator brig_validator(*brig_container);
  *result = brig_validator.validate() ? 0 : 1;

  return HSA_STATUS_SUCCESS;
}

hsa_status_t ModuleDisassemble(
  hsa_ext_module_t module,
  const char *hsail_text_filename
) {
  if (!hsail_text_filename) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  auto find_status = mod2con.find(module);
  if (find_status == mod2con.end()) {
    return static_cast<hsa_status_t>(HSA_EXT_STATUS_ERROR_INVALID_MODULE);
  }

  HSAIL_ASM::BrigContainer *brig_container =
    reinterpret_cast<HSAIL_ASM::BrigContainer*>(find_status->second);
  assert(brig_container);
  HSAIL_ASM::Disassembler brig_disassembler(*brig_container);
  if (brig_disassembler.run(hsail_text_filename)) {
    return HSA_STATUS_ERROR_INVALID_ARGUMENT;
  }

  return HSA_STATUS_SUCCESS;
}
