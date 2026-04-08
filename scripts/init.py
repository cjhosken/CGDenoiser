import nuke
import os
import platform

# Define the plugin directory
plugin_dir = os.path.join(os.path.dirname(os.path.realpath(__file__)), "CGDenoiser")

# 1. Tell Nuke where the .dll (node) is
nuke.pluginAddPath(plugin_dir)

# 2. Tell Windows where the supporting .dlls (OIDN, TBB) are
if platform.system() == "Windows":
    # For modern Python (Nuke 13+)
    if hasattr(os, 'add_dll_directory'):
        os.add_dll_directory(plugin_dir)
    
    # Fallback/Redundancy: Add to PATH environment variable
    os.environ["PATH"] += os.pathsep + plugin_dir