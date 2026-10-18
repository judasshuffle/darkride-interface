# Downloads a build-time SDK only. The viewer never downloads models or uploads frames.
[CmdletBinding()]
param([string]$SevenZip = 'C:\Program Files\7-Zip\7z.exe')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$destination = Join-Path $projectRoot 'build/native-camera-deps'
New-Item -ItemType Directory -Force $destination | Out-Null
$archive = Join-Path $destination 'opencv-4.10.0-windows.exe'
$expected = 'BFF38466091C313DAC21A0B73EEA8278316A89C1D434C6F0B10697E087670168'
if (!(Test-Path -LiteralPath $archive)) {
    Invoke-WebRequest 'https://github.com/opencv/opencv/releases/download/4.10.0/opencv-4.10.0-windows.exe' -OutFile $archive
}
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $expected) {
    throw 'OpenCV archive checksum mismatch. Remove the archive and retry.'
}
if (!(Test-Path -LiteralPath $SevenZip)) { throw 'Install 7-Zip or supply -SevenZip with the path to 7z.exe.' }
& $SevenZip x $archive "-o$destination" -y
if ($LASTEXITCODE -ne 0) { throw 'OpenCV extraction failed.' }
Write-Output "OpenCV_DIR=$destination/opencv/build"
