# Factory reset checks

Run on Windows with MinGW (`gcc` and `windres`) from the repository root:

```powershell
.\tests\run_factory_reset_tests.ps1
```

Each run builds in a fresh `.ui-check/reset-test-*` directory. The test executable refuses to run outside a directory with that prefix and uses only synthetic records beside that executable.

Checks cover successful and repeated reset, missing files, unrelated `.dat` preservation, read-only files, rollback when a file is locked, existing recovery-file protection, default preferences, active-session discard, UI state clearing, and persistence after shutdown/reload.

The confirmation dialog and cancellation path are checked manually in an isolated preview copy; no reset is performed against the user's records during development.
