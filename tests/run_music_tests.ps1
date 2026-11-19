$ErrorActionPreference = 'Stop'
$repoDirectory = Split-Path -Parent $PSScriptRoot
$testDirectory = Join-Path $repoDirectory ('.ui-check/music-test-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $testDirectory | Out-Null
# One second of silence: PCM mono, 8 kHz, 16 bit.
foreach ($name in @('first', 'second')) {
    $stream = [IO.File]::Create((Join-Path $testDirectory ($name + '.wav')))
    $writer = New-Object IO.BinaryWriter($stream)
    try {
        $writer.Write([Text.Encoding]::ASCII.GetBytes('RIFF')); $writer.Write([int]16036)
        $writer.Write([Text.Encoding]::ASCII.GetBytes('WAVEfmt ')); $writer.Write([int]16)
        $writer.Write([int16]1); $writer.Write([int16]1); $writer.Write([int]8000)
        $writer.Write([int]16000); $writer.Write([int16]2); $writer.Write([int16]16)
        $writer.Write([Text.Encoding]::ASCII.GetBytes('data')); $writer.Write([int]16000)
        $writer.Write((New-Object byte[] 16000))
    } finally { $writer.Dispose() }
}
Push-Location $repoDirectory
try {
    $executable = Join-Path $testDirectory 'test.exe'
    & gcc -O0 -g -Wall -Wextra -Werror -o $executable tests/music_test.c src/appearance.c src/language.c -lcomctl32 -lgdi32 -luser32 -lwinmm -lcomdlg32 -lole32 -loleaut32 -luuid
    if ($LASTEXITCODE -ne 0) { throw 'Music test compilation failed.' }
    & $executable
    if ($LASTEXITCODE -ne 0) { throw 'Music tests failed.' }
} finally { Pop-Location }
