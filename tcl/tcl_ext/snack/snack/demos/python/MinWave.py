#! /usr/bin/env python

from Tkinter import *
from tkSnack import *

root = Tkinter.Tk()

initializeSnack(root)

s = Sound(load='ex1.wav')
c = SnackCanvas(height=100, width=400)
c.pack()
c.create_waveform(0, 0, sound=s, width=400)

Button(root, text='Exit', command=root.quit).pack()

root.mainloop()
