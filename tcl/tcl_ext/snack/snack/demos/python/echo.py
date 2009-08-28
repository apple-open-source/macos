#! /usr/bin/env python

from Tkinter import *
from tkSnack import *

root = Tkinter.Tk()

initializeSnack(root)

s = Sound()

filt = Filter('echo', 0.6, 0.6, 30, 0.4, 50, 0.3)

def load():
   filename = root.tk.call('eval', 'snack::getOpenFile')
   s.config(file=filename)
        
def play():
   s.play(filter=filt)
        
def stop():
   s.stop()        

def config(arg):
   filt.configure(s1.get(), s2.get(), s3.get(), s4.get(), s5.get(), s6.get())

f = Frame(root)
f.pack()

s1 = Scale(f, from_=1.0, to=0, resolution=0.01, label="inGain", command=config)
s1.pack(side='left')

s2 = Scale(f, from_=1.0, to=0, resolution=0.01, label="outGain", command=config)
s2.pack(side='left')

s3 = Scale(f, from_=250, to=10, label="Delay1", command=config)
s3.pack(side='left')

s4 = Scale(f, from_=1.0, to=0, resolution=0.01, label="Decay1", command=config)
s4.pack(side='left')

s5 = Scale(f, from_=250, to=10, label="Delay2", command=config)
s5.pack(side='left')

s6 = Scale(f, from_=1.0, to=0, resolution=0.01, label="Decay2", command=config)
s6.pack(side='left')

s1.set(0.6)
s2.set(0.6)
s3.set(30)
s4.set(0.4)
s5.set(50)
s6.set(0.3)

fb = Frame(root)
fb.pack(side='bottom')
Button(fb, image='snackOpen', command=load).pack(side='left')
Button(fb, bitmap='snackPlay', command=play).pack(side='left')
Button(fb, bitmap='snackStop', command=stop).pack(side='left')

root.mainloop()
