"""Easy to use dialogs.  Modified to avoid talking to the window server.
Only Message() is supported, and prints to stdout.  The other routines will
throw a NotImplementedError exception.

Message(msg) -- display a message and an OK button.
AskString(prompt, default) -- ask for a string, display OK and Cancel buttons.
AskPassword(prompt, default) -- like AskString(), but shows text as bullets.
AskYesNoCancel(question, default) -- display a question and Yes, No and Cancel buttons.
GetArgv(optionlist, commandlist) -- fill a sys.argv-like list using a dialog
AskFileForOpen(...) -- Ask the user for an existing file
AskFileForSave(...) -- Ask the user for an output file
AskFolder(...) -- Ask the user to select a folder
bar = Progress(label, maxvalue) -- Display a progress bar
bar.set(value) -- Set value
bar.inc( *amount ) -- increment value by amount (default=1)
bar.label( *newlabel ) -- get or set text label.

More documentation in each function.
This module uses DLOG resources 260 and on.
Based upon STDWIN dialogs with the same names and functions.
"""

import os
import sys

__all__ = ['Message', 'AskString', 'AskPassword', 'AskYesNoCancel',
    'GetArgv', 'AskFileForOpen', 'AskFileForSave', 'AskFolder',
    'ProgressBar']

def cr2lf(text):
    if '\r' in text:
        text = string.join(string.split(text, '\r'), '\n')
    return text

def lf2cr(text):
    if '\n' in text:
        text = string.join(string.split(text, '\n'), '\r')
    if len(text) > 253:
        text = text[:253] + '\311'
    return text

def Message(msg, id=260, ok=None):
    """Display a MESSAGE string.

    Return when the user clicks the OK button or presses Return.

    The MESSAGE string can be at most 255 characters long.
    """
    sys.stderr.write(msg+'\n')


def AskString(prompt, default = "", id=261, ok=None, cancel=None):
    """Display a PROMPT string and a text entry field with a DEFAULT string.

    Return the contents of the text entry field when the user clicks the
    OK button or presses Return.
    Return None when the user clicks the Cancel button.

    If omitted, DEFAULT is empty.

    The PROMPT and DEFAULT strings, as well as the return value,
    can be at most 255 characters long.
    """

    raise NotImplementedError("AskString")

def AskPassword(prompt,  default='', id=264, ok=None, cancel=None):
    """Display a PROMPT string and a text entry field with a DEFAULT string.
    The string is displayed as bullets only.

    Return the contents of the text entry field when the user clicks the
    OK button or presses Return.
    Return None when the user clicks the Cancel button.

    If omitted, DEFAULT is empty.

    The PROMPT and DEFAULT strings, as well as the return value,
    can be at most 255 characters long.
    """
    raise NotImplementedError("AskPassword")

def AskYesNoCancel(question, default = 0, yes=None, no=None, cancel=None, id=262):
    """Display a QUESTION string which can be answered with Yes or No.

    Return 1 when the user clicks the Yes button.
    Return 0 when the user clicks the No button.
    Return -1 when the user clicks the Cancel button.

    When the user presses Return, the DEFAULT value is returned.
    If omitted, this is 0 (No).

    The QUESTION string can be at most 255 characters.
    """

    raise NotImplementedError("AskYesNoCancel")

class ProgressBar:
    def __init__(self, title="Working...", maxval=0, label="", id=263):
	raise NotImplementedError("ProgressBar")

ARGV_ID=265
ARGV_ITEM_OK=1
ARGV_ITEM_CANCEL=2
ARGV_OPTION_GROUP=3
ARGV_OPTION_EXPLAIN=4
ARGV_OPTION_VALUE=5
ARGV_OPTION_ADD=6
ARGV_COMMAND_GROUP=7
ARGV_COMMAND_EXPLAIN=8
ARGV_COMMAND_ADD=9
ARGV_ADD_OLDFILE=10
ARGV_ADD_NEWFILE=11
ARGV_ADD_FOLDER=12
ARGV_CMDLINE_GROUP=13
ARGV_CMDLINE_DATA=14

def GetArgv(optionlist=None, commandlist=None, addoldfile=1, addnewfile=1, addfolder=1, id=ARGV_ID):
    raise NotImplementedError("GetArgv")

def SetDefaultEventProc(proc):
    raise NotImplementedError("SetDefaultEventProc")

def AskFileForOpen(
        message=None,
        typeList=None,
        # From here on the order is not documented
        version=None,
        defaultLocation=None,
        dialogOptionFlags=None,
        location=None,
        clientName=None,
        windowTitle=None,
        actionButtonLabel=None,
        cancelButtonLabel=None,
        preferenceKey=None,
        popupExtension=None,
        eventProc=None,
        previewProc=None,
        filterProc=None,
        wanted=None,
        multiple=None):
    """Display a dialog asking the user for a file to open.

    wanted is the return type wanted: FSSpec, FSRef, unicode or string (default)
    the other arguments can be looked up in Apple's Navigation Services documentation"""

    raise NotImplementedError("AskFileForOpen")

def AskFileForSave(
        message=None,
        savedFileName=None,
        # From here on the order is not documented
        version=None,
        defaultLocation=None,
        dialogOptionFlags=None,
        location=None,
        clientName=None,
        windowTitle=None,
        actionButtonLabel=None,
        cancelButtonLabel=None,
        preferenceKey=None,
        popupExtension=None,
        eventProc=None,
        fileType=None,
        fileCreator=None,
        wanted=None,
        multiple=None):
    """Display a dialog asking the user for a filename to save to.

    wanted is the return type wanted: FSSpec, FSRef, unicode or string (default)
    the other arguments can be looked up in Apple's Navigation Services documentation"""

    raise NotImplementedError("AskFileForSave")

def AskFolder(
        message=None,
        # From here on the order is not documented
        version=None,
        defaultLocation=None,
        dialogOptionFlags=None,
        location=None,
        clientName=None,
        windowTitle=None,
        actionButtonLabel=None,
        cancelButtonLabel=None,
        preferenceKey=None,
        popupExtension=None,
        eventProc=None,
        filterProc=None,
        wanted=None,
        multiple=None):
    """Display a dialog asking the user for select a folder.

    wanted is the return type wanted: FSSpec, FSRef, unicode or string (default)
    the other arguments can be looked up in Apple's Navigation Services documentation"""

    raise NotImplementedError("AskFolder")


def test():
    import time

    Message("Testing EasyDialogs.")

if __name__ == '__main__':
    try:
        test()
    except KeyboardInterrupt:
        Message("Operation Canceled.")
