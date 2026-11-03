#include "menu.h"
#include "key.h"
#include "oled.h"
#include "font.h"
#include "bt_ctrl.h"
#include <stdio.h>

typedef enum
{
    PAGE_MAIN = 0,
    PAGE_BT_CTRL,
    PAGE_AUTO,
} page_t;

static page_t  s_page   = PAGE_MAIN;//当前页
static uint8_t s_cursor = 0;//光标位置
static uint8_t s_edit = 0;          /* 0 = 浏览，1 = 编辑中 */
static float   s_edit_val = 0.0f;   /* 临时值 */
float g_params[4] = { 3.0f, 0.8f, 0.03f, 0.0f };  /* Kp Ki Kd Filter */
int s_rpm_show = 0;          /* 实测转速（以后由速度环更新） */
int g_rpm_cmd  = 0;          /* AUTO 目标转速，可编辑，步长 5 */
static const float k_step[5] = { 5.0f, 0.1f, 0.01f, 0.001f, 1.0f };/*分度值*/
static const float k_min[5]  = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
static const float k_max[5]  = { 300.0f, 100.0f, 10.0f, 1.0f, 10.0f };




void draw_main(void)//主菜单完成图像绘制
{
    static const char *items[2] = { "BT CTRL", "AUTO TRACK"};

    OLED_PrintASCIIString(0, 0, "MODE", &afont12x6, OLED_COLOR_NORMAL);

    for (uint8_t i = 0; i < 2; i++)
    {
        char line[20];
        sprintf(line, "%s%s", (i == s_cursor) ? "> " : "  ", items[i]);
        OLED_PrintASCIIString(0, 16 + i * 16, line, &afont16x8, OLED_COLOR_NORMAL);

    }
    
}

void draw_bt_control(void)
{
    static const char *items[5] = { "RPM", "P", "I", "D", "FILTER" };

    /* 标题 8 px，正文 12 px 行距 11：5 行正好占满 64 px */
    char head[24];
    sprintf(head, "BT %s L%4d R%4d",
        bt_link_ok() ? "OK" : "--",
        bt_get_left_rpm(),
        bt_get_right_rpm());
    OLED_PrintASCIIString(0, 0, head, &afont8x6, OLED_COLOR_NORMAL);

    for (uint8_t i = 0; i < 5; i++)
    {
        int y = 8 + i * 11;
        char line[24];
        const char *sel  = (i == s_cursor) ? ">" : " ";
        const char *mark = (i == s_cursor) ? (s_edit ? "*" : "<") : " ";

        if (i == 0)
        {
            sprintf(line, "%sRPM: %4d%s", sel, s_rpm_show, mark);
        }
        else
        {
            float val = (s_edit && i == s_cursor) ? s_edit_val : g_params[i - 1];
            int v = (int)(val * 1000.0f + 0.5f);
            sprintf(line, "%s%s: %d.%03d%s", sel, items[i], v / 1000, v % 1000, mark);
        }

        OLED_PrintASCIIString(0, y, line, &afont12x6, OLED_COLOR_NORMAL);
    }
}

void draw_auto(void)
{
    static const char *items[5] = { "RPM", "P", "I", "D", "FILTER" };

    /* 标题 8 px，正文 12 px 行距 11：5 行正好占满 64 px */
    OLED_PrintASCIIString(0, 0, "AUTO_TRACK", &afont8x6, OLED_COLOR_NORMAL);

    for (uint8_t i = 0; i < 5; i++)
    {
        int y = 8 + i * 11;
        char line[24];
        const char *sel  = (i == s_cursor) ? ">" : " ";
        const char *mark = (i == s_cursor) ? (s_edit ? "*" : "<") : " ";

        if (i == 0)
        {
            /* 编辑时显示临时值，平时显示目标转速 */
            int rpm = (s_edit && i == s_cursor) ? (int)(s_edit_val + 0.5f) : g_rpm_cmd;
            sprintf(line, "%sRPM: %4d%s", sel, rpm, mark);
        }
        else
        {
            float val = (s_edit && i == s_cursor) ? s_edit_val : g_params[i - 1];
            int v = (int)(val * 1000.0f + 0.5f);
            sprintf(line, "%s%s: %d.%03d%s", sel, items[i], v / 1000, v % 1000, mark);
        }

        OLED_PrintASCIIString(0, y, line, &afont12x6, OLED_COLOR_NORMAL);
    }
}

void menu_draw(void)//绘制动作
{
    OLED_NewFrame();

    switch (s_page)
    {
        case PAGE_MAIN:       draw_main();       break;
        case PAGE_BT_CTRL:    draw_bt_control(); break;
        case PAGE_AUTO:       draw_auto();       break;
        default:              draw_main();       break;
    }
    OLED_ShowFrame();
}

void menu_init(void)
{
    s_page = PAGE_MAIN;
    s_cursor = 0;
    menu_draw();
    s_edit = 0;
}



void handle_main(key_event_t e)//主菜单按键绘制
{
    switch(e)
    {
        case KEY_RIGHT:     s_cursor= (s_cursor+ 1)% 2; break;
        case KEY_LEFT:      s_cursor= (s_cursor+ 1)% 2; break;
        case KEY_OK:        switch(s_cursor)
                            {
                                case 0: s_cursor = 0; s_page = PAGE_BT_CTRL;      break;
                                case 1: s_cursor = 0; s_page = PAGE_AUTO;         break;
                                default:                            break;
                            }                           break;
        default: break;
    }
}

void handle_auto(key_event_t e)
{ 
    switch(e)
    {
        case KEY_RIGHT:     s_cursor= (s_cursor+ 1)% 5; break;
        case KEY_LEFT:      s_cursor= (s_cursor+ 4)% 5; break;
        case KEY_OK:        s_edit = 1;
                            s_edit_val = (s_cursor == 0) ? (float)g_rpm_cmd
                                                         : g_params[s_cursor - 1];
                            break;
        case KEY_BACK:      s_cursor = 0;
                            s_page = PAGE_MAIN;         break;              
        default: break;
    }
}

void handle_btctrl(key_event_t e)
{ 
    switch(e)
    {
        case KEY_RIGHT:     s_cursor= (s_cursor+ 1)% 5; break;
        case KEY_LEFT:      s_cursor= (s_cursor+ 4)% 5; break;
        case KEY_OK:        if (s_cursor >= 1)
                            {
                                s_edit = 1;
                                s_edit_val = g_params[s_cursor - 1];
                            }
                            break;
        case KEY_BACK:      s_cursor = 0;
                            s_page = PAGE_MAIN;         break;              
        default: break;
    }
}

void menu_handle(key_event_t e)//菜单页面跳转
{
    if (s_edit == 1)
    {
        float step = k_step[s_cursor];
        float lo   = k_min[s_cursor];
        float hi   = k_max[s_cursor];

        switch (e)
        {
            case KEY_LEFT:  s_edit_val -= step; break;
            case KEY_RIGHT: s_edit_val += step; break;
            case KEY_OK:
                if (s_cursor == 0) g_rpm_cmd = (int)(s_edit_val + 0.5f);
                else               g_params[s_cursor - 1] = s_edit_val;
                s_edit = 0;
                break;
            case KEY_BACK:  s_edit = 0; break;
            default: break;
        }

        if (s_edit_val < lo) s_edit_val = lo;
        if (s_edit_val > hi) s_edit_val = hi;

        menu_draw();
        return;
    }    
    switch(s_page)
    {
        case PAGE_MAIN:         handle_main(e);     break;/*主菜单*/
        case PAGE_AUTO:         handle_auto(e);     break;/*自动循迹菜单*/
        case PAGE_BT_CTRL:      handle_btctrl(e);   break;/*蓝牙控制菜单*/
        default:                                    break;
    }
    menu_draw();
}

void menu_refresh(void)
{
    menu_draw();
}

