import nuke
import os

plugin_dir = os.path.join(os.path.dirname(os.path.realpath(__file__)), "CGDenoiser")
nuke.pluginAddPath(plugin_dir)