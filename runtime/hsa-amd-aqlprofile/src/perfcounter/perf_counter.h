#ifndef _HSA_PERF_H_
#define _HSA_PERF_H_

#include <stdint.h>

#include <vector>
#include <map>
#include <string>

namespace pm4_profile {
class DefaultCmdBuf;
class CommandWriter;

typedef std::vector<uint32_t> CountersVec;
typedef std::map<uint32_t, CountersVec> CountersMap;

class PerfCounter {
 public:
  virtual ~PerfCounter() {}

  // Generate start profiling commands.
  virtual void begin(DefaultCmdBuf* cmdBuff, CommandWriter* cmdWriter,
                     const CountersMap& countersMap) = 0;

  // Generate stop profiling commands.
  // Return actual required data buffer size.
  virtual uint32_t end(DefaultCmdBuf* cmdBuff, CommandWriter* cmdWriter,
                       const CountersMap& countersMap, void* dataBuff) = 0;

  // Returns number of shader engines per block
  // for the blocks featured shader engines instancing
  virtual uint32_t getNumSe() = 0;
};
}  // namespace pm4_profile
#endif  // _HSA_PERF_H_
