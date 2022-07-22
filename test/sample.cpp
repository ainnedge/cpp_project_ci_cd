#include <cpp_project_ci_cd/sample.h>

#include <gtest/gtest.h>

TEST(GtestTester, mulTest) {
  const int a = 7;
  const int b = 3;
  const int result = sample::mul(a, b);
  
  EXPECT_EQ( result, a * b );
}

