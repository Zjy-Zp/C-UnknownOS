#ifndef ACPI_H
#define ACPI_H

#include "common.h"

int acpi_init(void);        /* 找到 FADT 返回 1 */
int acpi_prepare(void);     /* 使能 ACPI 并解析 \_S5_，填充关机参数 */
int acpi_shutdown(void);    /* 触发 S5 断电，成功则不返回 */
int acpi_available(void);
const char *acpi_oem_id(void);

/* 诊断用：prepare 之后读取 */
u16 acpi_pm1a_port(void);
u16 acpi_slp_typ(void);
int acpi_sci_enabled(void);
u16 acpi_pm1cnt_raw(void);
u16 acpi_readback(void);
u16 acpi_dbg(int i);

#endif
