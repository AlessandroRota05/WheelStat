# WheelStat - builds the OLED preview and renders the documentation images.
#
#   powershell -File tools/oled-preview/build.ps1
#
# Needs g++ (MinGW is fine), Python 3 with Pillow, and the Adafruit GFX
# library where the Arduino IDE installs it. Writes Media/oled/<lang>/.

param(
  [string]$GfxLibrary = "$env:USERPROFILE\Documents\Arduino\libraries\Adafruit_GFX_Library",
  [string]$Python = "python"
)

$ErrorActionPreference = "Stop"
$here = $PSScriptRoot
$repo = Resolve-Path "$here\..\.."
$work = Join-Path ([System.IO.Path]::GetTempPath()) "wheelstat-oled-preview"

if (-not (Test-Path "$GfxLibrary\Adafruit_GFX.cpp")) {
  throw "Adafruit GFX not found in $GfxLibrary (pass -GfxLibrary)."
}

$sources = @(
  "$here\preview.cpp",
  "$repo\src\ui\OledBoot.cpp",
  "$repo\src\ui\OledDraw.cpp",
  "$repo\src\ui\OledPages.cpp",
  "$repo\src\ui\OledSummary.cpp",
  "$repo\src\Channels.cpp",
  "$GfxLibrary\Adafruit_GFX.cpp"
)

foreach ($lang in "it", "en") {
  $frames = Join-Path $work $lang
  Remove-Item -Recurse -Force $frames -ErrorAction SilentlyContinue
  New-Item -ItemType Directory -Force $frames | Out-Null

  $exe = Join-Path $work "preview_$lang.exe"
  & g++ -std=gnu++17 -O1 -w "-DLANG_$($lang.ToUpper())" -DARDUINO=100 -DCONFIG_IDF_TARGET_ESP32 `
    "-I$here\shim" "-I$repo" "-I$GfxLibrary" @sources -o $exe
  if ($LASTEXITCODE -ne 0) { throw "build failed ($lang)" }

  & $exe $frames
  if ($LASTEXITCODE -ne 0) { throw "preview failed ($lang)" }

  & $Python "$here\render.py" $frames "$repo\Media\oled\$lang" $lang
  if ($LASTEXITCODE -ne 0) { throw "render failed ($lang)" }
}
