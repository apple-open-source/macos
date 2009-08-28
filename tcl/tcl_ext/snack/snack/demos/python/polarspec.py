#! /usr/bin/env python

from Tkinter import *
from tkSnack import *
from math import *

root = Tkinter.Tk()

initializeSnack(root)
snd = Sound()

w = 300
h = 300
s = 100
n = 1024
type = StringVar()
type.set("FFT") 

def stop():
    snd.stop()
    root.after_cancel(id)

def draw():
    if (snd.length() > n) :
        pos = snd.length() - n
        spec = snd.dBPowerSpectrum(start=pos,fftlen=n,winlen=n,analysistype=type.get())
        coords=[]
        f=0.0001
        for val in spec :
            v = 6.282985 * log(f)/log(2.0)
            a = 1.4*(val+s)
            x = w/2+a*cos(v)
            y = h/2+a*sin(v)
            coords.append(x)
            coords.append(y)
            f = f + 16000.0 / n
        c.delete('polar')
        c.create_polygon(coords, tags='polar', fill='green')
    if (snd.length(unit='sec') > 20) :
        stop()
    id = root.after(100,draw)
        
def start():
    pos = 0
    snd.record()
    c.update_idletasks()
    id = root.after(100,draw)

c = SnackCanvas(height=h, width=w, bg='black')
c.pack()
f = Frame()
f.pack()
Button(f, bitmap='snackRecord', fg='red', command=start).pack(side='left')
Button(f, bitmap='snackStop', command=stop).pack(side='left')
Radiobutton(f, text='FFT', variable=type, value='FFT').pack(side='left')
Radiobutton(f, text='LPC', variable=type, value='LPC').pack(side='left')
Button(f, text='Exit', command=root.quit).pack(side='left')

root.mainloop()
