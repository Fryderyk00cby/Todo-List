Add-Type -AssemblyName System.Drawing

$sizes = @(16, 32, 48, 256)
$pngs = @()

foreach ($s in $sizes) {
    $bmp = New-Object System.Drawing.Bitmap($s, $s)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.Clear([System.Drawing.Color]::Transparent)

    # Rounded-square background (Fluent-ish blue)
    $r = [Math]::Max(2, [int]($s * 0.22))
    $d = $r * 2
    $w = $s - 1
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $path.AddArc(0, 0, $d, $d, 180, 90)
    $path.AddArc($w - $d, 0, $d, $d, 270, 90)
    $path.AddArc($w - $d, $w - $d, $d, $d, 0, 90)
    $path.AddArc(0, $w - $d, $d, $d, 90, 90)
    $path.CloseFigure()
    $brush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(255, 0, 120, 212))
    $g.FillPath($brush, $path)

    # White checkmark
    $penWidth = [Math]::Max(2.0, $s * 0.11)
    $pen = New-Object System.Drawing.Pen([System.Drawing.Color]::White, $penWidth)
    $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
    $pts = @(
        (New-Object System.Drawing.PointF(($s * 0.27), ($s * 0.53))),
        (New-Object System.Drawing.PointF(($s * 0.44), ($s * 0.70))),
        (New-Object System.Drawing.PointF(($s * 0.74), ($s * 0.32)))
    )
    $g.DrawLines($pen, $pts)

    $g.Dispose()
    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    $pngs += ,@($s, $ms.ToArray())
}

# Assemble ICO container (PNG-compressed entries)
$count = $pngs.Count
$out = New-Object System.IO.MemoryStream
$bw = New-Object System.IO.BinaryWriter($out)
$bw.Write([UInt16]0)        # reserved
$bw.Write([UInt16]1)        # type: icon
$bw.Write([UInt16]$count)

$offset = 6 + 16 * $count
foreach ($entry in $pngs) {
    $s = $entry[0]
    $data = $entry[1]
    $dim = if ($s -ge 256) { 0 } else { $s }
    $bw.Write([Byte]$dim)               # width
    $bw.Write([Byte]$dim)               # height
    $bw.Write([Byte]0)                  # palette colors
    $bw.Write([Byte]0)                  # reserved
    $bw.Write([UInt16]1)                # planes
    $bw.Write([UInt16]32)               # bit depth
    $bw.Write([UInt32]$data.Length)     # data size
    $bw.Write([UInt32]$offset)          # data offset
    $offset += $data.Length
}
foreach ($entry in $pngs) {
    $bw.Write($entry[1])
}

[System.IO.File]::WriteAllBytes((Join-Path $PSScriptRoot 'app.ico'), $out.ToArray())
$bw.Dispose()
Write-Host "app.ico written ($count sizes)"
