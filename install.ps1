param(
    [string]$nuke_dir = "",
    [string]$build_dir = "build",
    [string]$install_dir = "plugins",

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

    [switch]$metal,
    [switch]$no_metal,

    [switch]$hip,
    [switch]$no_hip
)

# -------------------------
# Validate required input
# -------------------------
if (-not $nuke_dir) {
    Write-Host "Error: -nuke_dir is required" -ForegroundColor Red
    exit 1
}

# -------------------------
# Normalize paths
# -------------------------
$nuke_dir = (Resolve-Path $nuke_dir).Path

# -------------------------
# Defaults (match your intent)
# -------------------------
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
# Clean build dir
# -------------------------
if (Test-Path $build_dir) {
    Remove-Item $build_dir -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $build_dir | Out-Null
New-Item -ItemType Directory -Force -Path $install_dir | Out-Null

# -------------------------
# Parallel build detection
# -------------------------
$jobs = (Get-CimInstance Win32_Processor | Select-Object -ExpandProperty NumberOfLogicalProcessors)
if (-not $jobs) { $jobs = 4 }

$env:CMAKE_BUILD_PARALLEL_LEVEL = $jobs

# -------------------------
# Configure
# -------------------------
cmake -S . -B $build_dir `
  -Wno-dev `
  -DCMAKE_INSTALL_PREFIX="$install_dir" `
  -DNuke_ROOT="$nuke_dir" `
  -DENABLE_OPTIX="$($FLAGS["OPTIX"])" `
  -DENABLE_OIDN="$($FLAGS["OIDN"])" `
  -DCPU="$($FLAGS["CPU"])" `
  -DCUDA="$($FLAGS["CUDA"])" `
  -DSYCL="$($FLAGS["SYCL"]))" `
  -DMETAL="$($FLAGS["METAL"])" `
  -DHIP="$($FLAGS["HIP"])" `
  -Wno-dev

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configure failed" -ForegroundColor Red
    exit 1
}

# -------------------------
# Build + install
# -------------------------
cmake --build $build_dir --config Release --target install -- /m:$jobs

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed" -ForegroundColor Red
    exit 1
}

Write-Host "Done." -ForegroundColor Green