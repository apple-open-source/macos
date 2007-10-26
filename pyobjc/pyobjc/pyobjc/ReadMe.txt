This is an umbrella package for easily install all of PyObjC, it doesn't 
contain any useful features by itself.

Please check the documentation and examples and the various pyobjc subpackages
for more information.

To create a binary installer and install that:

$ python setup.py bdist_mpkg --open

This will install PyObjC as well as a recent copy of py2app, but does require
that you use a checkout of all these packages.

To build just the meta-egg:

$ python setup.py bdist_egg

In either case you have to remove the directory pyobjc.egg-info before running
the command.
