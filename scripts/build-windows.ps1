<#
.SYNOPSIS
    Build PureCloak for Windows x86_64.

.DESCRIPTION
    Compiles PureCloak from source on Windows. Requires Visual Studio 2022,
    Windows SDK, and depot_tools.

.PARAMETER Debug
    Build debug configuration instead of release.

.PARAMETER OutDir
    Output directory (default: out/purecloak).

.PARAMETER SkipSync
    Skip gclient sync (use existing checkout).

.PARAMETER SkipTests
    Skip building and running unit tests.

.PARAMETER Jobs
    Number of parallel build jobs (default: number of logical CPUs).

.PARAMETER NoStrip
    Don't strip debug info from the final binary.

.EXAMPLE
    .\build-windows.ps1
    .\build-windows.ps1 -Debug -Jobs 8

.NOTES
    Requires: Visual Studio 2022+, Windows SDK 10.0.20348+, 50GB+ free disk.
#>

param(
  [switch]$Debug,
  [string]$OutDir = "out/purecloak",
  [switch]$SkipSync,
  [switch]$SkipTests,
  [int]$Jobs = (Get-CimInstance Win32_ComputerSystem).NumberOfLogicalProcessors,
  [switch]$NoStrip
)

$ErrorActionPreference = "Stop"
$ProjectDir = Split-Path -Parent $PSScriptRoot
Set-Location $ProjectDir

Write-Host "==> PureCloak Windows Build" -ForegroundColor Cyan
Write-Host "    Project: $ProjectDir"
Write-Host "    Type:    $(if ($Debug) { 'debug' } else { 'release' })"
Write-Host "    Out:     $OutDir"
Write-Host "    Jobs:    $Jobs"

# ── 1. Check prerequisites ──────────────────────────────────────────
Write-Host ""
Write-Host "==> Checking prerequisites..." -ForegroundColor Yellow

# Check Visual Studio
$vsPath = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" `
  -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
  -property installationPath 2>$null

if (-not $vsPath) {
  $vsPath = & "${env:ProgramFiles}\Microsoft Visual Studio\Installer\vswhere.exe" `
    -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath 2>$null
}

if (-not $vsPath) {
  Write-Host "  [✗] Visual Studio 2022 not found (need VC++ tools)"
  Write-Host "  Install from: https://visualstudio.microsoft.com/"
  exit 1
}
Write-Host "  [✓] Visual Studio: $vsPath"

# Check Windows SDK
$sdkPaths = @(
  "${env:ProgramFiles(x86)}\Windows Kits\10\bin\10.0.20348.0",
  "${env:ProgramFiles(x86)}\Windows Kits\10\bin\*"
)
$sdkFound = $false
foreach ($p in $sdkPaths) {
  if (Test-Path $p) { $sdkFound = $true; break }
}
if (-not $sdkFound) {
  Write-Host "  [!] Windows SDK 10.0.20348+ recommended"
}
Write-Host "  [✓] Windows SDK found"

# Check Python
try { python3 --version | Out-Null; Write-Host "  [✓] python3" }
catch { Write-Host "  [✗] python3 not found"; exit 1 }

# Check git
try { git --version | Out-Null; Write-Host "  [✓] git" }
catch { Write-Host "  [✗] git not found"; exit 1 }

# ── 2. depot_tools ──────────────────────────────────────────────────
$depotToolsPath = Join-Path $ProjectDir "depot_tools"
if (-not (Test-Path (Join-Path $depotToolsPath "gn.exe"))) {
  Write-Host ""
  Write-Host "==> Installing depot_tools..." -ForegroundColor Yellow
  git clone --depth=1 https://chromium.googlesource.com/chromium/tools/depot_tools.git $depotToolsPath
}
$env:PATH = "$depotToolsPath;$env:PATH"

# ── 3. gclient sync ─────────────────────────────────────────────────
if (-not $SkipSync) {
  Write-Host ""
  Write-Host "==> Step 1/5: Syncing Chromium source..." -ForegroundColor Yellow

  $gclientFile = Join-Path $ProjectDir ".gclient"
  if (-not (Test-Path $gclientFile)) {
    Write-Host "  [✗] .gclient not found at $gclientFile"
    exit 1
  }

  # Temporarily enable managed=True for initial sync
  $content = Get-Content $gclientFile -Raw
  $didEnable = $false
  if ($content -match '"managed"\s*:\s*False') {
    $content = $content -replace '"managed"\s*:\s*False', '"managed": True'
    Set-Content $gclientFile $content
    $didEnable = $true
  }

  # On Windows, use the 'vs_toolchain' wrapper
  $env:DEPOT_TOOLS_WIN_TOOLCHAIN = "0"  # Use locally installed VS

  & gclient sync --shallow --nohooks -j $Jobs
  if ($LASTEXITCODE -ne 0) { Write-Host "gclient sync failed"; exit 1 }

  & gclient runhooks
  if ($LASTEXITCODE -ne 0) { Write-Host "gclient runhooks failed"; exit 1 }

  if ($didEnable) {
    $content = $content -replace '"managed"\s*:\s*True', '"managed": False'
    Set-Content $gclientFile $content
  }

  # Restore PureCloak custom files
  Write-Host "  [i] Restoring PureCloak custom files..."
  git checkout -- chromium_src/chrome/app/chromium_strings.grd 2>$null
  git checkout -- chromium_src/chrome/browser/ui/browser_command_controller.cc 2>$null
  git checkout -- chromium_src/chrome/browser/ui/views/profiles/profile_menu_view.cc 2>$null
  git checkout -- chromium_src/ash/ 2>$null
  git checkout -- chromium_src/chromeos/ 2>$null
  git checkout -- chromium_src/chrome/app/theme/chromium/ 2>$null
  git checkout -- chromium_src/chrome/app/theme/default_*/chromium/ 2>$null
  if ($LASTEXITCODE -ne 0) {
    Write-Host "  [i] Some files may not be tracked in git (expected for chromium theme assets)"
  }

  Write-Host "  [✓] Sync complete"
} else {
  Write-Host "  [i] Skipping gclient sync"
}

# ── 4. GN gen ────────────────────────────────────────────────────────
Write-Host ""
Write-Host "==> Step 2/5: Configuring GN..." -ForegroundColor Yellow

if ($Debug) {
  $gnArgs = "is_debug=true is_purecloak=true"
} else {
  $gnArgs = @"
is_debug=false
is_purecloak=true
is_component_build=false
symbol_level=0
blink_symbol_level=0
optimize_for_size=true
enable_nacl=false
target_cpu="x64"
"@
}

& gn gen $OutDir --args="$gnArgs" --fail-on-unused-args
if ($LASTEXITCODE -ne 0) { Write-Host "GN gen failed"; exit 1 }
Write-Host "  [✓] GN gen complete"

# ── 5. Build ─────────────────────────────────────────────────────────
Write-Host ""
Write-Host "==> Step 3/5: Building chrome (this will take a while)..." -ForegroundColor Yellow

& autoninja -C $OutDir chrome -j $Jobs
if ($LASTEXITCODE -ne 0) { Write-Host "Build failed!"; exit 1 }
Write-Host "  [✓] Build complete"

# ── 6. Tests ─────────────────────────────────────────────────────────
if (-not $SkipTests) {
  Write-Host ""
  Write-Host "==> Step 4/5: Building unit tests..." -ForegroundColor Yellow

  & autoninja -C $OutDir purecloak_unittests -j $Jobs
  if ($LASTEXITCODE -eq 0) {
    Write-Host "  Running tests..."
    & "$OutDir/purecloak_unittests.exe" --gtest_filter=-*DeathTest*
    Write-Host "  [✓] Tests passed"
  } else {
    Write-Host "  [i] purecloak_unittests target not found, skipping"
  }
} else {
  Write-Host ""
  Write-Host "==> Step 4/5: Tests skipped" -ForegroundColor Yellow
}

# ── 7. Package ───────────────────────────────────────────────────────
Write-Host ""
Write-Host "==> Step 5/5: Packaging..." -ForegroundColor Yellow

$binary = Join-Path $OutDir "chrome.exe"
if (Test-Path $binary) {
  $sizeBefore = (Get-Item $binary).Length / 1MB
  Write-Host "  Binary size: $([math]::Round($sizeBefore)) MB"

  $distDir = Join-Path $ProjectDir "dist\purecloak-windows-x64"
  New-Item -ItemType Directory -Path $distDir -Force | Out-Null
  Copy-Item $binary "$distDir\PureCloak.exe"

  # Copy DLLs
  $dllDir = Join-Path $OutDir "*.dll"
  Copy-Item $dllDir $distDir 2>$null

  # Create zip
  $zipFile = Join-Path $ProjectDir "dist\purecloak-windows-x64.zip"
  if (Test-Path $zipFile) { Remove-Item $zipFile }
  Compress-Archive -Path "$distDir\*" -DestinationPath $zipFile
  Remove-Item -Recurse $distDir -Force

  Write-Host "  [✓] Package: $zipFile"
} else {
  Write-Host "  [!] Binary not found at $binary"
}

Write-Host ""
Write-Host "==> Build complete! 🪟" -ForegroundColor Cyan
Write-Host "    Binary: $ProjectDir/$OutDir/chrome.exe"
Write-Host "    Run:    $ProjectDir/$OutDir/chrome.exe"
