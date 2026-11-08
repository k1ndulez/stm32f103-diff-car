#include "cmd.h"

static int32_t wheels_limit_pct (int32_t v)
{
    if (v > 100) v = 100;
    if (v < -100)v = -100;
    return v;
}

static int32_t normalize_yaw (int32_t target_yaw)
{
    target_yaw %= 360;
    if (target_yaw < 0) target_yaw += 360;
    return target_yaw;
}



static uint8_t parse_uint(const char **pp, int32_t *out)//从字符串里提取一个数字，并把指针往后推
{
    const char *p = *pp;
    int32_t v = 0;
    uint8_t any = 0;

    while (*p == ' ' || *p == '\t') p++;////跳过空格

    while (*p >= '0' && *p <= '9')
    {
        if (v < 1000) v = v * 10 + (*p - '0');
        p++;
        any = 1;
    }

    *pp = p;

    if (!any) return 0;
    *out = v;
    return 1;
}
uint8_t cmd_parse(const char *line, cmd_result_t *res)//翻译数据
{
   const char *p;
    int32_t v = 100;
    char c;

    if (!line || !res) return 0;
    res->valid = 0;
    res->type = CMD_TYPE_NONE;//初始化res
    res->yaw_target = 0;
    res->left_pct = 0; 
    res->right_pct = 0;

    while (*line == ' ' || *line == '\t') line++;
    c = *line;
    if (!c) return 0;

    p = line + 1;

    uint8_t has_num = parse_uint(&p, &v);

    switch (c)
    {
    case 'F': case 'f': v = wheels_limit_pct(v) ;res->left_pct =  v; res->right_pct =  v; res->type = CMD_TYPE_WHEELS; break;
    case 'B': case 'b': v = wheels_limit_pct(v) ;res->left_pct = -v; res->right_pct = -v; res->type = CMD_TYPE_WHEELS; break;
    case 'L': case 'l': v = wheels_limit_pct(v) ;res->left_pct = -v; res->right_pct =  v; res->type = CMD_TYPE_WHEELS; break;
    case 'R': case 'r': v = wheels_limit_pct(v) ;res->left_pct =  v; res->right_pct = -v; res->type = CMD_TYPE_WHEELS; break;
    case 'S': case 's': v = wheels_limit_pct(v) ;res->left_pct =  0; res->right_pct =  0; res->type = CMD_TYPE_WHEELS; break;
    case 'Y': case 'y': if (!has_num) return 0; v = normalize_yaw (v);     res->yaw_target = v; res->type = CMD_TYPE_YAW;                        break;
    
    default: return 0;
    }

    res->valid = 1;//标志位说明解析成功
    return 1;
}