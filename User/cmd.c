#include "cmd.h"

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

    while (*line == ' ' || *line == '\t') line++;
    c = *line;
    if (!c) return 0;

    p = line + 1;

    parse_uint(&p, &v);

    if (v > 100) v = 100;
    if (v < 0)   v = 0;

    switch (c)
    {
    case 'F': case 'f': res->left_pct =  v; res->right_pct =  v; break;
    case 'B': case 'b': res->left_pct = -v; res->right_pct = -v; break;
    case 'L': case 'l': res->left_pct = -v; res->right_pct =  v; break;
    case 'R': case 'r': res->left_pct =  v; res->right_pct = -v; break;
    case 'S': case 's': res->left_pct =  0; res->right_pct =  0; break;
    default: return 0;
    }

    res->valid = 1;//标志位说明解析成功
    return 1;
}