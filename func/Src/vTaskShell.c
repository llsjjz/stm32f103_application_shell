#include "vTaskShell.h"
#include "shell.h"

static Shell_t commands[9];
#define COMMAND_CAPACITY (sizeof(commands) / sizeof(commands[0]))

static void CmdPing(void)
{
    printf("pong\r\n");
}

static void CmdHelp(void)
{
    (void)Shell_PutCmdCount(commands, Shell_GetCount(commands));
}

bool AppShellInit(void)
{
    if (!Shell_Init(commands, COMMAND_CAPACITY, "ping", CmdPing))
        return false;

    return Shell_Register(commands, COMMAND_CAPACITY, "help", CmdHelp);
}

//任务相关
void vTaskShell(void *pvParameters) 
{
  ( void ) pvParameters;
  for (;;) {
    (void)Shell_Run(commands);
    
    vTaskDelay(pdMS_TO_TICKS( 100 )); // 延迟1000个时钟节拍，即1000ms
  }
  vTaskDelete(NULL); // 删除当前任务，永远不会执行到这里
}