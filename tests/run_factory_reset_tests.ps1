$ErrorActionPreference = 'Stop'
$repoDirectory = Split-Path -Parent $PSScriptRoot
$testDirectory = Join-Path $repoDirectory ('.ui-check/reset-test-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testDirectory | Out-Null
Push-Location $repoDirectory
try {
    $resourcePath = Join-Path $testDirectory 'app.manifest.o'
    & windres -i app.manifest.rc -o $resourcePath
    if ($LASTEXITCODE -ne 0) { throw 'Resource compilation failed.' }
    $testExecutable = Join-Path $testDirectory 'test.exe'
    & gcc -O0 -g -Wall -Wextra -Werror -Wno-sign-compare -o $testExecutable `
        tests/factory_reset_test.c src/todo.c src/study.c src/appearance.c `
        src/settings.c src/language.c src/factory_reset.c $resourcePath `
        -lcomctl32 -lgdi32 -luser32
    if ($LASTEXITCODE -ne 0) { throw 'Test compilation failed.' }
    & $testExecutable
    if ($LASTEXITCODE -ne 0) { throw 'Factory reset tests failed.' }
} finally {
    Pop-Location
}
