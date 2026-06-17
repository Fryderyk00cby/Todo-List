# Todo List

轻量级 Windows 原生待办工具 · Lightweight native Windows todo app

使用 **C + Win32** 编写，编译后得到单个 `todo_list.exe`，无需 Python、.NET 或其他运行时。

---

## 文档 · Documentation

| 语言 | 链接 |
|------|------|
| 中文 | [README.zh-CN.md](README.zh-CN.md) |
| English | [README.en.md](README.en.md) |

---

## 功能概览 · Features

- 添加、完成、删除任务
- 可选截止日期，过期项高亮显示
- **工作计时** — 切换到 Work 页，记录每日工作时段与时长
- 自动保存至 `todos.dat` / `study.dat`（工作记录，纯文本，可手动编辑）
- 零依赖：单个可执行文件，拷贝即用

---

## 快速开始 · Quick start

**前提：** 已安装 [MinGW (gcc)](https://www.mingw-w64.org/) 或 [Visual Studio Build Tools](https://visualstudio.microsoft.com/visual-cpp-build-tools/) 之一。

```bat
build.bat
todo_list.exe
```

也可在编译后双击 `run.bat` 启动。

详细用法、数据格式与开发说明见上方完整文档。
