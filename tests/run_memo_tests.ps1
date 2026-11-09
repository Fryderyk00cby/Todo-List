$ErrorActionPreference = 'Stop'
$repoDirectory = Split-Path -Parent $PSScriptRoot
$testDirectory = Join-Path $repoDirectory ('.ui-check/memo-test-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testDirectory | Out-Null
Push-Location $repoDirectory
try {
    $testExecutable = Join-Path $testDirectory 'test.exe'
    & gcc -O0 -g -Wall -Wextra -Werror -Wno-sign-compare -o $testExecutable `
        tests/memo_test.c src/study.c src/appearance.c src/language.c -lcomctl32 -lgdi32 -luser32
    if ($LASTEXITCODE -ne 0) { throw 'Memo test compilation failed.' }
    & $testExecutable
    if ($LASTEXITCODE -ne 0) { throw 'Memo tests failed.' }
} finally {
    Pop-Location
}
