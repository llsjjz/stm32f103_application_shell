#include "func.h"

#include "led.h"
#include "debug.h"
#include "key.h"


//******************************************************************************************/
func_uart_base_t uart1;
static uint8_t txbuffer[UART_MAX];
static uint8_t rxbuffer[UART_MAX];
//******************************************************************************************/

void func_init(void)
{
  Func_Uart_Init(&uart1,rxbuffer,UART_MAX,txbuffer,UART_MAX,&huart1);

  // 创建任务
  xTaskCreate(vTaskLED, "LED", LED_StackSize, NULL, LED_Priority, &LED_TaskHandle); // 优先级设为 2，较高

  vTaskStartScheduler();
}

void func_while(void)
{
  
}

// 重定向 printf 到 UART1
int _write(int file, char *ptr, int len)
{
    Func_Uart_Transmit(&uart1, (uint8_t *)ptr , len);
    //HAL_UART_Transmit(&huart1, (uint8_t *)ptr, len, HAL_MAX_DELAY);
    return len;
}