# Building

### 1. Build from Source

#### Step 1 — Build dependencies
Run:

**Windows**
```bash
building/build.bat
```

**Linux / macOS**
```bash
building/build.sh
```

#### Step 2 - Build plugin
Run:

**Windows**
```bash
install.bat
```

**Linux / macOS**
```bash
install.sh
```

This will build and install the plugin directly into ~/.nuke

## ⚙️ Build Options

Control features via CMake flags:

Option	Description
-DNO_OPTIX=ON	Disable OptiX support
-DNO_OIDN_CPU=OFF	Enable OIDN CPU backend
-DNO_OIDN_CUDA=ON	Disable OIDN CUDA backend
-DNO_OIDN_HIP=ON	Disable OIDN HIP backend
-DNO_OIDN_METAL=ON	Disable OIDN Metal backend
-DNO_OIDN_SYCL=ON	Disable OIDN SYCL backend


## 🧠 Usage
Inputs
Input	Description
color	Main beauty render
albedo	Diffuse albedo pass (optional)
normal	World or camera space normals (optional)
motion	Motion vectors (required for temporal OptiX)

## Contributions
I welcome any feedback and contributions to the code. The node is quite unstable and the more testers I have, the better!