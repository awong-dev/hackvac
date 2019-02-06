#ifndef ESPCXX_TEST_H_
#define ESPCXX_TEST_H_

#ifdef GTEST_MAIN
#define ESPCXX_MOCKABLE virtual
#else
#define ESPCXX_MOCKABLE
#endif

#endif  // ESPCXX_TEST_H_
