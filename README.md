# CGDenoiser
Nuke plugin for denoising CG renders with OIDN and OptiX.

place CGDenoiser in `.nuke/`

## Installation

### Building from Source

First, run either `building/build.bat` or `building/build.sh` to build the dependencies.

Then, run either `install.bat` or `install.sh` to build the plugin.

Once complete, you should see `plugins/CGDenoiser`.

### Adding to Nuke

Copy `plugins/CGDenoiser` into your NUKE_PATH (usually `.nuke`)

This needs to be added to `.nuke/init.py`
```py
import nuke
import os

nuke.pluginAddPath(os.path.join(os.path.dirname(os.path.realpath(__file__)), "CGDenoiser"))
```

Copy `scripts/menu.py` into `.nuke/menu.py`