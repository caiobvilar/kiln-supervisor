/* test/unit/test_modbus_frame.c -- Modbus RTU ADU build/parse tests. */
/* @verifies KILN-INT-001 */
/* @verifies KILN-CON-001 */
#include "unity.h"

#include <string.h>

#include "modbus_frame.h"

void setUp(void) {}
void tearDown(void) {}

void test_build_read_request(void)
{
    uint8_t out[MODBUS_FRAME_MAX];
    int n = modbus_build_request(out, sizeof(out), 0x01, MODBUS_FC_READ_MULTIPLE, 0x0001, 0x0001);
    TEST_ASSERT_EQUAL_INT(8, n);
    uint8_t expect[] = {0x01, 0x03, 0x00, 0x01, 0x00, 0x01, 0xD5, 0xCA};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expect, out, 8);
}

void test_build_write_single(void)
{
    uint8_t out[MODBUS_FRAME_MAX];
    int n = modbus_build_request(out, sizeof(out), 0x01, MODBUS_FC_WRITE_SINGLE, 0x0001, 0x000A);
    TEST_ASSERT_EQUAL_INT(8, n);
    uint8_t expect[] = {0x01, 0x06, 0x00, 0x01, 0x00, 0x0A, 0x58, 0x0D};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expect, out, 8);
}

void test_build_write_multiple_reject_count_gt16(void)
{
    uint8_t out[MODBUS_FRAME_MAX];
    TEST_ASSERT_LESS_THAN(
        0, modbus_build_request(out, sizeof(out), 0x01, MODBUS_FC_WRITE_MULTIPLE, 0x0001, 17));
}

void test_frame_check_accepts_valid(void)
{
    uint8_t frame[] = {0x01, 0x03, 0x00, 0x01, 0x00, 0x01, 0xD5, 0xCA};
    int n = modbus_frame_check(frame, sizeof(frame), 0x01);
    TEST_ASSERT_GREATER_OR_EQUAL(0, n);
}

void test_frame_check_rejects_corrupt_crc(void)
{
    uint8_t frame[] = {0x01, 0x03, 0x00, 0x01, 0x00, 0x01, 0xD5, 0xCB}; /* bad CRC LSB */
    TEST_ASSERT_LESS_THAN(0, modbus_frame_check(frame, sizeof(frame), 0x01));
}

void test_frame_check_rejects_wrong_address(void)
{
    uint8_t frame[] = {0x01, 0x03, 0x00, 0x01, 0x00, 0x01, 0xD5, 0xCA};
    TEST_ASSERT_LESS_THAN(0, modbus_frame_check(frame, sizeof(frame), 0x02));
}

void test_frame_check_rejects_too_short(void)
{
    uint8_t frame[] = {0x01, 0x03, 0x00};
    TEST_ASSERT_LESS_THAN(0, modbus_frame_check(frame, sizeof(frame), 0x01));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_build_read_request);
    RUN_TEST(test_build_write_single);
    RUN_TEST(test_build_write_multiple_reject_count_gt16);
    RUN_TEST(test_frame_check_accepts_valid);
    RUN_TEST(test_frame_check_rejects_corrupt_crc);
    RUN_TEST(test_frame_check_rejects_wrong_address);
    RUN_TEST(test_frame_check_rejects_too_short);
    return UNITY_END();
}
