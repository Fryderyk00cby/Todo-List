# Factory reset checks

## Calendar memo checks

Run `./tests/run_memo_tests.ps1` on Windows with MinGW. Tests use a fresh `.ui-check/memo-test-*` directory and synthetic notes only. They cover Unicode and multiline persistence, maximum-length notes, invalid data protection, locked-file save failures, date/month navigation, leap days, memory reset, and text measurements for both languages in all three themes at 100%, 150%, and 200% scaling. Factory reset integration tests also cover memo files and saving before switching tabs.

Run on Windows with MinGW (`gcc` and `windres`) from the repository root:

```powershell
.\tests\run_factory_reset_tests.ps1
```

Each run builds in a fresh `.ui-check/reset-test-*` directory. The test executable refuses to run outside a directory with that prefix and uses only synthetic records beside that executable.

Checks cover successful and repeated reset, missing files, unrelated `.dat` preservation, read-only files, rollback when a file is locked, existing recovery-file protection, default preferences, active-session discard, UI state clearing, and persistence after shutdown/reload.

The confirmation dialog and cancellation path are checked manually in an isolated preview copy; no reset is performed against the user's records during development.

## Manual focus entry checks

```powershell
New-Item -ItemType Directory -Force .ui-check | Out-Null
gcc -O0 -g -Wall -Wextra -Werror -o .ui-check/manual_focus_test.exe tests/manual_focus_test.c src/study.c
.\.ui-check\manual_focus_test.exe
```

Covers overnight and leap-day splitting, reversed/empty/future intervals, blank tasks, the one-minute minimum and 31-day maximum, midnight endings, daily totals, and saving/reloading. Persistence uses a unique temporary file, which is removed after the test.

## Manual focus layout checks

```powershell
New-Item -ItemType Directory -Force .ui-check | Out-Null
gcc -O0 -g -Wall -Wextra -Werror -Wno-sign-compare -o .ui-check/manual_focus_layout_test.exe tests/manual_focus_layout_test.c src/todo.c src/study.c src/memo.c src/appearance.c src/settings.c src/language.c src/factory_reset.c -lcomctl32 -lgdi32 -luser32
.\.ui-check\manual_focus_layout_test.exe
```

Creates isolated Win32 controls without opening the app's record files. Uses GDI text measurements to check English and Chinese labels at 100%, 150%, and 200% scaling, and verifies that the date/time fields display the full custom format with enough width for the spin buttons.
