# Todo List

A lightweight native Windows todo app written in **C + Win32**. After building, you get a single `todo_list.exe` — no Python, .NET, or other runtime required.

---

## Documentation

| Language | Link |
|----------|------|
| English (full guide) | [README.en.md](README.en.md) |
| 中文 | [README.zh-CN.md](README.zh-CN.md) |

---

## Features

- Add, complete, and delete tasks
- Optional due dates with overdue highlighting
- **Work timer** — switch to the **Focus time** tab to track daily work sessions and duration
- Auto-save to `todos.dat` and `study.dat` (work records, plain text, hand-editable)
- Zero dependencies — single executable, copy and run

---

## Settings

Use the gear button to choose a **Sage**, **Lavender**, or **Apricot** theme and switch between **English** and **简体中文** under **Language / 语言**. Both preferences apply instantly and are remembered on restart.

See the [English guide](README.en.md#personalize-your-space) or [中文指南](README.zh-CN.md#个性化外观) for details.

---

## Quick start

**Prerequisites:** Install [MinGW (gcc)](https://www.mingw-w64.org/) or [Visual Studio Build Tools](https://visualstudio.microsoft.com/visual-cpp-build-tools/).

```bat
build.bat
todo_list.exe
```

You can also double-click **`run.bat`** after building.

For detailed usage, data formats, and development notes, see the full documentation linked above.
