# Build & Installation Guide

## Overview

This project builds and installs a Nuke plugin with optional support for CPU and GPU backends (OIDN, CUDA, OptiX, etc.). The build system is powered by **CMake**, with helper scripts for Windows and Unix-based systems.

---

# 🏗️ Build Process

The build is split into two stages:

1. **Build dependencies**
2. **Build & install plugin**

---

## 1. Prerequisites

Ensure the following are installed:

* **CMake** (3.20+ recommended)
* C++ compiler:

  * Windows: Visual Studio (MSVC)
  * Linux: GCC / Clang
  * macOS: Clang
* **Nuke installed** (required for plugin build)
* Optional:

  * CUDA Toolkit (for CUDA / OptiX)
  * Compatible GPU drivers

---

## 2. Step 1 — Build Dependencies

This compiles third-party libraries (OIDN, etc.).

### Windows

```bash
building/build.bat
```

### Linux / macOS

```bash
building/build.sh
```

---

## 3. Step 2 — Build & Install Plugin

This step **requires your Nuke installation path**.

---

### 🔹 Linux / macOS

```bash
install.sh --nuke-dir=/path/to/Nuke
```

Example:

```bash
install.sh --nuke-dir=/usr/local/Nuke13.2v5
```

---

### 🔹 Windows

```powershell
install.ps1 -nuke_dir "C:\Program Files\Nuke13.2v5"
```

---

## 📦 Installation Output

By default:

* Build directory:

  ```
  ./build
  ```

* Installed plugin:

  ```
  ./plugins
  ```

You can move or symlink this into your `.nuke` directory if needed.

---

# ⚙️ Build Options

## Feature Toggles

| Feature | Default                    | Description              |
| ------- | -------------------------- | ------------------------ |
| OIDN    | ON                         | Intel Open Image Denoise |
| CPU     | ON                         | OIDN CPU backend         |
| CUDA    | OFF                        | OIDN CUDA backend        |
| OptiX   | ON (Linux) / OFF (Windows) | NVIDIA OptiX             |
| SYCL    | OFF                        | Intel GPU backend        |
| HIP     | OFF                        | AMD backend              |
| Metal   | OFF                        | Apple GPU backend        |

---

## Script Flags (Linux/macOS)

```bash
install.sh --nuke-dir=/path/to/nuke [options]
```

| Flag                     | Description              |
| ------------------------ | ------------------------ |
| `--optix` / `--no-optix` | Enable/disable OptiX     |
| `--oidn` / `--no-oidn`   | Enable/disable OIDN      |
| `--cpu` / `--no-cpu`     | Enable/disable OIDN CPU  |
| `--cuda` / `--no-cuda`   | Enable/disable OIDN CUDA |
| `--sycl` / `--no-sycl`   | Enable/disable SYCL      |
| `--hip` / `--no-hip`     | Enable/disable HIP       |
| `--metal` / `--no-metal` | Enable/disable Metal     |
| `--build-dir=PATH`       | Custom build directory   |
| `--install-dir=PATH`     | Custom install directory |

---

## Script Flags (Windows)

```powershell
install.ps1 -nuke_dir <path> [options]
```

| Flag                   | Description                |
| ---------------------- | -------------------------- |
| `-optix` / `-no_optix` | Enable/disable OptiX       |
| `-oidn` / `-no_oidn`   | Enable/disable OIDN        |
| `-cpu` / `-no_cpu`     | Enable/disable CPU backend |
| `-cuda` / `-no_cuda`   | Enable/disable CUDA        |
| `-sycl` / `-no_sycl`   | Enable/disable SYCL        |
| `-hip` / `-no_hip`     | Enable/disable HIP         |
| `-metal` / `-no_metal` | Enable/disable Metal       |
| `-build_dir <path>`    | Custom build directory     |
| `-install_dir <path>`  | Custom install directory   |
| `-cuda_dir <path>`     | Optional CUDA Toolkit path |

---

## Example Builds

### CPU-only (safe default)

```bash
install.sh --nuke-dir=/path/to/nuke --no-cuda --no-optix
```

### Full NVIDIA build

```bash
install.sh --nuke-dir=/path/to/nuke --cuda --optix
```

---

# 🧠 Plugin Usage

## Inputs

| Input    | Description                                  |
| -------- | -------------------------------------------- |
| `color`  | Main render (required)                       |
| `albedo` | Diffuse albedo (optional)                    |
| `normal` | Normals (optional)                           |
| `motion` | Motion vectors (required for OptiX temporal) |

---

## Typical Workflows

### Basic Denoising

* Input: `color`
* Backend: CPU (default)

### High-Quality Denoising

* Inputs:

  * `color`
  * `albedo`
  * `normal`

### Temporal Denoising (OptiX)

* Requires:

  * CUDA + OptiX enabled at build
  * `motion` input

---

# ⚠️ Notes

* `--nuke-dir` / `-nuke_dir` is **required**
* OptiX requires NVIDIA GPU + drivers
* CUDA builds require CUDA Toolkit installed
* OIDN may crash on some Windows 11 machines
* Some backends are platform-specific
* Clean builds are recommended when switching configs
* Plugin is experimental and may be unstable

---

# 🤝 Contributions

Contributions and testing are welcome.

When reporting issues, include:

* OS and compiler
* Nuke version
* Enabled build flags
* Full build logs
