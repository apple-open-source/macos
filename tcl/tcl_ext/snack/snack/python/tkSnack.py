"""
tkSnack
An interface to Kare Sjolander's Snack Tcl extension
http://www.speech.kth.se/snack/index.html

by Kevin Russell and Kare Sjolander
last modified: Mar 28, 2003
"""

import Tkinter
import types
import string

Tkroot = None
audio = None
mixer = None

def initializeSnack(newroot):
    global Tkroot, audio, mixer
    Tkroot = newroot
    Tkroot.tk.call('eval', 'package require snack')
    Tkroot.tk.call('snack::createIcons')
    Tkroot.tk.call('snack::setUseOldObjAPI')
    audio = AudioControllerSingleton()
    mixer = MixerControllerSingleton()


def _cast(astring):
    """This function tries to convert a string returned by a Tcl call
    to a Python integer or float, if possible, and otherwise passes on the
    raw string (or None instead of an empty string)."""
    try:
        return int(astring)
    except ValueError:
        try:
            return float(astring)
        except ValueError:
            if astring:
                return astring
            else:
                return None


class NotImplementedException(Exception):
    pass


class TkObject:
    """A mixin class for various Python/Tk communication functions,
    such as reading and setting the object's configuration options.
    We put them in a mixin class so we don't have to keep repeating
    them for sounds, filters, and spectrograms.
    These are mostly copied from the Tkinter.Misc class."""

    def _getboolean(self, astring):
        if astring:
            return self.tk.getboolean(astring)

    def _getints(self, astring):
        if astring:
            return tuple(map(int, self.tk.splitlist(astring)))

    def _getdoubles(self, astring):
        if astring:
            return tuple(map(float, self.tk.splitlist(astring)))

    def _options(self, cnf, kw=None):
        if kw:
            cnf = Tkinter._cnfmerge((cnf, kw))
        else:
            cnf = Tkinter._cnfmerge(cnf)
        res = ()
        for k,v in cnf.items():
            if v is not None:
                if k[-1] == '_': k = k[:-1]
                #if callable(v):
                #    v = self._register(v)
                res = res + ('-'+k, v)
        return res

    def configure(self, cnf=None, **kw):
        self._configure(cnf, kw)

    def _configure(self, cnf=None, kw={}):
        if kw:
            cnf = Tkinter._cnfmerge((cnf, kw))
        elif cnf:
            cnf = Tkinter._cnfmerge(cnf)
        if cnf is None:
            cnf = {}
            for x in self.tk.split(
                self.tk.call(self.name, 'configure')):
                cnf[x[0][1:]] = (x[0][1:],) + x[1:]
                return cnf
        if type(cnf) is types.StringType:
            x = self.tk.split(self.tk.call(self.name, 'configure', '-'+cnf))
            return (x[0][1:],) + x[1:]
        self.tk.call((self.name, 'configure') + self._options(cnf))
    config = configure

    def cget(self, key):
        return _cast(self.tk.call(self.name, 'cget' , '-'+key))    
    
    # Set "cget" as the method to handle dictionary-like attribute access
    __getitem__ = cget

    def __setitem__(self, key, value):
        self.configure({key: value})

    def keys(self):
        return map(lambda x: x[0][1:],
                   self.tk.split(self.tk.call(self.name, 'configure')))

    def __str__(self):
        return self.name



class Sound (TkObject):
    
    def __init__(self, name=None, master=None, **kw):
        self.name = None
        if not master:
            if Tkroot:
                master = Tkroot
            else:
                raise RuntimeError, \
                      'Tk not intialized or not registered with Snack'
        self.tk = master.tk
        if not name:
            self.name = self.tk.call(('sound',) + self._options(kw))
        else:
            self.name = self.tk.call(('sound', name) + self._options(kw))
        #self._configure(cnf, kw)
        
    def append(self, binarydata, **kw):
        """Appends binary string data to the end of the sound."""
        self.tk.call((self.name, 'append', binarydata) + self._options(kw))
     
    def concatenate(self, othersound):
        """Concatenates the sample data from othersound to the end of
        this sound.  Both sounds must be of the same type."""
        self.tk.call(self.name, 'concatenate', othersound.name)
        
    def configure(self, **kw):
        """The configure command is used to set options for a sound."""
        self.tk.call((self.name, 'configure') + self._options(kw))

    def copy(self, sound, **kw):
        """Copies sample data from another sound into self."""
        self.tk.call((self.name, 'copy', sound.name) + self._options(kw))

    def changed(self, flag):
        """This command is used to inform Snack that the sound object has been
        modified. Normally Snack tracks changes to sound objects automatically,
        but in a few cases this must be performed explicitly. For example,
        if individual samples are changed using the sample command these
        will not be tracked for performance reasons."""
        self.tk.call((self.name, 'changed', flag))

    def convert(self, **kw):
        """Convert a sound to a different sample encoding, sample rate,
        or number of channels."""
        self.tk.call((self.name, 'convert') + self._options(kw))

    def crop(self, start=1, end=None, **kw):
        """Removes all samples outside of the range [start..end]."""
        if end is None:
            end = self.length()
        self.tk.call((self.name, 'crop', start, end) + self._options(kw))

    def cut(self, start=1, end=None, **kw):
        """Removes all samples inside the range [start..end]."""
        if end is None:
            end = self.length()
        self.tk.call((self.name, 'cut', start, end) + self._options(kw))
        
    def data(self, binarydata=None, **kw):
        """Loads sound data from, or writes to, a binary string."""
        if binarydata: # copy data to sound
            self.tk.call((self.name, 'data', binarydata) + self._options(kw))
        else: # return sound data
            return self.tk.call((self.name, 'data') + self._options(kw))
                          
    def destroy(self):
        """Removes the Tcl command for this sound and frees the storage
        associated with it."""
        self.tk.call(self.name, 'destroy')
        
    def dBPowerSpectrum(self, **kw):
        """Computes the log FFT power spectrum of the sound (at the time
        given by the start option) and returns a list of dB values."""
        result = self.tk.call((self.name, 'dBPowerSpectrum')
                              + self._options(kw))
        return self._getdoubles(result)        

    def powerSpectrum(self, **kw):
        """Computes the FFT power spectrum of the sound (at the time
        given by the start option) and returns a list of magnitude values."""
        result = self.tk.call((self.name, 'powerSpectrum')
                              + self._options(kw))
        return self._getdoubles(result)        

    def filter(self, filter, **kw):
        """Applies the given filter to the sound."""
        return self.tk.call((self.name, 'filter', filter.name) +
                            self._options(kw))
        
    def formant(self, **kw):
        """Returns a list of formant trajectories."""
        result = self.tk.call((self.name, 'formant') + self._options(kw))
        return map(self._getdoubles, self.tk.splitlist(result))
    
    def flush(self):
        """Removes all audio data from the sound."""
        self.tk.call(self.name, 'flush')

    def info(self, format='string'):
        """Returns a list with information about the sound.  The entries are
        [length, rate, max, min, encoding, channels, fileFormat, headerSize]
        """
        result = self.tk.call(self.name, 'info')
        if format == 'list':
            return map(self._cast, string.split(result))
        else:
            return result
        
    def insert(self, sound, position, **kw):
        """Inserts sound at position."""
        self.tk.call((self.name, 'insert', sound.name, position) + self._options(kw))
    
    def length(self, n=None, **kw):
        """Gets/sets the length of the sound in number of samples (default)
        or seconds, as determined by the 'units' option."""
        if n is not None:
            result = self.tk.call((self.name, 'length', n) + self._options(kw))
        else:
            result = self.tk.call((self.name, 'length') + self._options(kw))
        return _cast(result)

    def load(self, filename, **kw):
        """Reads new sound data from a file.  Synonym for "read"."""
        self.tk.call((self.name, 'read', filename) + self._options(kw))
    
    def max(self, **kw):
        """Returns the largest positive sample value of the sound."""
        return _cast(self.tk.call((self.name, 'max') + self._options(kw)))

    def min(self, **kw):
        """Returns the largest negative sample value of the sound."""
        return _cast(self.tk.call((self.name, 'min') + self._options(kw)))

    def mix(self, sound, **kw):
        """Mixes sample data from another sound into self."""
        self.tk.call((self.name, 'mix', sound.name) + self._options(kw))

    def pause(self):
        """Pause current record/play operation.  Next pause invocation
        resumes play/record."""
        self.tk.call(self.name, 'pause')
        
    def pitch(self, method=None, **kw):
        """Returns a list of pitch values."""
        if method is None or method is "amdf" or method is "AMDF":
            result = self.tk.call((self.name, 'pitch') + self._options(kw))
            return self._getdoubles(result)
        else:
            result = self.tk.call((self.name, 'pitch', '-method', method) + 
                                  self._options(kw))
            return map(self._getdoubles, self.tk.splitlist(result))

    def play(self, **kw):
        """Plays the sound."""
        self.tk.call((self.name, 'play') + self._options(kw))

    def power(self, **kw):
        """Computes the FFT power spectrum of the sound (at the time
        given by the start option) and returns a list of power values."""
        result = self.tk.call((self.name, 'power')
                              + self._options(kw))
        return self._getdoubles(result)        

    def read(self, filename, **kw):
        """Reads new sound data from a file."""
        self.tk.call((self.name, 'read', filename) + self._options(kw))

    def record(self, **kw):
        """Starts recording data from the audio device into the sound object."""
        self.tk.call((self.name, 'record') + self._options(kw))

    def reverse(self, **kw):
        """Reverses a sound."""
        self.tk.call((self.name, 'reverse') + self._options(kw))

    def sample(self, index, left=None, right=None):
        """Without left/right, this gets the sample value at index.
        With left/right, it sets the sample value at index in the left
        and/or right channels."""
        if right is not None:
            if left is None:
                left = '?'
            opts = (left, right)
        elif left is not None:
            opts = (left,)
        else:
            opts = ()
        return _cast(self.tk.call((self.name, 'sample', index) + opts))
        
    def stop(self):
        """Stops current play or record operation."""
        self.tk.call(self.name, 'stop')

    def stretch(self, **kw):
        self.tk.call((self.name, 'stretch') + self._options(kw))

    def write(self, filename, **kw):
        """Writes sound data to a file."""
        self.tk.call((self.name, 'write', filename) + self._options(kw))


class AudioControllerSingleton(TkObject):
    """This class offers functions that control various aspects of the
    audio devices.
    It is written as a class instead of as a set of module-level functions
    so that we can piggy-back on the Tcl-interface functions in TkObject,
    and so that the user can invoke the functions in a way more similar to
    how they're invoked in Tcl, e.g., snack.audio.rates().
    It is intended that there only be once instance of this class, the
    one created in snack.initialize.
    """
    
    def __init__(self):
        self.tk = Tkroot.tk
    
    def encodings(self):
        """Returns a list of supported sample encoding formats for the
        currently selected device."""
        result = self.tk.call('snack::audio', 'encodings')
        return self.tk.splitlist(result)
        
    def rates(self):
        """Returns a list of supported sample rates for the currently
        selected device."""
        result = self.tk.call('snack::audio', 'frequencies')
        return self._getints(result)

    def frequencies(self):
        """Returns a list of supported sample rates for the currently
        selected device."""
        result = self.tk.call('snack::audio', 'frequencies')
        return self._getints(result)
        
    def inputDevices(self):
        """Returns a list of available audio input devices"""
        result = self.tk.call('snack::audio', 'inputDevices')
        return self.tk.splitlist(result)
        
    def playLatency(self, latency=None):
        """Sets/queries (in ms) how much sound will be queued up at any
        time to the audio device to play back."""
        if latency is not None:
            return _cast(self.tk.call('snack::audio', 'playLatency', latency))
        else:
            return _cast(self.tk.call('snack::audio', 'playLatency'))
            
    def pause(self):
        """Toggles between play/pause for all playback on the audio device."""
        self.tk.call('snack::audio', 'pause')
        
    def play(self):
        """Resumes paused playback on the audio device."""
        self.tk.call('snack::audio', 'play')
        
    def play_gain(self, gain=None):
        """Returns/sets the current play gain.  Valid values are integers
        in the range 0-100."""
        if gain is not None:
            return _cast(self.tk.call('snack::audio', 'play_gain', gain))
        else:
            return _cast(self.tk.call('snack::audio', 'play_gain'))
            
    def outputDevices(self):
        """Returns a list of available audio output devices."""
        result = self.tk.call('snack::audio', 'outputDevices')
        return self.tk.splitlist(result)
        
    def selectOutput(self, device):
        """Selects an audio output device to be used as default."""
        self.tk.call('snack::audio', 'selectOutput', device)
        
    def selectInput(self, device):
        """Selects an audio input device to be used as default."""
        self.tk.call('snack::audio', 'selectInput', device)
        
    def stop(self):
        """Stops all playback on the audio device."""
        self.tk.call('snack::audio', 'stop')

    def elapsedTime(self):
        """Return the time since the audio device started playback."""
        result = self.tk.call('snack::audio', 'elapsedTime')
        return self.tk.getdouble(result)

class Filter(TkObject):

    def __init__(self, name, *args, **kw):
        global Tkroot
        self.name = None
        if Tkroot:
            master = Tkroot
        else:
            raise RuntimeError, \
                 'Tk not intialized or not registered with Snack'
        self.tk = master.tk
        self.name = self.tk.call(('snack::filter', name) + args +
                                 self._options(kw))

    def configure(self, *args):
        """Configures the filter."""
        self.tk.call((self.name, 'configure') + args)
        
    def destroy(self):
        """Removes the Tcl command for the filter and frees its storage."""
        self.tk.call(self.name, 'destroy')
        
        
class MixerControllerSingleton(TkObject):

    """Like AudioControllerSingleton, this class is intended to have only
    a single instance object, which will control various aspects of the
    mixers."""
    
    def __init__(self):
        self.tk = Tkroot.tk
        
    def channels(self, line):
        """Returns a list with the names of the channels for the
        specified line."""
        result = self.tk.call('snack::mixer', 'channels', line)
        return self.tk.splitlist(result)
        
    def devices(self):
        """Returns a list of the available mixer devices."""
        result = self.tk.call('snack::mixer', 'devices')
        return self.tk.splitlist(result)
        
    def input(self, jack=None, tclVar=None):
        """Gets/sets the current input jack.  Optionally link a boolean
        Tcl variable."""
        opts = ()
        if jack is not None:
            opts = opts + jack
        if tclVar is not None:
            opts = opts + tclVar
        return self.tk.call(('snack::mixer', 'input') + opts)

    def inputs(self):
        """Returns a list of available input ports."""
        result = self.tk.call('snack::mixer', 'inputs')
        return self.tk.splitlist(result)
        
    def lines(self):
        """Returns a list with the names of the lines of the mixer device."""
        result = self.tk.call('snack::mixer', 'lines')
        return self.tk.splitlist(result)
        
    def output(self, jack=None, tclVar=None):
        """Gets/sets the current output jack.  Optionally link a boolean
        Tcl variable."""
        opts = ()
        if jack is not None:
            opts = opts + jack
        if tclVar is not None:
            opts = opts + tclVar
        return self.tk.call(('snack::mixer', 'output') + opts)

    def outputs(self):
        """Returns a list of available output ports."""
        result = self.tk.call('snack::mixer', 'outputs')
        return self.tk.splitlist(result)
        
    def update(self):
        """Updates all linked variables to reflect the status of the
        mixer device."""
        self.tk.call('snack::mixer', 'update')
        
    def volume(self, line, leftVar=None, rightVar=None):
        if self.channels(line)[0] == 'Mono':
            return self.tk.call('snack::mixer', 'volume', line, rightVar)
        else:
            return self.tk.call('snack::mixer', 'volume', line, leftVar, rightVar)
        
    def select(self, device):
        """Selects a device to be used as default."""
        self.tk.call('snack::mixer', 'select', device)
        


class SoundFrame(Tkinter.Frame):
    
    """A simple "tape recorder" widget."""
    
    def __init__(self, parent=None, sound=None, *args, **kw):
        Tkinter.Frame.__init__(self)
        if sound:
            self.sound = sound
        else:
            self.sound = Sound()
        self.canvas = SnackCanvas(self, height=100)
        kw['sound'] = self.sound.name
        self.canvas.create_waveform(0, 0, kw)
        self.canvas.pack(side='top')
        bbar = Tkinter.Frame(self)
        bbar.pack(side='left')
        Tkinter.Button(bbar, image='snackOpen', command=self.load
                       ).pack(side='left')
        Tkinter.Button(bbar, bitmap='snackPlay', command=self.play
                       ).pack(side='left')
        Tkinter.Button(bbar, bitmap='snackRecord', fg='red',
                       command=self.record).pack(side='left')
        Tkinter.Button(bbar, bitmap='snackStop', command=self.stop
                       ).pack(side='left')
        Tkinter.Button(bbar, text='Info', command=self.info).pack(side='left')

        
    def load(self):
        file = Tkroot.tk.call('eval', 'snack::getOpenFile')
        self.sound.read(file, progress='snack::progressCallback')
        
    def play(self):
        self.sound.play()
        
    def stop(self):
        self.sound.stop()        
        
    def record(self):
        self.sound.record()
        
    def info(self):
        print self.sound.info()
        
def createSpectrogram(canvas, *args, **kw):
    """Draws a spectrogram of a sound on canvas."""
    return canvas._create('spectrogram', args, kw)

def createSection(canvas, *args, **kw):
    """Draws and FFT log power spectrum section on canvas."""
    return canvas._create('section', args, kw)

def createWaveform(canvas, *args, **kw):
    """Draws a waveform on canvas."""
    return canvas._create('waveform', args, kw)


class SnackCanvas(Tkinter.Canvas):
    
    def __init__(self, master=None, cnf={}, **kw):
        Tkinter.Widget.__init__(self, master, 'canvas', cnf, kw)
    
    def create_spectrogram(self, *args, **kw):
        """Draws a spectrogram of a sound on the canvas."""
        return self._create('spectrogram', args, kw)

    def create_section(self, *args, **kw):
        """Draws an FFT log power spectrum section."""
        return self._create('section', args, kw)

    def create_waveform(self, *args, **kw):
        """Draws a waveform."""
        return self._create('waveform', args, kw)


if __name__ == '__main__':
    # Create a test SoundFrame if the module is called as the main program
    root = Tkinter.Tk()
    initializeSnack(root)
    frame = SoundFrame(root)
    frame.pack(expand=0)
    root.mainloop()
