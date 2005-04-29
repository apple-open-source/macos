#! /usr/bin/env python

from Tkinter import *
from tkSnack import *

root = Tkinter.Tk()

initializeSnack(root)
snd = Sound()

def stop():
    snd.stop()

def start():
    snd.record()

c = SnackCanvas(height=200, width=400, bg='black')
c.pack()
c.create_spectrogram(1,1,sound=snd,width=400,height=200,pixelspersec=200)

f = Frame()
f.pack()
Button(f, bitmap='snackRecord', fg='red', command=start).pack(side='left')
Button(f, bitmap='snackStop', command=stop).pack(side='left')
Button(f, text='Exit', command=root.quit).pack(side='left')

root.mainloop()
