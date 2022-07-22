#include <cpp_project_ci_cd/cpp_project_ci_cd.h>

#include <gtest/gtest.h>

TEST(GtestTester, mulTest1) {
  const int n1 = 7;
  const int n2 = 3;
  const int result = cppProjectCiCd::mul(n1, n2);
  
  EXPECT_EQ( result, n1 * n2 );
}

TEST(GtestTester, mulTest2) {
  const int n1 = -7;
  const int n2 = 3;
  const int result = cppProjectCiCd::mul(n1, n2);
  
  EXPECT_EQ( result, n1 * n2 );
}

TEST(GtestTester, divTest1) {
  const int n1 = 207;
  const int n2 = 23;
  const int result = cppProjectCiCd::mul(n1, n2);
  
  EXPECT_EQ( result, n1 / n2 );
}

