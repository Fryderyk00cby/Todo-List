# Todo List

轻量级 Windows 原生待办工具，使用 C + Win32 编写。双击 `todo_list.exe` 即可运行，无需安装 Python、.NET 等运行环境。

## 快速开始

1. 首次使用先编译：

   ```bat
   build.bat
   ```

2. 双击 **`todo_list.exe`**（或运行 `run.bat`）。
3. 可选：右键 `todo_list.exe` → **发送到** → **桌面快捷方式**。

## 使用方法

| 操作 | 方式 |
|------|------|
| 添加任务 | 在输入框输入文字，点击 **Add** 或按 **Enter** |
| 标记完成 | 点击任务前的复选框 |
| 删除任务 | 先点击任务行（或复选框）选中，再点 **Delete Selected** 或按 **Delete** |

任务会自动保存到程序同目录下的 `todos.dat`。拷贝整个文件夹即可备份或迁移。

## 数据格式

`todos.dat` 为纯文本文件：

```
0|Buy groceries
1|Finish report
```

- `0` = 未完成
- `1` = 已完成

## 项目结构

```
todo_list/
  src/
    main.c          # Win32 界面
    todo.c          # 任务存储与增删改
    todo.h
  build.bat         # 编译脚本（gcc 或 cl）
  run.bat           # 启动脚本
  todo_list.exe     # 本地编译产物（已 gitignore）
  todos.dat         # 你的任务数据（已 gitignore）
```

## 重新编译

仅在修改源码后需要：

```bat
build.bat
```

`build.bat` 会优先尝试 **gcc**（MinGW），其次 **cl**（Visual Studio Build Tools）。

## 开发说明

- **语言：** C（C99）
- **平台：** Windows 10/11
- **界面：** Win32 API + Common Controls（ListView）

欢迎持续添加功能。请保持零依赖：最终仍是一个可独立运行的 `.exe`，不引入额外运行时。
