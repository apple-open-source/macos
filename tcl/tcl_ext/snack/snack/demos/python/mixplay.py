#! /usr/bin/env python

from Tkinter import *
from tkSnack import *

root = Tkinter.Tk()

initializeSnack(root)

snd1 = Sound()
snd2 = Sound()

map1 = Filter('map', 1.0)
map2 = Filter('map', 1.0)

def play():
   snd1.play(filter=map1)
   snd2.play(filter=map2)

def stop():
   snd1.stop()
   snd2.stop()

def config(arg):
   map1.configure(scale1.get())
   map2.configure(scale2.get())
                                                                           
def load1():
   filename = root.tk.call('eval', 'snack::getOpenFile')
   snd1.config(file=filename)

def load2():
   filename = root.tk.call('eval', 'snack::getOpenFile')
   snd2.config(file=filename)
         
                                                              
f = Frame(root)
f.pack()

scale1 = Scale(f, from_=1.0, to=0, resolution=0.01, label="sound 1", command=config)
scale1.pack(side='left')

scale2 = Scale(f, from_=1.0, to=0, resolution=0.01, label="sound 2", command=config)
scale2.pack(side='left')

scale1.set(1.0)
scale2.set(1.0)

fb = Frame(root)
fb.pack(side='bottom')
Button(fb, text='load 1', command=load1).pack(side='left')
Button(fb, text='load 2', command=load2).pack(side='left')
Button(fb, bitmap='snackPlay', command=play).pack(side='left')
Button(fb, bitmap='snackStop', command=stop).pack(side='left')

root.mainloop()
