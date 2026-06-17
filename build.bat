@echo off
setlocal EnableDelayedExpansion
cd /d "%~dp0"

echo Building todo_list.exe...

where gcc >nul 2>&1
if !ERRORLEVEL! EQU 0 (
    if exist app.manifest.rc (
        windres -i app.manifest.rc -o app.manifest.o >nul 2>&1
        if !ERRORLEVEL! EQU 0 (
            gcc -O2 -Wall -mwindows -o todo_list.exe src/main.c src/todo.c src/study.c app.manifest.o -lcomctl32 -lgdi32 -luser32
        ) else (
            gcc -O2 -Wall -mwindows -o todo_list.exe src/main.c src/todo.c src/study.c -lcomctl32 -lgdi32 -luser32
        )
    ) else (
        gcc -O2 -Wall -mwindows -o todo_list.exe src/main.c src/todo.c src/study.c -lcomctl32 -lgdi32 -luser32
    )
    if !ERRORLEVEL! EQU 0 (
        echo Build succeeded with gcc.
        exit /b 0
    )
    echo gcc build failed.
)

where cl >nul 2>&1
if !ERRORLEVEL! EQU 0 (
    set "RES_FILE="
    where rc >nul 2>&1
    if !ERRORLEVEL! EQU 0 (
        rc /nologo /fo app.res app.manifest.rc >nul 2>&1
        if !ERRORLEVEL! EQU 0 set "RES_FILE=app.res"
    )
    cl /nologo /O2 /W3 /Fe:todo_list.exe src\main.c src\todo.c src\study.c !RES_FILE! user32.lib gdi32.lib comctl32.lib /link /SUBSYSTEM:WINDOWS
    if !ERRORLEVEL! EQU 0 (
        echo Build succeeded with cl.
        if exist main.obj del main.obj
        if exist todo.obj del todo.obj
        if exist study.obj del study.obj
        if exist app.res del app.res
        exit /b 0
    )
    echo cl build failed.
)

echo No C compiler found.
echo If todo_list.exe already exists, you can still run it directly.
exit /b 1
