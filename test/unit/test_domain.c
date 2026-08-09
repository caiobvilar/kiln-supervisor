/* test_domain.c -- host unit test for the placeholder domain module. */
#include "unity.h"

#include "domain.h"

void setUp(void) {}
void tearDown(void) {}

void test_placeholder_returns_42(void) { TEST_ASSERT_EQUAL_INT(42, domain_placeholder()); }

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_placeholder_returns_42);
    return UNITY_END();
}
