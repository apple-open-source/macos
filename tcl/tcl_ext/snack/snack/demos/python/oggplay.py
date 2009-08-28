#! /usr/bin/env python
#
# Usage: oggplay.py file.ogg
#
# Note: this script will also play audio files in any other format supported
# by tkSnack

import sys
from Tkinter import *
from tkSnack import *

initializeSnack(Tkinter.Tk())

# Load the Ogg/Vorbis format package

Tk().tk.eval('package require snackogg')

if sys.argv[1:]:
	snd = Sound(file=sys.argv[1])
	snd.play(blocking=1)
else:
	print "Usage: oggplay.py file.ogg"
