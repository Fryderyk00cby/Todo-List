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
- **Work timer** — switch to the **Work** tab to track daily work sessions and duration
- Auto-save to `todos.dat` and `study.dat` (work records, plain text, hand-editable)
- Zero dependencies — single executable, copy and run

---

## Quick start

**Prerequisites:** Install [MinGW (gcc)](https://www.mingw-w64.org/) or [Visual Studio Build Tools](https://visualstudio.microsoft.com/visual-cpp-build-tools/).

```bat
build.bat
todo_list.exe
```

You can also double-click **`run.bat`** after building.

For detailed usage, data formats, and development notes, see the full documentation linked above.
