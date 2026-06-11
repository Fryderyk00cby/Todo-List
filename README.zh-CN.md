# Todo List

轻量级 Windows 原生待办工具，使用 **C + Win32** 编写。编译后得到单个 `todo_list.exe`，无需安装 Python、.NET 或其他运行环境。

**其他语言：** [English](README.en.md) · [Hub](README.md)

---

## 功能特性

- **增删改查** — ListView 列表界面，支持添加、完成、删除任务
- **截止日期** — 可选 Due 日期选择器；未完成且已过期时在 Due 列显示为红色
- **自动保存** — 写入程序同目录下的 `todos.dat`（纯文本，可手动编辑）
- **零依赖** — 单个 `.exe`，拷贝整个文件夹即可备份或迁移
- **快捷键** — Enter 添加、Delete 删除，菜单中亦有对应项

---

## 快速开始

### 环境要求

在 Windows 上需要 **任选其一** C 编译器：

| 编译器 | 常见安装方式 |
|--------|--------------|
| **gcc**（MinGW-w64） | [MinGW-w64](https://www.mingw-w64.org/) 或 MSYS2（`pacman -S mingw-w64-x86_64-gcc`） |
| **cl**（MSVC） | [Visual Studio Build Tools](https://visualstudio.microsoft.com/visual-cpp-build-tools/)，勾选「使用 C++ 的桌面开发」 |

### 编译与运行

```bat
build.bat
todo_list.exe
```

编译完成后也可双击 **`run.bat`** 启动。

**可选：** 右键 `todo_list.exe` → **发送到** → **桌面快捷方式**。

---

## 使用方法

### 基本操作

| 操作 | 方式 |
|------|------|
| 添加任务 | 在输入框输入文字，可选勾选 **Due** 并选择日期，点击 **Add** 或按 **Enter** |
| 标记完成 | 点击任务前的复选框 |
| 选中任务 | 点击任务行（再次点击可取消选中） |
| 编辑任务 | 双击任务行，修改后按 **Enter** 确认 / **Esc** 取消 |
| 删除任务 | 先选中一行，再点 **Delete Selected** 或按 **Delete** |

### 快捷键

| 快捷键 | 作用 |
|--------|------|
| **Enter**（输入框聚焦时） | 添加任务 |
| **Delete**（列表聚焦时） | 删除当前选中任务 |
| **任务 → Add Task** | 同 Add 按钮 |
| **任务 → Delete Selected** | 同 Delete 按钮 |

### 界面说明

- **顶部标题栏** — 右侧显示未完成（pending）任务数量
- **Due 列** — 无日期显示 `-`；未完成且已过期显示为 **红色**
- **已完成任务** — 灰色文字 + 删除线
- **空列表** — 显示提示：*No tasks yet. Add one above.*

---

## 数据存储

任务会在每次变更时自动写入 **`todos.dat`**（与 `todo_list.exe` 同目录），退出程序时也会再次保存。

### 文件格式

每行一条任务：

```text
<状态>|<截止日期>|<任务内容>
```

| 字段 | 说明 |
|------|------|
| `状态` | `0` = 未完成，`1` = 已完成 |
| `截止日期` | `YYYY-MM-DD` 格式，无则留空 |
| `任务内容` | 任务描述（最多 256 字符） |

**示例：**

```text
0||Buy groceries
0|2026-06-15|Finish report
1|2026-06-10|Done task
```

**旧格式**（加载时仍兼容）：

```text
0|无截止日期的任务名
```

**说明：**

- 文件编码为 **UTF-8**；旧版按系统 ANSI（GBK）保存的文件会在加载时自动迁移
- 保存采用「临时文件 + 原子替换」，意外退出不会损坏数据
- 任务文字中的竖线 `|` 保存时会被替换为空格
- 任务文字中的换行会被去掉
- 可用记事本直接编辑 `todos.dat`，重启程序后生效

---

## 项目结构

```text
todo_list/
├── src/
│   ├── main.c          # Win32 界面（窗口、ListView、控件）
│   ├── todo.c          # 任务存储、读写、增删改
│   └── todo.h          # 公共类型与 API
├── app.manifest        # Common Controls v6 + DPI 感知
├── app.manifest.rc     # 清单资源（MinGW windres）
├── build.bat           # 编译脚本（优先 gcc，其次 cl）
├── run.bat             # 启动脚本
├── todo_list.exe       # 编译产物（已 gitignore）
└── todos.dat           # 任务数据（已 gitignore）
```

### 模块分工

| 文件 | 职责 |
|------|------|
| `main.c` | 窗口布局、ListView 渲染、用户交互、自定义绘制（颜色、删除线） |
| `todo.c` | 内存中的任务列表、文件 I/O、日期校验、过期判断 |

---

## 从源码编译

仅在修改源码后需要重新编译：

```bat
build.bat
```

`build.bat` 会优先尝试 **gcc**（MinGW），失败则尝试 **cl**（Visual Studio）。成功时会提示使用的编译器。

**gcc 等价命令：**

```bat
gcc -O2 -Wall -mwindows -o todo_list.exe src/main.c src/todo.c -lcomctl32 -lgdi32 -luser32
```

**未找到编译器时：** 脚本会报错退出。若之前已成功编译过，现有的 `todo_list.exe` 仍可正常运行。

---

## 开发说明

| 项目 | 说明 |
|------|------|
| 语言 | C（C99） |
| 平台 | Windows 10 / 11 |
| 界面 | Win32 API + Common Controls（ListView、DateTimePicker） |
| 清单 | Common Controls 6.0，Per-Monitor DPI 感知 |

**设计原则：** 保持零依赖 — 最终仍是一个可独立运行的 `.exe`，不引入额外运行时或安装程序。

欢迎继续添加功能。新增特性时，建议沿用现有的 `todo.c` / `main.c` 分层，避免引入新的第三方依赖。
