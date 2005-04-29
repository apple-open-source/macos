#! /usr/bin/env python

from Tkinter import *
from tkSnack import *

root = Tkinter.Tk()

initializeSnack(root)
"""AudioControllerSingleton().playLatency(100)"""

s = Sound()

filt = Filter('generator', 440.0)
        
def play():
   s.stop()
   s.play(filter=filt)
        
def stop():
   s.stop()        

def config(arg):
   type = var.get()
   if var.get() == "sine" :
      shape = 0.0
   elif var.get() == "rectangle" :
      shape = 0.5
   elif var.get() == "triangle" :
      shape = 0.5
   elif var.get() == "sawtooth" :
      type = "triangle"
      shape = 0.0
   else :
      shape = 0.0
   filt.configure(s1.get(), s2.get(), shape, type, -1)

f = Frame(root)
f.pack()

s1 = Scale(f, from_=4000, to=50, label="Frequency", length=200, command=config)
s1.pack(side='left')

s2 = Scale(f, from_=32767, to=0, label="Amplitude", length=200, command=config)
s2.pack(side='left')

s1.set(440.0)
s2.set(20000)

var  = StringVar()
var.set("sine")

menu = OptionMenu(root, var, "sine", "rectangle", "triangle", "sawtooth", "noise")
menu.pack()
root.bind_all("<Button1-ButtonRelease>", config)

fb = Frame(root)
fb.pack(side='bottom')
Button(fb, bitmap='snackPlay', command=play).pack(side='left')
Button(fb, bitmap='snackStop', command=stop).pack(side='left')

root.mainloop()
