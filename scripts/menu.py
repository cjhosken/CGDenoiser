import nuke
import os

os.environ['PATH'] += os.pathsep + os.path.dirname(os.path.realpath(__file__))
nuke.menu('Nodes').addCommand("CGDenoiser", "nuke.createNode('CGDenoiser')")