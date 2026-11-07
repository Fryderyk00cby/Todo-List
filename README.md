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

## Personalize your space

The **Daylight** interface adds soft colors, rounded cards, spacious task rows, and visible hover / keyboard focus states. Open **Settings** with the gear button in the upper right, then choose a **Sage**, **Lavender**, or **Apricot** preview card under **Appearance**; the choice is saved beside the executable in `appearance.ini` and restored on restart.

The overview shows pending tasks, completed / total tasks with a progress bar, and today's saved focus time (excluding a running session). **My tasks** holds your list; **Focus time** offers a dedicated timer card, with daily history and **Weekly rhythm** charts. Tab / Shift+Tab moves between controls, and Space activates focused buttons.

**Back** or **Esc** returns to the previous view. Settings sections and controls live in `src/settings.c` / `src/settings.h`, ready for additional preferences.

The appearance module lives in `src/appearance.c` and `src/appearance.h`. Task and session data formats remain compatible, and no new runtime dependencies are required.

---

## Quick start

**Prerequisites:** Install [MinGW (gcc)](https://www.mingw-w64.org/) or [Visual Studio Build Tools](https://visualstudio.microsoft.com/visual-cpp-build-tools/).

```bat
build.bat
todo_list.exe
```

You can also double-click **`run.bat`** after building.

For detailed usage, data formats, and development notes, see the full documentation linked above.
