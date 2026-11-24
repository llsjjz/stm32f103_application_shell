# 极简串口 Shell

面向 STM32 + FreeRTOS 的超轻量命令行解析库。约 130 行 C 代码，零动态内存，静态表驱动，开箱即可挂到任意 UART 上。

```text
> help
1 : ping
2 : help

> ping
pong
```

---

## 目录

- [特性](#特性)
- [目录结构](#目录结构)
- [快速开始](#快速开始)
- [集成到你自己的工程](#集成到你自己的工程)
- [API 参考](#api-参考)
- [工作原理](#工作原理)
- [限制与注意事项](#限制与注意事项)
- [常见问题](#常见问题)
- [延伸阅读](#延伸阅读)

---

## 特性

| 特性 | 说明 |
| --- | --- |
| 零动态内存 | 命令表由调用者静态分配，运行期无 `malloc` |
| 零栈开销 | 接收缓冲区为 `static`，不占用任务栈 |
| 纯表驱动 | 注册即用，无需修改解析逻辑 |
| 定长命令名 | `SHELL_CMD_MAX_LENGTH`（63）字节，编译期即可控 |
| 精确匹配 | `strcmp` 全等匹配，`p` 不会误命中 `ping` |
| 可禁用命令 | 运行期按序号启用/禁用，无需注销 |
| 完整帧校验 | 拒绝含 `\0` 的帧、自动裁剪尾部 `\r\n`、超长帧直接丢弃 |
| 无阻塞 | 发送走 DMA + 环形队列，不阻塞调用者 |

## 目录结构

```text
func/
├── Inc/
│   ├── shell.h          # 唯一需要包含的头文件
│   └── vTaskShell.h     # 示例：FreeRTOS 任务封装
└── Src/
    ├── shell.c          # 库本体
    └── vTaskShell.c     # 示例：注册 ping / help 两条命令
```

`shell.c` 只依赖两个 I/O 函数（来自 `func_uart_0`）：

```c
bool Func_Uart_Receive (func_uart_base_t *dev, uint8_t *frame, uint16_t length, uint16_t *out_length);
bool Func_Uart_Transmit(func_uart_base_t *dev, const uint8_t *frame, uint16_t length);
```

## 快速开始

### 1. 准备环境

- STM32F103ZET6（任意 Cortex-M + HAL 均可）
- UART 已由 CubeMX 生成，DMA 收发已开启
- 串口参数：**115200 / 8-N-1**

### 2. 构建

```bash
cmake --preset Debug
cmake --build --preset Debug
```

产物：`build/Debug/stm32f103zet6_freeRTOS_Dome.hex`

### 3. 上板运行

烧录后打开串口终端，发送 `help` 回车：

```text
1 : ping
2 : help
```

## 集成到你自己的工程

### 方式 A：使用 CMake（推荐）

```cmake
target_sources(your_target PRIVATE
    path/to/func/Src/shell.c
    path/to/func/Src/func_uart_0.c
    path/to/func/Src/circle_buffer.c
)

target_include_directories(your_target PRIVATE
    path/to/func/Inc
)
```

### 方式 B：最小化移植

1. 拷贝 `shell.h` / `shell.c` 到你的工程。
2. 让 `shell.c` 能找到 `func.h`，或把 `shell.c` 里的两处 UART 调用替换成你自己的实现：
   - `Shell_GetCmd()` → 从串口取出**一整帧**数据（以换行或空闲中断为界）
   - `Shell_PutCmd()` → 输出一行文本
3. 在初始化处调用 `Shell_Init()`，在任务/主循环里轮询 `Shell_Run()`。

## API 参考

### 数据结构

```c
typedef struct {
    const char  *name;    /* 命令名，指向长期有效的字符串 */
    void      (*func)(void);  /* 命令回调，无参无返回 */
    unsigned char count;  /* [0] 为命令总数；[1..N] 为 1=启用 / 0=禁用 */
} Shell_t;
```

> ⚠️ **下标 0 是管理项，不存放命令。** 因此数组至少要 `最多命令数 + 1` 个元素。

### 接口一览

| 函数 | 说明 | 返回值 |
| --- | --- | --- |
| `Shell_Init(list, capacity, name, func)` | 初始化命令表，并注册第一条命令 | 成功 `true` |
| `Shell_Register(list, capacity, name, func)` | 追加注册一条命令，重名拒绝 | 成功 `true` |
| `Shell_Run(list)` | 取一帧命令并执行 | 执行成功 `true` |
| `Shell_GetCount(list)` | 已注册命令数 | `0..255` |
| `Shell_Set(list, number, count)` | 启用(`1`)/禁用(`0`)第 `number` 条 | 成功 `true` |
| `Shell_PutCmdCount(list, count)` | 打印前 `count` 条命令（含禁用项） | 成功 `true` |

### 参数约定

| 参数 | 约束 |
| --- | --- |
| `list` | 非 `NULL`，元素个数 ≥ 2 |
| `capacity` | 数组元素总数（**含**下标 0），必须 ≥ 2 |
| `name` | 非空、以 `\0` 结尾、长度 1..63、不含 `\r` `\n`，且**生命周期长于命令表**（建议用字符串字面量） |
| `func` | 非 `NULL` |
| `number` | `1..Shell_GetCount()`，`0` 非法 |
| `count` | `Shell_Set` 中为 `0/1`；`Shell_PutCmdCount` 中超出总数则输出全部 |

### 完整示例

```c
#include "shell.h"
#include "vTaskShell.h"

static Shell_t commands[9];                          /* 最多 8 条命令 + 1 个管理项 */
#define COMMAND_CAPACITY (sizeof(commands) / sizeof(commands[0]))

static void CmdPing(void) { printf("pong\r\n"); }

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

void vTaskShell(void *pvParameters)
{
    (void)pvParameters;
    for (;;) {
        (void)Shell_Run(commands);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

在 `func_init()` 中调用 `AppShellInit()` 并创建任务：

```c
AppShellInit();
xTaskCreate(vTaskShell, "Shell", SHELL_StackSize, NULL, SHELL_Priority, NULL);
```

## 工作原理

```text
UART RX (DMA + IDLE)
   │  一帧 = 一次 IDLE / TC 之间的全部字节
   ▼
rxbuffer ──► Shell_GetCmd()      裁剪尾部 \r\n，拒绝含 \0 的帧
   │
   ▼
Shell_FindCmd()                  strcmp 精确匹配，跳过禁用项
   │
   ▼
command->func()                  执行回调
```

关键设计：

- **一次 `Shell_Run()` 只处理一帧**，天然与中断解耦，不阻塞。
- **命令表与数量分离**：`list[0].count` 存总数，命令固定从 `list[1]` 开始，边界判断只需一次比较。
- **拒绝半帧**：超长帧、含 `\0` 的帧、无数据帧一律返回 `false`，不会误执行前缀。
- **发送无拷贝等待**：输出经环形队列 + DMA，`Func_Uart_Transmit` 入队立即返回。

## 限制与注意事项

1. **一条命令一帧。** 若两次发送间隔小于 1 个字符时间，会被合并成一帧，只解析一次。脚本化烧录时请在命令之间加延时，或用 `\r\n` 逐条发送并等待回显。
2. **命令不带参数。** 回调签名为 `void (*)(void)`，如需 `led on` 这类语法，请改用 `int argc, char *argv[]` 版本。
3. **不做行编辑。** 不支持退格、方向键、历史记录；请使用普通串口终端（关闭本地回显）。
4. **仅单任务使用。** 命令表与接收缓冲区无锁保护，请只在一个任务中调用 `Shell_*`；回调内不得递归调用 `Shell_Run`。
5. **命令名上限 63 字节**（`SHELL_CMD_MAX_LENGTH`），单帧上限受 `UART_MAX` 约束。
6. **大小写敏感**，`PING` 不会命中 `ping`。

## 常见问题

**Q：注册返回 `false`，命令却没出现？**
A：检查三件事——数组是否预留了下标 0、`capacity` 是否等于数组元素总数、命令名是否已存在（重名会被拒绝）。

**Q：串口发了命令没反应？**
A：`Shell_Run()` 是轮询的，确认它被周期性调用；并确认一帧结束时总线确实产生了 IDLE（发送端不要在帧中间停顿）。

**Q：为什么 `help` 只显示前几条？**
A：`Shell_PutCmdCount` 的 `count` 是"打印条数"，传 `Shell_GetCount(commands)` 才是全部。

**Q：能注册多少条命令？**
A：受两个上限约束——数组容量，以及 `count` 字段的 `255`（`UCHAR_MAX`）。

## 延伸阅读

- [`Shell库建立与优化学习笔记.md`](./Shell库建立与优化学习笔记.md) —— 设计取舍与迭代过程
- [`FreeRTOS移植步骤.md`](./FreeRTOS移植步骤.md) —— 本工程的 RTOS 移植说明
- [`func/Src/shell.c`](./func/Src/shell.c) —— 库本体，含逐行注释
- [`func/Src/func_uart_0.c`](./func/Src/func_uart_0.c) —— 帧收发与分帧机制

---

## 许可证

本仓库尚未声明开源许可证。如计划对外发布，建议补充 `LICENSE` 文件（MIT / Apache-2.0 均可）。