#include <cpp_project_ci_cd/cpp_project_ci_cd.h>

#include <gtest/gtest.h>

TEST(GtestTester, mulTest) {
  const int n1 = 7;
  const int n2 = 3;
  const int result = cppProjectCiCd::mul(n1, n2);
  
  EXPECT_EQ( result, n1 * n2 );
}

