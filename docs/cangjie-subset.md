# Cangjie specification excerpt

This file is the Cangjie counterpart of [docs/language.md](language.md).
`plain-llama` appends it to the benchmark's Cangjie planning and code prompts.
`typed-llama` does not. The pages are copied from CangjieSkills,
https://github.com/SunriseSummer/CangjieSkills branch `cangjie-1.0.5`, under
`references/language`. They cover the same kind of fragment as the typed
specification: functions, bindings, conditions, loops, integers, Booleans,
operators, and `main`.

Generator comments and parent navigation links are omitted. The remaining text,
including the Chinese prose and examples, is the upstream documentation. Those
pages describe Cangjie 1.0.5. The local compiler used for scoring may be newer.

Source pages:
- `references/language/function/1-函数定义/1-1-基本语法.md`
- `references/language/function/1-函数定义/1-2-参数列表.md`
- `references/language/function/1-函数定义/1-3-返回类型.md`
- `references/language/function/1-函数定义/1-4-函数体.md`
- `references/language/basic_concepts/2-程序结构/2-2-变量定义.md`
- `references/language/basic_concepts/3-表达式/3-2-if-条件判断.md`
- `references/language/basic_concepts/3-表达式/3-3-while-do-while-循环.md`
- `references/language/basic_concepts/3-表达式/3-5-break-continue.md`
- `references/language/basic_data_type/1-整数类型/1-1-类型列表.md`
- `references/language/basic_data_type/1-整数类型/1-2-整数字面量.md`
- `references/language/basic_data_type/1-整数类型/1-5-整数支持的运算.md`
- `references/language/basic_data_type/11-基本运算符/11-2-算术运算符.md`
- `references/language/basic_data_type/11-基本运算符/11-4-关系运算符.md`
- `references/language/basic_data_type/11-基本运算符/11-5-逻辑运算符.md`
- `references/language/basic_data_type/3-布尔类型.md`
- `references/language/package/3-程序入口/3-1-main-函数.md`


## Source: `references/language/function/1-函数定义/1-1-基本语法.md`

# 1.1 基本语法


函数以 `func` 开头，依次声明名称、参数列表、可选返回类型和函数体；函数体最后一个表达式可作为返回值。

```cangjie cjtest=syntax id=syntax-42d5419be4-1 form=unit
func add(a: Int64, b: Int64): Int64 {
    a + b
}
```
- 关键字 `func` 开头
- 参数在 `()` 内，逗号分隔
- 返回类型在参数列表后的 `:` 之后（可选，编译器可推断）
- 函数体在 `{}` 内


## Source: `references/language/function/1-函数定义/1-2-参数列表.md`

# 1.2 参数列表


- **非命名参数**：`p: T` — 位置参数，调用时无标签
- **命名参数**：`p!: T` — 定义时使用 `!` 后缀，调用时使用 `p: value` 形式（**调用不需要写`!`**）
- **带默认值的命名参数**：`p!: T = expr` — 仅命名参数可有默认值，使用 `=` 赋值
- **顺序规则**：非命名参数须在命名参数之前，命名参数后不能跟非命名参数
- **不可变性**：所有函数参数在函数体内不可变（不能重新赋值）
- **作用域**：参数名从定义处到函数体结束有效。在函数体内重新定义同名变量会报错
- **命名参数完整示例**：
  ```cangjie cjtest=syntax id=syntax-67a1fac9ca-1 form=stmt
  // 函数定义，其中 indent 是命名参数，默认值为 2
  func formatNumber(value: Int64, indent!: Int64 = 2): String {
      " " * indent + value.toString()
  }

  // 函数调用，命名参数 indent 使用前缀传值（注意不带 ! 符号）
  println(formatNumber(42, indent: 4))  // "    42"

  // 省略命名参数时使用默认值
  println(formatNumber(42))  // "  42"（indent 使用默认值 2）
  ```


## Source: `references/language/function/1-函数定义/1-3-返回类型.md`

# 1.3 返回类型


- 可**显式声明**或**省略**（编译器推断）
- 若已声明，所有 `return e` 表达式和函数体的最终表达式须为声明类型的子类型
- 若返回类型为 `Unit`，编译器自动在所有返回点插入 `return ()`
- 若推断失败，编译器报错


## Source: `references/language/function/1-函数定义/1-4-函数体.md`

# 1.4 函数体


- 包含变量定义、表达式、以及可选的嵌套函数定义
- **`return expr`**：终止执行，返回 `expr`（须匹配返回类型）
- **`return`**：等价于 `return ()`，须要求 `Unit` 返回类型
- **`return` 表达式的类型为 `Nothing`**
- **函数体类型** = 最后一项的类型：表达式 → 该表达式类型；变量定义/函数声明/空 → `Unit`

---


## Source: `references/language/basic_concepts/2-程序结构/2-2-变量定义.md`

# 2.2 变量定义


- 语法：`修饰符 名称: 类型 = 值`
- 修饰符：
  - `let`：不可变，只能赋值一次
  - `var`：可变，可多次赋值
  - `const`：编译时常量
- 类型标注可省略（当可推断时）
- 全局/静态变量须初始化
- 局部变量可延迟初始化，但使用前必须赋值

## 已验证反例

`let` 绑定不可重新赋值；以下完整程序应由 1.0.5 编译器拒绝。

```cangjie cjtest=compile id=language-let-reassignment-invalid exit=1
package example

main(): Int64 {
    let answer = 41
    answer = 42
    return answer
}
```

```text cjtest=expect for=language-let-reassignment-invalid stream=stderr match=contains
cannot assign to immutable value
```


## Source: `references/language/basic_concepts/3-表达式/3-2-if-条件判断.md`

# 3.2 if 条件判断


- 语法：`if (cond) { exprs1 } else { exprs2 }`，`cond` 是 `Bool` 类型表达式
- 支持 `else if` 链式使用
- 用作值时：有 `else` 时类型由上下文或各分支最小公共父类型决定，值为所执行分支最后一个表达式的值，无 `else` 时类型始终为 `Unit`
- 条件表达式可以用 `let pattern <- expr` 做模式匹配，详见[模式匹配](../../pattern_match/index.md)


## Source: `references/language/basic_concepts/3-表达式/3-3-while-do-while-循环.md`

# 3.3 while / do-while 循环


- 语法：`while (cond) { exprs }` 和 `do { exprs } while (cond)`，`cond` 是 `Bool` 类型表达式
- 返回类型始终为 `Unit`
- 条件表达式可以用 `let pattern <- expr` 做模式匹配


## Source: `references/language/basic_concepts/3-表达式/3-5-break-continue.md`

# 3.5 break / continue


- `break` 退出循环，`continue` 跳至下一次迭代
- 二者的类型均为 `Nothing`


## Source: `references/language/basic_data_type/1-整数类型/1-1-类型列表.md`

# 1.1 类型列表


- **有符号整数**：`Int8`、`Int16`、`Int32`、`Int64`、`IntNative`
- **无符号整数**：`UInt8`、`UInt16`、`UInt32`、`UInt64`、`UIntNative`
- 有符号 N 位范围：−2^(N−1) ~ 2^(N−1)−1；无符号 N 位范围：0 ~ 2^N−1
- `IntNative`/`UIntNative` 为平台相关宽度
- 默认整数类型为 `Int64`（无上下文时字面量默认推断为 `Int64`）


## Source: `references/language/basic_data_type/1-整数类型/1-2-整数字面量.md`

# 1.2 整数字面量


- **二进制**：`0b` 或 `0B` 前缀，例如 `0b00011000`
- **八进制**：`0o` 或 `0O` 前缀，例如 `0o30`
- **十进制**：无前缀，例如 `24`
- **十六进制**：`0x` 或 `0X` 前缀，例如 `0x18`
- 允许使用下划线 `_` 分隔：`0b0001_1000`
- 超出范围的值会导致编译错误


## Source: `references/language/basic_data_type/1-整数类型/1-5-整数支持的运算.md`

# 1.5 整数支持的运算


- 算术运算、位运算、关系运算、自增/自减（`++`/`--`）、复合赋值
- 可在整数类型之间转换、与浮点互转、转换为 `Rune`


## Source: `references/language/basic_data_type/11-基本运算符/11-2-算术运算符.md`

# 11.2 算术运算符


| 运算符 | 说明 | 操作数类型 |
|--------|------|------------|
| `-`（一元） | 取负 | 数值类型 |
| `+` | 加（也用于字符串拼接） | 相同数值类型 |
| `-` | 减 | 相同数值类型 |
| `*` | 乘 | 相同数值类型 |
| `/` | 除（整数除法向零截断） | 相同数值类型 |
| `%` | 取模 `a - b*(a/b)` | 仅整数 |
| `**` | 幂 | `Int64^UInt64→Int64`，`Float64^(Int64\|Float64)→Float64` |

- 溢出会被检测并报错（运行时错误）


## Source: `references/language/basic_data_type/11-基本运算符/11-4-关系运算符.md`

# 11.4 关系运算符


`==`、`!=`、`<`、`<=`、`>`、`>=`


## Source: `references/language/basic_data_type/11-基本运算符/11-5-逻辑运算符.md`

# 11.5 逻辑运算符


速查`!`：非（一元前缀）；`&&`：与；`||`：或。

| 运算符 | 说明 | 短路求值 |
|--------|------|----------|
| `!` | 非（一元前缀） | 不适用 |
| `&&` | 与 | 是：`false && x` → `false` |
| `\|\|` | 或 | 是：`true \|\| x` → `true` |


## Source: `references/language/basic_data_type/3-布尔类型.md`

# 3. 布尔类型


`Bool` 只有 `true` 和 `false` 两个值，用于条件表达式及逻辑运算。

- 类型：`Bool`
- 字面量：`true`、`false`
- 支持运算符：
  - 逻辑运算：`!`（非）、`&&`（与）、`||`（或）
  - 关系运算：`==`、`!=`
  - 复合赋值：`&&=`、`||=`

---


## Source: `references/language/package/3-程序入口/3-1-main-函数.md`

# 3.1 `main` 函数


- `main` 是入口点，每个根包最多一个
- 不能有访问修饰符
- 被导入包时不会被导入
- 参数：无参或 `Array<String>`
- 返回类型：`Unit` 或整数类型
- 定义时不使用 `func` 关键字

