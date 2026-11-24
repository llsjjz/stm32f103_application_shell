#ifndef SHELL_H
#define SHELL_H

#include <stdbool.h>
#include <stddef.h>

#define SHELL_CMD_MAX_LENGTH 63U

typedef struct {
    const char *name;
    void (*func)(void);
    unsigned char count;         /* 0 号为命令数量，1..N 为命令使能状态 */
} Shell_t;

/* capacity 是实际数组元素数，包含 0 号管理项；最多注册 255 条命令。
 * Init 同时注册第一条命令；只有初始化成功后才能调用其余接口。
 * name 必须是长期有效、以 NUL 结尾的字符串，长度为 1..63，不能含 CR/LF。
 * 命令表和接收缓冲区不提供并发保护，应由单个任务使用，回调不得递归 Run。
 */
bool Shell_Init(Shell_t* shell_list, size_t capacity, const char* name, void (*func)(void));
bool Shell_Register(Shell_t* shell_list, size_t capacity, const char* name, void (*func)(void));
/* 每个 UART 帧是一条完整命令，允许末尾 CR/LF；无数据或命令无效返回 false。 */
bool Shell_Run(Shell_t* shell_list);
unsigned char Shell_GetCount(Shell_t* shell_list);
bool Shell_Set(Shell_t* shell_list, unsigned char number, unsigned char count);
/* 按注册顺序输出前 count 条命令，包含禁用项；count 超过总数时输出全部。 */
bool Shell_PutCmdCount(Shell_t* shell_list, unsigned char count);

#endif
