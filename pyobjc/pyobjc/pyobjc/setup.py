''' 
PyObjC is a bridge between Python and Objective-C.  It allows full
featured Cocoa applications to be written in pure Python.  It is also
easy to use other frameworks containing Objective-C class libraries
from Python and to mix in Objective-C, C and C++ source.

Python is a highly dynamic programming language with a shallow learning
curve.  It combines remarkable power with very clear syntax.

PyObjC also supports full introspection of Objective-C classes and
direct invocation of Objective-C APIs from the interactive interpreter.

PyObjC requires MacOS X 10.3 or later.  PyObjC works both with the Apple
provided Python installation in MacOS X 10.3 (and later) and with
MacPython 2.3.  

Note: This is a meta-packages that will install the PyObjC engine as well as
a large number of wrappers for MacOS X frameworks.
'''

TODO='''
* Make sure the mpkg includes setuptools as well, and test installation on a clean
  system.
* Tweak bdist_mpkg to include useful descriptions for subpages
* Add a background image to this bdist_mpkg output, and update readme/welcome
* Add a useful readme to the DMG output, possibly add artwork as well.
* Due to a misfeature of setuptools this setup file only works when
  either all dependencies are available somewhere or no egg-info directory
  is present. Can we work around this?
'''

import ez_setup
ez_setup.use_setuptools()

from setuptools import setup, Command
from distutils.dep_util import newer
from distutils import log
from distutils.errors import DistutilsSetupError


import os, sys, shutil, subprocess


#from PyObjCMetaData.commands import extra_cmdclass

VERSION='1.5'
FRAMEWORKS='''\
        AddressBook
        AppKit
        AppleScriptKit
        Automator
        CalendarStore
        Cocoa
        Collaboration
        CoreData
        CoreFoundation
        CoreText
        DictionaryServices
        ExceptionHandling
        FSEvents
        Foundation
        IOBluetooth
        InputMethodKit
        InstallerPlugins
        InstantMessage
        InterfaceBuilder
        InterfaceBuilderKit
        LatentSemanticMapping
        LaunchServices
        Message
        PreferencePanes
        PubSub
        QTKit
        Quartz
        ScreenSaver
        ScriptingBridge
        SecurityInterface
        SenTestingKit
        SyncServices
        WebKit
        XgridFoundation
'''.split()

HDIUTIL = '/usr/bin/hdiutil'
if not os.path.exists(HDIUTIL):
    raise ImportError("hdiutil not present")

DMG_FILES = filter(None,
"""
Resources/Dmg/ReadMe.rtf
License.txt
NEWS.html
""".splitlines())

#
# A distutils command for creating a .DMG with the complete release.
#

class bdist_dmg(Command):
    description = "Create a Mac OS X disk image with the binary installer"
    user_options = [
        ('release-dir=', None,
         'Release directory [default: release]'),
    ]

    def initialize_options(self):
        self.release_dir = 'release'
        self.finalized = False

    def finalize_options(self):
        self.finalized = True

    def run(self):
        log.info("Creating a simple disk image")
        dist = self.distribution

        # Add OSX version again?
        ident = '%s-python%s' % (
            dist.get_fullname(),
            sys.version[:3],
        )
        release_temp = os.path.join(self.release_dir, ident)
        release_file = release_temp + '.dmg'

        mpkg = self.reinitialize_command('bdist_mpkg', reinit_subcommands=1)
        mpkg.dist_dir = release_temp
        mpkg.keep_temp = True
        mpkg.ensure_finalized()
        mpkg.run()
        for fn in DMG_FILES:
            if not os.path.exists(fn):
                log.warn("File %s doesn't exist", fn)
            
            else:
                self.copy_file(fn, os.path.join(release_temp, os.path.basename(fn)))

        log.info("Creating %s", release_file)
        status = subprocess.call([
                HDIUTIL, 'create', '-ov', 
                    '-imagekey', 'zlib-level=9', 
                    '-srcfolder', release_temp, 
                    release_file])
        if status != 0:
            raise DistutilsSetupError("Running diskutil failed")


def _subproject_schemas(frameworklist):
    result =  dict([ ('pyobjc-framework-%s'%(nm,), '../pyobjc-framework-%s/setup.py'%(nm,)) for nm in frameworklist ])
    result['pyobjc-metadata'] = '../pyobjc-metadata/setup.py'

    # PyObjC basically requires py2app to be useful, might as well include
    # it in the mpkg installer.
    result['py2app'] = '../py2app/setup.py'
    result['macholib'] = '../macholib/setup.py'
    result['modulegraph'] = '../modulegraph/setup.py'
    result['altgraph'] = '../altgraph/setup.py'
    result['pyobjc-core'] = '../pyobjc-core/setup.py'
    result['pyobjc-xcode'] = '../pyobjc-xcode/setup.py'
    return result

print "X"
setup(
    name='pyobjc',
    version=VERSION,
    description = "Objective-C bridge for Python",
    long_description = __doc__,
    author='bbum, RonaldO, SteveM, LeleG, many others stretching back through the reaches of time...',
    author_email='pyobjc-dev@lists.sourceforge.net',
    url='http://pyobjc.sourceforge.net',
    platforms = [ "MacOS X" ],
    packages = [],
    setup_requires = [ 
        'pyobjc-metadata>=0.1', 
        'bdist_mpkg>=0.4.1', 
        'docutils' 
    ],
    install_requires = [ 
        'pyobjc-core==%s'%(VERSION,),
        'pyobjc-xcode==%s'%(VERSION,),
    ] + [
        'pyobjc-framework-%s'%(nm,) #==%s'%(nm, VERSION) 
            for nm in FRAMEWORKS 
    ],
    dependency_links = [],
    #test_suite='',
    cmdclass = {
        'bdist_dmg': bdist_dmg,
#        'build_html': build_html,
    },

    # The package is actually zip-safe, but py2app isn't zip-aware yet.
    zip_safe = True,

    options=dict(
        bdist_mpkg=dict(
            welcome='Resources/Mpkg/Welcome.rtf',
            readme='Resources/Mpkg/Readme.rtf',
            #background='Resources/Mpkg/background.jpg',
            license='License.txt',
            scheme_subprojects=_subproject_schemas(FRAMEWORKS),
        ),
    ), 
)
