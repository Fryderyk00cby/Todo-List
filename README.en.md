# Todo List

A lightweight native Windows todo app. Double-click `todo_list.exe` to run — no Python, .NET, or other runtime required.

## Quick start

1. Build the app (first time only):

   ```bat
   build.bat
   ```

2. Double-click **`todo_list.exe`** (or run `run.bat`).
3. Optional: right-click `todo_list.exe` → **Send to** → **Desktop (create shortcut)**.

## Usage

| Action | How |
|--------|-----|
| Add a task | Type in the input box, then click **Add** or press **Enter** |
| Mark complete | Click the checkbox next to a task |
| Delete a task | Click a task row (or its checkbox), then click **Delete Selected** or press **Delete** |

Tasks are saved automatically to `todos.dat` in the same folder as the executable. Copy the folder to back up or move your list.

## Data format

`todos.dat` is a plain-text file:

```
0|Buy groceries
1|Finish report
```

- `0` = pending
- `1` = completed

## Project structure

```
todo_list/
  src/
    main.c          # Win32 GUI
    todo.c          # Task storage & CRUD
    todo.h
  build.bat         # Compile (gcc or cl)
  run.bat           # Launch helper
  todo_list.exe     # Built locally (gitignored)
  todos.dat         # Your tasks (gitignored)
```

## Rebuild

Only needed after changing source code:

```bat
build.bat
```

`build.bat` tries **gcc** (MinGW) first, then **cl** (Visual Studio Build Tools).

## Development

- **Language:** C (C99)
- **Platform:** Windows 10/11
- **GUI:** Win32 API + Common Controls (ListView)

Contributions and new features are welcome. Keep the app dependency-free: a single `.exe` with no extra runtime.
