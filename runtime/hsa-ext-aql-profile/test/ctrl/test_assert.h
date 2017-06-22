#ifndef _TEST_ASSERT_H_
#define _TEST_ASSERT_H_

#define test_assert(cond)                                                                          \
  {                                                                                                \
    if (!(cond)) {                                                                                 \
      std::cout << "ASSERT FAILED(" << #cond << ") at \"" << __FILE__ << "\" line " << __LINE__    \
                << std::endl;                                                                      \
      abort();                                                                                     \
    }                                                                                              \
  }

#endif  // _TEST_ASSERT_H_
