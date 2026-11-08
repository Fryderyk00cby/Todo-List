# Todo List

A lightweight native Windows todo app written in **C + Win32**. After building, you get a single `todo_list.exe` — no Python, .NET, or other runtime required.

**Other languages:** [中文](README.zh-CN.md)

---

## Features

- **Add, complete, and delete** tasks from a clean ListView interface
- **Optional due dates** with a date picker; overdue dates shown in red
- **Work timer** — switch to the **Focus time** tab to track daily work sessions and duration
- **Work history** — browse any day's total time and session records (time range, duration, topic)
- **Auto-save** to `todos.dat` and `study.dat` (work records, plain text, hand-editable)
- **Zero dependencies** — one `.exe`, copy the folder to move or back up your list
- **Keyboard-friendly** — Enter to add, Delete to remove, menu shortcuts included

---

## Personalize your space

The **Daylight** interface adds soft colors, rounded cards, spacious task rows, and visible hover / keyboard focus states. Open **Settings** with the gear button in the upper right, then choose a **Sage**, **Lavender**, or **Apricot** preview card under **Appearance**; the choice is saved beside the executable in `appearance.ini` and restored on restart.

Choose **English** or **简体中文** under **Settings → Language / 语言** to switch the interface immediately. The language is saved in `settings.ini` beside the executable and restored on restart (English by default). Task text and running focus sessions are preserved.

The overview shows pending tasks, completed / total tasks with a progress bar, and today's saved focus time (excluding a running session). **My tasks** holds your list; **Focus time** offers a dedicated timer card, with daily history and **Weekly rhythm** charts. Tab / Shift+Tab moves between controls, and Space activates focused buttons.

**Back** or **Esc** returns to the previous view. Settings sections and controls live in `src/settings.c` / `src/settings.h`, ready for additional preferences.

The appearance module lives in `src/appearance.c` and `src/appearance.h`. Task and session data formats remain compatible, and no new runtime dependencies are required.

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
| Add a task | Type in the input box, optionally check **Due** and pick a date, then click **+ Add task** or press **Enter** |
| Mark complete | Click the checkbox next to a task |
| Select a task | Click the task row (click again to deselect) |
| Edit a task | Double-click the row, then **Enter** to confirm / **Esc** to cancel |
| Delete a task | Select a row, then click **Delete task** or press **Delete** |

### Keyboard shortcuts

| Shortcut | Action |
|----------|--------|
| **Enter** (while input is focused) | Add task |
| **Delete** (while list is focused) | Delete selected task |
| **Task → Add Task** | Same as Add button |
| **Task → Delete Selected** | Same as Delete button |

### Visual cues

- **Overview** — shows pending tasks, completed / total tasks, and today's saved focus time
- **Due column** — `-` when no date; **red** when overdue and not yet completed
- **Completed tasks** — gray text with strikethrough
- **Empty list** — placeholder message: *A fresh start. Add your first small step above.*

---

## Work timer

Use the **My tasks** / **Focus time** tabs at the top to switch views.

### Timer actions

| Action | How |
|--------|-----|
| Switch view | Click **My tasks** or **Focus time** at the top |
| Start working | Enter what you're working on, click **Start focus** or press **Enter** |
| Stop timer | Click **Finish session** (same button toggles); sessions under 1 minute are not saved |
| View history | Click **View history** |

While recording, a large `HH:MM:SS` clock counts up from zero. Before you start, it shows the current system time. Stopping under 1 minute does not save; 1 minute or longer is saved automatically.

### History

| Action | How |
|--------|-----|
| Back to timer | Click **Back** |
| Change date | Use **Prev** / **Next** or the date picker |
| Change week | In **Weekly rhythm**, use **Prev** / **Next** (future weeks are not available) |
| View details | List shows time range (to the minute), duration (`HH:MM:SS`), and topic |
| Delete a record | Select a row, then **Delete Selected** or **Delete** |
| Week summary | Click **Weekly rhythm** for a bar chart of daily totals this week (Mon–Sun) |

The header summarizes **total work time** and session count for the selected day.

### Work data format

Each line in `study.dat` (UTF-8, filename kept for compatibility) is one session:

```text
<date>|<start>|<end>|<seconds>|<topic>
```

Sessions that cross midnight are split into one record per calendar day (the first segment ends at `23:59:59`, the next starts at `00:00:00`).

**Examples:**

```text
2026-06-17|09:30:00|10:45:00|4500|Win32 API
2026-06-17|14:00:00|15:30:00|5400|Data structures
2026-06-17|23:30:00|23:59:59|1800|Late work
2026-06-18|00:00:00|00:30:00|1800|Late work
```

---

## Data storage (todos)

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
| `text` | Task description (up to 255 UTF-8 bytes; Chinese characters typically use 3 bytes each) |

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

- The file is encoded as **UTF-8**; legacy ANSI text is converted on load when possible
- Saves write to a temporary file before replacing the previous file
- Pipe characters (`|`) in task text are replaced with spaces on save
- Newlines in task text are stripped
- You can edit `todos.dat` in a text editor; restart the app to reload

---

## Project structure

```text
todo_list/
├── src/
│   ├── main.c              # Win32 UI, navigation, task and focus views
│   ├── todo.c / todo.h     # Task storage, load/save, sorting and deadlines
│   ├── study.c / study.h   # Focus records and daily/weekly statistics
│   ├── appearance.c / .h   # Theme palettes and shared drawing
│   ├── settings.c / .h     # Settings controls, layout and drawing
│   ├── language.c / .h     # Language selection, persistence and lookup
│   └── language_strings.h # English / Simplified Chinese text catalog
├── app.manifest            # Common Controls v6 + DPI awareness
├── app.manifest.rc         # Manifest and icon resources
├── app.ico                 # Application icon
├── make_icon.ps1           # Icon generation script
├── build.bat               # Build script (gcc first, then cl)
├── run.bat                 # Launch helper
├── todo_list.exe           # Build output (gitignored)
├── todos.dat / study.dat   # Tasks and focus records (gitignored)
└── appearance.ini / settings.ini # Theme and language preferences (gitignored)
```

---

## Build from source

After changing source code, close the running app and run `build.bat`.

`build.bat` tries **gcc** (MinGW) first, then **cl** (Visual Studio). On success it prints which compiler was used.

**Manual MinGW build (including manifest and icon resources):**

```bat
windres -i app.manifest.rc -o app.manifest.o
gcc -O2 -Wall -mwindows -o todo_list.exe src/main.c src/todo.c src/study.c src/appearance.c src/settings.c src/language.c app.manifest.o -lcomctl32 -lgdi32 -luser32
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

Contributions and new features are welcome. Keep UI, storage, appearance, settings, and language responsibilities in their respective modules. Add interface text to `language_strings.h`.
