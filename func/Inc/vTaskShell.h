#ifndef VTASKSHELL__H
#define VTASKSHELL__H

#include "func.h"

bool AppShellInit(void);
//任务相关
#define SHELL_Priority 1 // 调试任务优先级
#define SHELL_StackSize 256 // 调试任务栈大小(单位：字)
void vTaskShell(void *pvParameters);

#endif
