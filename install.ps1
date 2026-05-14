param(
    [string]$nuke_dir = "",
    [string]$cuda_dir = "",
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
  ENABLE_OPTIX = "OFF"
  ENABLE_OIDN  = "ON"
  ENABLE_CPU   = "ON"
  ENABLE_CUDA  = "OFF"
  ENABLE_SYCL  = "OFF"
  ENABLE_HIP   = "OFF"
  ENABLE_METAL = "OFF"
}

if ($optix)     { $FLAGS.ENABLE_OPTIX = "ON" }
if ($no_optix)  { $FLAGS.ENABLE_OPTIX = "OFF" }

if ($oidn)      { $FLAGS.ENABLE_OIDN = "ON" }
if ($no_oidn)   { $FLAGS.ENABLE_OIDN = "OFF" }

if ($cpu)       { $FLAGS.ENABLE_CPU = "ON" }
if ($no_cpu)    { $FLAGS.ENABLE_CPU = "OFF" }

if ($cuda)      { $FLAGS.ENABLE_CUDA = "ON" }
if ($no_cuda)   { $FLAGS.ENABLE_CUDA = "OFF" }

if ($sycl)      { $FLAGS.ENABLE_SYCL = "ON" }
if ($no_sycl)   { $FLAGS.ENABLE_SYCL = "OFF" }

if ($hip)       { $FLAGS.ENABLE_HIP = "ON" }
if ($no_hip)    { $FLAGS.ENABLE_HIP = "OFF" }

if ($metal)     { $FLAGS.ENABLE_METAL = "ON" }
if ($no_metal)  { $FLAGS.ENABLE_METAL = "OFF" }

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
  -DCMAKE_INSTALL_PREFIX="$install_dir" `
  -DCMAKE_BUILD_TYPE=Release `
  -DNuke_ROOT="$nuke_dir" `
  -DENABLE_OPTIX="$($FLAGS["ENABLE_OPTIX"])" `
  -DENABLE_OIDN="$($FLAGS["ENABLE_OIDN"])" `
  -DENABLE_CPU="$($FLAGS["ENABLE_CPU"])" `
  -DENABLE_CUDA="$($FLAGS["ENABLE_CUDA"])" `
  -DENABLE_SYCL="$($FLAGS["ENABLE_SYCL"])" `
  -DENABLE_METAL="$($FLAGS["ENABLE_METAL"])" `
  -DENABLE_HIP="$($FLAGS["ENABLE_HIP"])" `
  -DCUDAToolkit_ROOT="$cuda_dir" `
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