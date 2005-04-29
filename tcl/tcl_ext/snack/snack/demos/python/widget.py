#! /usr/bin/env python

from Tkinter import *
from tkSnack import *

root = Tkinter.Tk()

initializeSnack(root)

s1 = Sound(load='ex1.wav')
s2 = Sound()

Frame(root).pack(pady=5)
Label(root, text='Snack Sound Toolkit Demonstration',
      font='Helvetica 14 bold').pack()

def load():
    file = root.tk.call('eval', 'snack::getOpenFile')
    s2.read(file)

def save():
    file = root.tk.call('eval', 'snack::getSaveFile')
    s2.write(file)
        
def play():
    s2.play()

def pause():
    s2.pause()
        
def stop():
    s2.stop()
    root.after_cancel(id)

def timer():
    len = s2.length(units='seconds')
    timelab.configure(text=len)
    root.after(100,timer)
        
def record():
    s2.record()
    id=root.after(100,timer)

f0 = Frame(root)
f0.pack(pady=5)
Label(f0, text='Basic sound handling:').pack(anchor='w')
timelab = Label(f0, text='0.00 sec',width=10)
timelab.pack(side='left')
Button(f0, bitmap='snackPlay', command=play).pack(side='left')
Button(f0, bitmap='snackPause', command=pause).pack(side='left')
Button(f0, bitmap='snackStop', command=stop).pack(side='left')
Button(f0, bitmap='snackRecord', fg='red', command=record).pack(side='left')
Button(f0, image='snackOpen', command=load).pack(side='left')
Button(f0, image='snackSave', command=save).pack(side='left')
 
colors = '#000 #006 #00B #00F #03F #07F #0BF #0FF #0FB #0F7\
	 #0F0 #3F0 #7F0 #BF0 #FF0 #FB0 #F70 #F30 #F00'


c = SnackCanvas(width=680, height=140, highlightthickness=0)
c.pack(pady=5)

c.create_text(0, 0, text='Waveform canvas item type:',anchor='nw')
c.create_waveform(0, 20, sound=s1, height=120, width=250 ,frame='yes')
c.create_text(250, 0, text='Spectrogram canvas item type:',anchor='nw')
c.create_spectrogram(250, 20, sound=s1, height=120, width=250, colormap=colors)
c.create_text(480, 0, text='Spectrum section canvas item type:',anchor='nw')
c.create_section(500, 20, sound=s1, height=120, width=180 ,frame='yes',
	start=8000, end=10000, minval=-100)

root.mainloop()
