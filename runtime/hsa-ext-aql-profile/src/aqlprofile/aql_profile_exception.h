#ifndef _AQL_PROFILE_EXCEPTION_H_
#define _AQL_PROFILE_EXCEPTION_H_

#include <string>
#include <sstream>

namespace aql_profile {

template <typename T> class aql_profile_exception : public std::exception {
 public:
  aql_profile_exception(const std::string& m, const T& v) : msg(m), val(v) {}
  virtual const char* what() const throw() {
    std::ostringstream oss;
    oss << msg << "(" << val << ")";
    return strdup(oss.str().c_str());
  }

 private:
  std::string msg;
  T val;
};
}

#endif  // _AQL_PROFILE_EXCEPTION_H_
