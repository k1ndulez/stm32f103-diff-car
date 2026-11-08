#ifndef __CMD_H
#define __CMD_H

#include <stdint.h>
#define CMD_MAX_LEN   12u//数据长度上限

typedef enum {
    CMD_TYPE_NONE = 0,
    CMD_TYPE_WHEELS,   /* 直接给左右轮百分比 */
    CMD_TYPE_YAW       /* 给目标航向，交给 heading */
} cmd_type_t;

typedef struct {
    int16_t left_pct;        /* -100~+100，仅 WHEELS 有效 */
    int16_t right_pct;      /* -100~+100，仅 WHEELS 有效 */
    int16_t yaw_target;     /* 0~359，仅 YAW 有效 */
    cmd_type_t type;        /* 指令类型 */
    uint8_t valid;          /* 1 = 解析成功 */
} cmd_result_t;//解析后的指令

uint8_t cmd_parse(const char *line, cmd_result_t *out);

#endif