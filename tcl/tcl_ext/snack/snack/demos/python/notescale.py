#! /usr/bin/env python

from Tkinter import *
from tkSnack import *

root = Tkinter.Tk()

initializeSnack(root)
"""AudioControllerSingleton().playLatency(100)"""

def playbeep(freq):
   s.stop()
   filt.configure(freq)
   s.play(filter=filt)

def beepC4():
   playbeep(261.6)

def beepD4():
   playbeep(293.7)

def beepE4():
   playbeep(329.7)

def beepF4():
   playbeep(349.3)

def beepG4():
   playbeep(392.1)

def beepA4():
   playbeep(440.0)

def beepB4():
   playbeep(493.9)

def beepC5():
   playbeep(523.3)

s = Sound()

filt = Filter('generator', 440.0, 30000, 0.0, 'sine', 8000)
        
Button(root, text='C4', command=beepC4).pack(side='left')
Button(root, text='D4', command=beepD4).pack(side='left')
Button(root, text='E4', command=beepE4).pack(side='left')
Button(root, text='F4', command=beepF4).pack(side='left')
Button(root, text='G4', command=beepG4).pack(side='left')
Button(root, text='A4', command=beepA4).pack(side='left')
Button(root, text='B4', command=beepB4).pack(side='left')
Button(root, text='C5', command=beepC5).pack(side='left')

root.mainloop()
