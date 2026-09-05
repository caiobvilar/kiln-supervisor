#ifndef KILN_REGISTERS_H
#define KILN_REGISTERS_H

/* KM5P_r0 Modbus holding-register numbers, from the Ascon Tecnologic register
 * map recorded in docs/hardware/km5p-controller.md.
 *
 * ALL VALUES UNVERIFIED -- bench read-back against the physical unit is pending
 * (SRS OI-01). No code may depend on these until verified; they exist here so
 * the documented numbers are auditable in one place.
 */
#define KILN_REG_PV          1u    /* 0x0001 -- live measured value, signed, dP */
#define KILN_REG_OP_SETPOINT 3u    /* 0x0003 -- operative set point, dP, r */
#define KILN_REG_SP          6u    /* 0x0006 -- settable set point, r/w */
#define KILN_REG_ALARMS      10u   /* 0x000A -- alarm status bitmask */
#define KILN_REG_ALARM_ACK_A 13u   /* 0x000D -- alarm reset/ack, r/w */
#define KILN_REG_ALARM_ACK_B 14u   /* 0x000E -- alarm reset/ack, r/w */
#define KILN_REG_CONTROL     15u   /* 0x000F -- 0 Auto, 1 Manual, 2 Standby, r/w */
#define KILN_REG_PROG_STATUS 580u  /* 0x0244 -- program status, 0..7, r/w */
#define KILN_REG_PROG_STEP   582u  /* 0x0246 -- program step in execution, 0..9 */
#define KILN_REG_ADDR        10337u /* 0x2861 -- param Add, oFF or 1..254, r/w */
#define KILN_REG_BAUD        10338u /* 0x2862 -- param bAud, 0..4, r/w */
#define KILN_REG_PR_ST       10367u /* 0x287F -- param Pr.St, 0 reset..3 continue */

#endif /* KILN_REGISTERS_H */