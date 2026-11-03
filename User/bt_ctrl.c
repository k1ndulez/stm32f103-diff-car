/**
  ******************************************************************************
  * @file    bt_ctrl.c
  * @brief   蓝牙指令接收：逐字节状态机，自动识别二进制帧和文本行
  *
  * 为什么要两种格式：
  *   PC 端程序用二进制帧（紧凑、带校验）；
  *   手机串口 App 只能发文本，所以支持 "100,33\n" 这种写法。
  *   两者共用一个入口，靠首字节自动分流，上层代码完全不用改。
  *
  * 为什么用状态机而不是"收满一帧再处理"：
  *   串口是连续字节流，没有"帧"的概念。中间断一次、丢一个字节，
  *   接收端就会永久错位。状态机每来一个字节就判断一次，
  *   一旦发现不对立刻复位，能自己重新对齐。
  ******************************************************************************
  */
#include "bt_ctrl.h"

/* ---------------------------------------------------------------- 状态 --- */
typedef enum
{
    BT_MODE_UNKNOWN = 0,    /* 还没决定这一条是二进制还是文本 */
    BT_MODE_BIN,            /* 正在收 0xAA 二进制帧 */
    BT_MODE_TXT             /* 正在收文本行 */
} bt_mode_t;

static bt_mode_t s_mode;

static uint8_t  s_buf[BT_FRAME_SIZE];   /* 二进制帧缓冲 */
static uint8_t  s_idx;

static char     s_txt[BT_TEXT_MAX];     /* 文本行缓冲 */
static uint8_t  s_tlen;

static int16_t  s_left_rpm;
static int16_t  s_right_rpm;
static uint32_t s_last_frame_ms;
static uint8_t  s_link_ok;
static uint32_t s_frames;
static uint32_t s_errors;

/* -------------------------------------------------------------- 小工具 --- */
/* 把一个 0~255 的字节还原成 -128~127 的有符号数（补码） */
static int16_t byte_to_s8(uint8_t raw)
{
    return (raw > 127u) ? (int16_t)((int16_t)raw - 256) : (int16_t)raw;
}

/* 百分比 -> rpm。例：+100 -> 150，-33 -> -49 */
static int16_t pct_to_rpm(int32_t pct)
{
    return (int16_t)((pct * (int32_t)BT_RPM_MAX) / 100);
}

/* 从字符串里取一个整数，成功返回 1。支持前导空格和正负号 */
static uint8_t next_int(const char **pp, int32_t *out)
{
    const char *p = *pp;
    int32_t sign = 1;
    int32_t v    = 0;
    uint8_t any  = 0u;

    while (*p == ' ' || *p == '\t') p++;
    if (*p == '-')      { sign = -1; p++; }
    else if (*p == '+') { p++; }

    while (*p >= '0' && *p <= '9')
    {
        if (v < 100000) v = v * 10 + (int32_t)(*p - '0');
        p++;
        any = 1u;
    }

    *pp = p;
    if (!any) return 0u;
    *out = sign * v;
    return 1u;
}

/* 限幅后写入目标值，并刷新链路状态 */
static void apply_targets(int32_t l_pct, int32_t r_pct, uint32_t now_ms)
{
    if (l_pct >  100) l_pct =  100;
    if (l_pct < -100) l_pct = -100;
    if (r_pct >  100) r_pct =  100;
    if (r_pct < -100) r_pct = -100;

    s_left_rpm      = pct_to_rpm(l_pct);
    s_right_rpm     = pct_to_rpm(r_pct);
    s_last_frame_ms = now_ms;
    s_link_ok       = 1u;
    s_frames++;
}

/* ------------------------------------------------------------ 文本解析 --- */
static void parse_text_line(const char *s, uint32_t now_ms)
{
    const char *p = s;
    int32_t l = 0;
    int32_t r = 0;

    if (!next_int(&p, &l))
    {
        s_errors++;
        return;
    }

    while (*p == ' ' || *p == '\t') p++;

    if (*p == ',' || *p == ';' || *p == ':')
    {
        p++;
        if (!next_int(&p, &r))
        {
            s_errors++;
            return;
        }
    }
    else
    {
        r = l;                  /* 只给一个数 -> 两轮同速 */
    }

    apply_targets(l, r, now_ms);
}

static void feed_text(uint8_t byte, uint32_t now_ms)
{
    if (byte == '\n' || byte == '\r')
    {
        if (s_tlen > 0u)
        {
            s_txt[s_tlen] = '\0';
            parse_text_line(s_txt, now_ms);
        }
        s_tlen = 0u;
        s_mode = BT_MODE_UNKNOWN;
        return;
    }

    if (byte == ' ' || byte == '\t')
    {
        return;                 /* 行内空格忽略，不占缓冲 */
    }

    if ((byte >= '0' && byte <= '9') || byte == '-' || byte == '+' ||
        byte == ',' || byte == ';' || byte == ':')
    {
        if (s_tlen < (BT_TEXT_MAX - 1u))
        {
            s_txt[s_tlen] = (char)byte;
            s_tlen++;
        }
        else
        {
            s_tlen = 0u;        /* 太长了，整行作废 */
            s_mode = BT_MODE_UNKNOWN;
            s_errors++;
        }
        return;
    }

    /* 其他字符一律忽略（手机上可能混进不可见字符） */
}

/* -------------------------------------------------------- 二进制帧解析 --- */
static void feed_binary(uint8_t byte, uint32_t now_ms)
{
    /* 进来时 s_idx 一定 >= 1（帧头已经收过了） */

    if (s_idx == 1u)
    {
        if (byte == BT_FRAME_LEN)
        {
            s_buf[1] = byte;
            s_idx    = 2u;
        }
        else if (byte == BT_FRAME_HEAD)
        {
            s_buf[0] = BT_FRAME_HEAD;   /* 上一个 AA 是假的，这个才是真帧头 */
            s_idx    = 1u;
        }
        else
        {
            s_idx  = 0u;
            s_mode = BT_MODE_UNKNOWN;
            s_errors++;
        }
        return;
    }

    s_buf[s_idx] = byte;
    s_idx++;

    if (s_idx < BT_FRAME_SIZE)
    {
        return;
    }

    if ((uint8_t)(s_buf[2] + s_buf[3]) == s_buf[4])
    {
        apply_targets(byte_to_s8(s_buf[2]), byte_to_s8(s_buf[3]), now_ms);
    }
    else
    {
        /* 校验失败：不更新目标（保持上一次的值，由超时兜底） */
        s_errors++;

        if (byte == BT_FRAME_HEAD)
        {
            s_buf[0] = BT_FRAME_HEAD;   /* 最后一个是帧头，下一帧已经开始了 */
            s_idx    = 1u;
            return;
        }
    }

    s_idx  = 0u;
    s_mode = BT_MODE_UNKNOWN;
}

/* ---------------------------------------------------------------- 对外 --- */
void bt_init(void)
{
    s_mode          = BT_MODE_UNKNOWN;
    s_idx           = 0u;
    s_tlen          = 0u;
    s_left_rpm      = 0;
    s_right_rpm     = 0;
    s_last_frame_ms = 0u;
    s_link_ok       = 0u;
    s_frames        = 0u;
    s_errors        = 0u;
}

void bt_feed(uint8_t byte, uint32_t now_ms)
{
    /* 还没决定这一条是什么格式：看首字节 */
    if (s_mode == BT_MODE_UNKNOWN)
    {
        if (byte == BT_FRAME_HEAD)
        {
            s_mode   = BT_MODE_BIN;
            s_buf[0] = BT_FRAME_HEAD;
            s_idx    = 1u;
            return;
        }

        if (byte == '\n' || byte == '\r' || byte == ' ' || byte == '\t')
        {
            return;             /* 行首空字符，忽略 */
        }

        if ((byte >= '0' && byte <= '9') || byte == '-' || byte == '+')
        {
            s_mode = BT_MODE_TXT;
            s_tlen = 0u;
            /* 不 return，直接落到下面按文本处理 */
        }
        else
        {
            return;             /* 杂散字节，丢弃 */
        }
    }

    if (s_mode == BT_MODE_TXT)
    {
        feed_text(byte, now_ms);
    }
    else if (s_mode == BT_MODE_BIN)
    {
        feed_binary(byte, now_ms);
    }
}

void bt_tick(uint32_t now_ms)
{
    if (s_link_ok == 0u)
    {
        return;                 /* 本来就没连上，不用重复归零 */
    }

    /* 用无符号相减，天然处理 32 位计时器回绕 */
    if ((uint32_t)(now_ms - s_last_frame_ms) > BT_LINK_TIMEOUT_MS)
    {
        s_left_rpm  = 0;
        s_right_rpm = 0;
        s_link_ok   = 0u;
    }
}

int16_t bt_get_left_rpm(void)
{
    return s_left_rpm;
}

int16_t bt_get_right_rpm(void)
{
    return s_right_rpm;
}

uint8_t bt_link_ok(void)
{
    return s_link_ok;
}

uint32_t bt_frame_count(void)
{
    return s_frames;
}

uint32_t bt_error_count(void)
{
    return s_errors;
}
