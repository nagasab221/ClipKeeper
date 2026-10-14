# Build ClipKeeper Release (no console), bundle Qt with windeployqt, and produce
# the Inno Setup installer.
#   .\package.ps1            -> dev identity (installs alongside the Python app)
#   .\package.ps1 -DropIn    -> shared "ClipKeeper" identity (true drop-in)
param([switch]$DropIn)

# Note: no ErrorActionPreference=Stop — the build tools write harmless notes to
# stderr (e.g. a vswhere probe), which Stop would misread as fatal. Real failures
# are caught by the explicit $LASTEXITCODE checks below.
$root  = $PSScriptRoot
$qt    = "C:\Qt\6.11.1\msvc2022_64"
$iscc  = "C:\Users\fabri\AppData\Local\Programs\Inno Setup 6\ISCC.exe"
$stage = Join-Path $root "dist\ClipKeeper"

Write-Host "== 1/3  Build Release ==" -ForegroundColor Cyan
$dropArg = if ($DropIn) { "ON" } else { "OFF" }
& "$root\build_release.bat" $dropArg
if ($LASTEXITCODE -ne 0) { throw "release build failed" }

Write-Host "== 2/3  windeployqt ==" -ForegroundColor Cyan
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Path $stage -Force | Out-Null
Copy-Item "$root\build-release\ClipKeeper.exe" $stage
& "$qt\bin\windeployqt.exe" --release --no-translations --no-opengl-sw --compiler-runtime `
    --dir $stage "$stage\ClipKeeper.exe"
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed" }

Write-Host "== 3/3  Inno Setup ==" -ForegroundColor Cyan
& $iscc "$root\installer.iss"
if ($LASTEXITCODE -ne 0) { throw "ISCC failed" }

Write-Host "DONE -> $root\dist\ClipKeeper-Setup.exe" -ForegroundColor Green
