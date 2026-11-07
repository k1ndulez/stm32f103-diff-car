#ifndef __CMD_H
#define __CMD_H

#include <stdint.h>
#define CMD_MAX_LEN   12u//数据长度上限

typedef struct {
    int16_t left_pct;    /* -100 ~ +100 */
    int16_t right_pct;
    uint8_t valid;
} cmd_result_t;//两轮速度及其数据状态

uint8_t cmd_parse(const char *line, cmd_result_t *out);

#endif