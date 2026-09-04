/* test/unit/test_modbus_crc.c -- CRC-16/Modbus known-vector tests. */
/* @verifies KILN-FUN-003 */
#include "unity.h"

#include "modbus_crc.h"

void setUp(void) {}
void tearDown(void) {}

static const uint8_t req_010300000001[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x01};
static const uint8_t req_010300010001[] = {0x01, 0x03, 0x00, 0x01, 0x00, 0x01};
static const uint8_t req_01060001000a[] = {0x01, 0x06, 0x00, 0x01, 0x00, 0x0A};

void test_crc_empty(void) { TEST_ASSERT_EQUAL_UINT16(0xFFFF, modbus_crc16(NULL, 0)); }

void test_crc_example_req(void)
{
    TEST_ASSERT_EQUAL_UINT16(0x0A84, modbus_crc16(req_010300000001, sizeof(req_010300000001)));
}

void test_crc_pv_read_req(void)
{
    TEST_ASSERT_EQUAL_UINT16(0xCAD5, modbus_crc16(req_010300010001, sizeof(req_010300010001)));
}

void test_crc_write_single_req(void)
{
    TEST_ASSERT_EQUAL_UINT16(0x0D58, modbus_crc16(req_01060001000a, sizeof(req_01060001000a)));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_crc_empty);
    RUN_TEST(test_crc_example_req);
    RUN_TEST(test_crc_pv_read_req);
    RUN_TEST(test_crc_write_single_req);
    return UNITY_END();
}
