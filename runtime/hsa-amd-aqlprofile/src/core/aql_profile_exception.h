#ifndef _AQL_PROFILE_EXCEPTION_H_
#define _AQL_PROFILE_EXCEPTION_H_

#include <string.h>

#include <string>
#include <sstream>

namespace aql_profile {

class aql_profile_exc_msg : public std::exception {
 public:
  explicit aql_profile_exc_msg(const std::string& msg) : str(msg) {}
  virtual const char* what() const throw() { return str.c_str(); }

 protected:
  std::string str;
};

template <typename T> class aql_profile_exc_val : public std::exception {
 public:
  aql_profile_exc_val(const std::string& msg, const T& val) {
    std::ostringstream oss;
    oss << msg << "(" << val << ")";
    str = oss.str();
  }
  virtual const char* what() const throw() { return str.c_str(); }

 protected:
  std::string str;
};
}  // namespace aql_profile

#endif  // _AQL_PROFILE_EXCEPTION_H_
