param(
    [switch]$optix,
    [switch]$no_optix,

    [switch]$oidn,
    [switch]$no_oidn,

    [switch]$cpu,
    [switch]$no_cpu,

    [switch]$cuda,
    [switch]$no_cuda,

    [switch]$sycl,
    [switch]$no_sycl,

    [switch]$hip,
    [switch]$no_hip,

    [switch]$metal,
    [switch]$no_metal,

    [string]$oidn_version = "2.4.1",
    [string]$optix_version = "9.1.0",
    [string]$tbb_version = "2021.13.0",
    [string]$ispc_version = "1.30.0",

    [string]$build_dir = "",
    [string]$install_dir = ""
)

# -------------------------
# Script directory
# -------------------------
$script_dir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $script_dir

# -------------------------
# Defaults (match BAT)
# -------------------------
$BUILD_DIR   = if ($build_dir) { $build_dir } else { Join-Path $script_dir "..\build" }
$INSTALL_DIR = if ($install_dir) { $install_dir } else { Join-Path $script_dir "..\lib" }

$FLAGS = @{
  OPTIX = "OFF"
  OIDN  = "ON"
  CPU   = "ON"
  CUDA  = "OFF"
  SYCL  = "OFF"
  HIP   = "OFF"
  METAL = "OFF"
}

if ($optix)     { $FLAGS.OPTIX = "ON" }
if ($no_optix)  { $FLAGS.OPTIX = "OFF" }

if ($oidn)      { $FLAGS.OIDN = "ON" }
if ($no_oidn)   { $FLAGS.OIDN = "OFF" }

if ($cpu)       { $FLAGS.CPU = "ON" }
if ($no_cpu)    { $FLAGS.CPU = "OFF" }

if ($cuda)      { $FLAGS.CUDA = "ON" }
if ($no_cuda)   { $FLAGS.CUDA = "OFF" }

if ($sycl)      { $FLAGS.SYCL = "ON" }
if ($no_sycl)   { $FLAGS.SYCL = "OFF" }

if ($hip)       { $FLAGS.HIP = "ON" }
if ($no_hip)    { $FLAGS.HIP = "OFF" }

if ($metal)     { $FLAGS.METAL = "ON" }
if ($no_metal)  { $FLAGS.METAL = "OFF" }

# -------------------------
# Ensure directories exist
# -------------------------
New-Item -ItemType Directory -Force -Path $BUILD_DIR | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $INSTALL_DIR "bin") | Out-Null

# cmd /c "rd /s /q $BUILD_DIR"

# -------------------------
# Parallel build (Windows cores)
# -------------------------
$cores = (Get-CimInstance Win32_Processor | Select-Object -ExpandProperty NumberOfLogicalProcessors)
if (-not $cores) { $cores = 4 }

$env:CMAKE_BUILD_PARALLEL_LEVEL = $cores

# -------------------------
# Configure
# -------------------------
cmake -S . -B $BUILD_DIR `
    -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" `
    -DENABLE_OPTIX:BOOL=$($FLAGS["OPTIX"]) `
    -DENABLE_OIDN:BOOL=$($FLAGS["OIDN"]) `
    -DENABLE_CPU:BOOL=$($FLAGS["CPU"]) `
    -DENABLE_CUDA:BOOL=$($FLAGS["CUDA"]) `
    -DENABLE_SYCL:BOOL=$($FLAGS["SYCL"]) `
    -DENABLE_HIP:BOOL=$($FLAGS["HIP"]) `
    -DENABLE_METAL:BOOL=$($FLAGS["METAL"]) `
    -DOIDN_VERSION="$oidn_version" `
    -DOPTIX_VERSION="$optix_version" `
    -DTBB_VERSION="$tbb_version" `
    -DISPC_VERSION="$ispc_version" `
    -Wno-dev

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configure failed" -ForegroundColor Red
    exit 1
}

# -------------------------
# Build
# -------------------------
cmake --build $BUILD_DIR --config Release -- /m:$cores

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed" -ForegroundColor Red
    exit 1
}

Write-Host "Done." -ForegroundColor Green