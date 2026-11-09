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

- **Calendar memos** — open **Memo** to browse months and keep important notes for each day. Notes auto-save; dots mark dates with notes.
- **Manual focus entry** ? use **Add past focus** to record time spent away from your computer.
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

## Factory reset

Open **Settings > Data & reset > Factory reset**. The confirmation defaults to **No**. Confirming permanently clears `todos.dat`, `study.dat` and `memos.dat` (including their `.tmp` files), discards any running session and unsaved input, and removes `appearance.ini` / `settings.ini` to restore Sage and English. Other files are untouched. Empty record files may be recreated when the app exits.

If a file is locked or read-only, the reset fails without clearing the current state. Any incomplete rollback or backup cleanup is reported with instructions for the `.reset-backup` files.

## Quick start

**Prerequisites:** Install [MinGW (gcc)](https://www.mingw-w64.org/) or [Visual Studio Build Tools](https://visualstudio.microsoft.com/visual-cpp-build-tools/).

```bat
build.bat
todo_list.exe
```

You can also double-click **`run.bat`** after building.

For detailed usage, data formats, and development notes, see the full documentation linked above.
