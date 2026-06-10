@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0"

echo Building todo_list.exe...

where gcc >nul 2>&1
if !ERRORLEVEL! EQU 0 (
    gcc -O2 -Wall -mwindows -o todo_list.exe src/main.c src/todo.c -lcomctl32 -lgdi32 -luser32
    if !ERRORLEVEL! EQU 0 (
        echo Build succeeded with gcc.
        exit /b 0
    )
    echo gcc build failed.
)

where cl >nul 2>&1
if !ERRORLEVEL! EQU 0 (
    cl /nologo /O2 /W3 /Fe:todo_list.exe src\main.c src\todo.c user32.lib gdi32.lib comctl32.lib /link /SUBSYSTEM:WINDOWS
    if !ERRORLEVEL! EQU 0 (
        echo Build succeeded with cl.
        if exist main.obj del main.obj
        if exist todo.obj del todo.obj
        exit /b 0
    )
    echo cl build failed.
)

echo No C compiler found.
echo If todo_list.exe already exists, you can still run it directly.
exit /b 1
