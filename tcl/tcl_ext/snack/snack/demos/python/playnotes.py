#! /usr/bin/env python

from Tkinter import *
from tkSnack import *

root = Tkinter.Tk()
initializeSnack(root)

s = Sound()
filt = Filter('generator', 440, 30000, 0.0, 'sine', 8000)

def beep(freq):
   filt.configure(freq)
   s.play(filter=filt,blocking=1)

beep(261.6)
beep(293.7)
beep(329.7)
beep(349.3)
