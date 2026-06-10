# Todo List

A lightweight native Windows todo app written in **C + Win32**. After building, you get a single `todo_list.exe` — no Python, .NET, or other runtime required.

**Other languages:** [中文](README.zh-CN.md) · [Hub](README.md)

---

## Features

- **Add, complete, and delete** tasks from a clean ListView interface
- **Optional due dates** with a date picker; overdue dates shown in red
- **Auto-save** to `todos.dat` next to the executable (plain text, hand-editable)
- **Zero dependencies** — one `.exe`, copy the folder to move or back up your list
- **Keyboard-friendly** — Enter to add, Delete to remove, menu shortcuts included

---

## Quick start

### Prerequisites

You need **one** of these C compilers on Windows:

| Compiler | Typical install |
|----------|-----------------|
| **gcc** (MinGW-w64) | [MinGW-w64](https://www.mingw-w64.org/) or MSYS2 (`pacman -S mingw-w64-x86_64-gcc`) |
| **cl** (MSVC) | [Visual Studio Build Tools](https://visualstudio.microsoft.com/visual-cpp-build-tools/) with "Desktop development with C++" |

### Build and run

```bat
build.bat
todo_list.exe
```

Or double-click **`run.bat`** after building.

**Optional:** Right-click `todo_list.exe` → **Send to** → **Desktop (create shortcut)**.

---

## Usage

### Basic actions

| Action | How |
|--------|-----|
| Add a task | Type in the input box, optionally check **Due** and pick a date, then click **Add** or press **Enter** |
| Mark complete | Click the checkbox next to a task |
| Select a task | Click the task row (click again to deselect) |
| Delete a task | Select a row, then click **Delete Selected** or press **Delete** |

### Keyboard shortcuts

| Shortcut | Action |
|----------|--------|
| **Enter** (while input is focused) | Add task |
| **Delete** (while list is focused) | Delete selected task |
| **Task → Add Task** | Same as Add button |
| **Task → Delete Selected** | Same as Delete button |

### Visual cues

- **Header** — shows the number of pending (incomplete) tasks
- **Due column** — `-` when no date; **red** when overdue and not yet completed
- **Completed tasks** — gray text with strikethrough
- **Empty list** — placeholder message: *No tasks yet. Add one above.*

---

## Data storage

Tasks are saved automatically to **`todos.dat`** in the same directory as `todo_list.exe`. The file is rewritten on every change and again when the app exits.

### File format

Each line is one task:

```text
<status>|<due_date>|<text>
```

| Field | Description |
|-------|-------------|
| `status` | `0` = pending, `1` = completed |
| `due_date` | `YYYY-MM-DD`, or empty if none |
| `text` | Task description (max 256 characters) |

**Examples:**

```text
0||Buy groceries
0|2026-06-15|Finish report
1|2026-06-10|Done task
```

**Legacy format** (still supported on load):

```text
0|Task name without due date
```

**Notes:**

- Pipe characters (`|`) in task text are replaced with spaces on save
- Newlines in task text are stripped
- You can edit `todos.dat` in a text editor; restart the app to reload

---

## Project structure

```text
todo_list/
├── src/
│   ├── main.c          # Win32 GUI (window, ListView, controls)
│   ├── todo.c          # Task storage, load/save, CRUD
│   └── todo.h          # Shared types and API
├── app.manifest        # Common Controls v6 + DPI awareness
├── app.manifest.rc     # Manifest resource (MinGW windres)
├── build.bat           # Build script (gcc first, then cl)
├── run.bat             # Launch helper
├── todo_list.exe       # Build output (gitignored)
└── todos.dat           # Your tasks (gitignored)
```

### Module overview

| File | Responsibility |
|------|----------------|
| `main.c` | Window layout, ListView rendering, user input, custom draw (colors, strikethrough) |
| `todo.c` | In-memory list, file I/O, deadline validation, overdue check |

---

## Build from source

Only needed after changing source code:

```bat
build.bat
```

`build.bat` tries **gcc** (MinGW) first, then **cl** (Visual Studio). On success it prints which compiler was used.

**gcc command (equivalent):**

```bat
gcc -O2 -Wall -mwindows -o todo_list.exe src/main.c src/todo.c -lcomctl32 -lgdi32 -luser32
```

**If no compiler is found:** the script exits with an error. If `todo_list.exe` already exists from a previous build, you can still run it.

---

## Development

| Item | Value |
|------|-------|
| Language | C (C99) |
| Platform | Windows 10 / 11 |
| GUI | Win32 API + Common Controls (ListView, DateTimePicker) |
| Manifest | Common Controls 6.0, per-monitor DPI aware |

**Design goal:** keep the app dependency-free — a single portable `.exe` with no extra runtime or installer.

Contributions and new features are welcome. When adding features, prefer extending the existing `todo.c` / `main.c` split rather than introducing new dependencies.
