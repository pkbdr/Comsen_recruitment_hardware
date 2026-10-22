# 实数/复数表达式模块

本模块由 AI 根据使用者要求编写，源码公开在 `Core/Src/basic_calc.c`，
接口在 `Core/Inc/basic_calc.h`。未使用提供的 `libcalculator_engine.a`。
触摸读取、输入追加和清空是使用者在 AI 指导下编写的部分；计算核心应单独说明来源。

## 已实现的功能

- 最多 16 字节输入，明确传入长度，不需要以 `\0` 结尾。
- `+ - * /`、括号和乘方 `^`，先乘方后乘除再加减；乘方从右到左，其余同级从左到右。
- 整数、小数、数字前的单个正负号，允许空格和制表符。
- 支持 `1+-2`、`1--2`、`2*-3`，不支持连续多个一元符号 `--1`。
- 支持 `sin`、`cos`、`tan`、`log`（底数 10）、`ln`、`sqrt`、常数 `pi` 和 `e`。
- 默认三角函数使用角度制，`basic_calc_evaluate_angle` / `basic_calc_render_angle` 可明确选择 DEG/RAD。
- 原实数接口不接受复数，第六项另提供 `basic_calc_evaluate_complex` / `basic_calc_render_complex`；见 [COMPLEX_CALC.md](COMPLEX_CALC.md)。
- 不支持输入科学计数法和一般隐式乘法；复数系数支持 `2i`。函数必须使用括号，单个前置负号的优先级低于乘方。
- 使用 `double`，结果显示保留最多 9 位有效数字，必要时用科学计数法。
- 固定输出 16 格，用空格补齐，不带 `\0`；不可直接当作普通 C 字符串使用。
- 空输入、语法错误、除零、定义域错误、输入过长和非有限/乘方下溢结果分别返回状态，并显示英文提示。
- 无动态内存分配，不依赖 HAL、RTOS 或可选的浮点 printf。

## 调用方法

在需要使用的源文件中包含：

```c
#include "basic_calc.h"
```

准备一个 16 格的结果数组，在 OK 键刚按下时调用：

```c
char result_line[16];
basic_calc_status_t status = basic_calc_render(input, input_len, result_line);
```

`input` 和 `input_len` 就是现有输入数组和有效长度。`result_line` 在成功时包含
结果，失败时包含错误提示。若不需要区分错误种类，可以直接调用而不保存返回值：

```c
basic_calc_render(input, input_len, result_line);
lcd1602_write_lines(input, result_line);
```

例如 `1+2*3` 显示 `7`，`8/4/2` 显示 `1`，`1/0` 显示 `Divide by zero`，
`1+` 显示 `Syntax ERROR`。显示舍入不改变内部计算的 double 值。

2026-10-22 已通过 `calculator_ui.c` 接入括号、SHIFT 数学函数、DEG/RAD、顶部 OK（T4）和面板 EXE（T29），
主循环保持结果显示，并协调 USB 文本显示与计算器界面的切换。整合版本通过
电脑测试和 WSL 构建，仍需实际烧录后确认触摸和整机行为。

## 测试

`tests/basic_calc_test.c` 覆盖优先级、同级结合顺序、正负号、小数、错误输入、
不带结束符的数组、输入长度限制、显示舍入、极大/极小数和输出内存边界。

在有本机 GCC 的工程目录中运行：

```text
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -fanalyzer -I Core/Inc Core/Src/basic_calc.c Core/Src/complex_math.c tests/basic_calc_test.c -lm -o basic_calc_test.exe
./basic_calc_test.exe
```

Windows 的 DLL 随机测试：

```text
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -shared -I Core/Inc Core/Src/basic_calc.c Core/Src/complex_math.c -o basic_calc_native.dll
python tests/basic_calc_random_test.py ./basic_calc_native.dll
```

四则算式与 Python 有理数参考比较，另由独立语法树生成函数/括号/乘方算式与 Python math 比较；另测 10000 个浮点显示和 4000 个任意字节输入。
这些是电脑上执行的测试，不代替实板的按键、显示和整机联调。
