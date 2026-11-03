#ifndef __MENU_H
#define __MENU_H

#include "key.h"
extern float g_params[4];
extern int   g_rpm_cmd;

void menu_init(void);
void menu_handle(key_event_t e);
void menu_refresh(void);

#endif