#include "shell.h"
#include "func.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

//*****************************************************************************************************/

static bool Shell_AddCmd(Shell_t* command, const char* name, void (*func)(void))
{
    if (command == NULL || name == NULL || func == NULL) return false;
    if (name[0] == '\0' || strlen(name) > SHELL_CMD_MAX_LENGTH ||
        strpbrk(name, "\r\n") != NULL) return false;

    command->name = name;
    command->func = func;
    command->count = 1;
    return true;
}

static bool Shell_GetCmd(char* str, uint16_t capacity)
{
    uint16_t length = 0;
    if (str == NULL || capacity < 2U) return false;
    if (!Func_Uart_Receive(&uart1, (uint8_t*)str,
                           (uint16_t)(capacity - 1U), &length)) return false;

    /* UART 返回原始字节；按实际长度终止，拒绝含 NUL 的帧，避免误执行前缀。 */
    if (memchr(str, '\0', length) != NULL) return false;
    while (length > 0U && (str[length - 1U] == '\r' || str[length - 1U] == '\n')) {
        --length;
    }
    str[length] = '\0';
    return length > 0U && length <= SHELL_CMD_MAX_LENGTH;
}

static bool Shell_PutCmd(Shell_t* shell_list, unsigned char index)
{
    if (shell_list == NULL || index == 0U || index > shell_list[0].count ||
        shell_list[index].name == NULL) return false;

    /* 为序号、分隔符和行尾留出空间。 */
    char str[SHELL_CMD_MAX_LENGTH + 16U];
    int length = snprintf(str, sizeof(str), "%u : %s\r\n",
                          (unsigned int)index, shell_list[index].name);
    if (length < 0 || (size_t)length >= sizeof(str)) return false;
    return Func_Uart_Transmit(&uart1, (const uint8_t*)str, (uint16_t)length);
}

static bool Shell_FindCmd(Shell_t* shell_list, const char* str, unsigned char* index)
{
    if (shell_list == NULL || str == NULL || str[0] == '\0') return false;

    /* size_t 避免命令数为 255 时循环索引回绕；查重也包含禁用的命令。 */
    for (size_t i = 1; i <= shell_list[0].count; ++i) {
        if (shell_list[i].name != NULL && strcmp(shell_list[i].name, str) == 0) {
            if (index != NULL) *index = (unsigned char)i;
            return true;
        }
    }
    return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool Shell_Init(Shell_t* shell_list, size_t capacity, const char* name, void (*func)(void))
{
    if (shell_list == NULL || capacity < 2U) return false;
    if (!Shell_AddCmd(&shell_list[1], name, func)) return false;

    shell_list[0].name = NULL;
    shell_list[0].func = NULL;
    shell_list[0].count = 1;
    return true;
}

bool Shell_Register(Shell_t* shell_list, size_t capacity, const char* name, void (*func)(void))
{
    if (shell_list == NULL || name == NULL || func == NULL) return false;

    size_t index = (size_t)shell_list[0].count + 1U;
    if (index >= capacity || shell_list[0].count == UCHAR_MAX) return false;
    if (Shell_FindCmd(shell_list, name, NULL)) return false;
    if (!Shell_AddCmd(&shell_list[index], name, func)) return false;

    ++shell_list[0].count;
    return true;
}

bool Shell_Run(Shell_t* shell_list)
{
    if (shell_list == NULL) return false;

    /* 先取走整帧再检查命令长度，避免超长帧一直堵在 UART 队首。
     * 静态分配避免占用任务栈；仅允许单任务、非递归调用。
     */
    static char str[UART_MAX + 1U];
    if (!Shell_GetCmd(str, sizeof(str))) return false;

    unsigned char index = 0;
    if (!Shell_FindCmd(shell_list, str, &index)) return false;
    if (shell_list[index].count == 0U || shell_list[index].func == NULL) return false;

    shell_list[index].func();
    return true;
}

unsigned char Shell_GetCount(Shell_t* shell_list)
{
    if (shell_list == NULL) return 0;
    return shell_list[0].count;
}

bool Shell_Set(Shell_t* shell_list, unsigned char number, unsigned char count)
{
    if (shell_list == NULL || number == 0U || number > shell_list[0].count) return false;
    shell_list[number].count = (count != 0U);
    return true;
}

bool Shell_PutCmdCount(Shell_t* shell_list, unsigned char count)
{
    if (shell_list == NULL) return false;

    size_t maxnum = count < shell_list[0].count ? count : shell_list[0].count;
    for (size_t i = 1; i <= maxnum; ++i) {
        if (!Shell_PutCmd(shell_list, (unsigned char)i)) return false;
    }
    return true;
}
