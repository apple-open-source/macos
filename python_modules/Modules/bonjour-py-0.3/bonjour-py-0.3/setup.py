"""
Bonjour module
""" 
from distutils.core import setup, Extension
from distutils.spawn import spawn
from distutils.sysconfig import get_python_inc
from distutils.sysconfig import get_config_vars
from distutils.util import get_platform
from os import environ, popen4
from os.path import isfile
import sys, string, re, exceptions, glob, os
from time import gmtime, strftime, time

begin_time = time()

RUN_SWIG      = 1
WARNINGS_ON   = 0
COMPILE_CC    = 0
DEBUG         = 0
RUN_EPYDOC    = 0
RUN_EPYDOCIFY = 1
HelpText ="""
***bonjour setup.py*** 
--h               This help text
--run-swig        Run SWIG to generate bonjour wrapper code.
                  Note: Only needs to be called if SWIG interface
                        source or the swig interface to it
                        has been modified.
--debug           Build with debug options enabled.
--run-epydoc      Generate epydoc reference documentation.
"""


if sys.platform == 'win32':
    import distutils.msvccompiler

    def replace(former=distutils.msvccompiler.get_build_version):
        try:
            return float(os.environ['MSVC_VERSION'])
        except KeyError, e:
            return former()

    distutils.msvccompiler.get_build_version = replace


# --------------------------------------------------
# swig definitions
# --------------------------------------------------
# Distutils doesn't currently work with recent 1.3.x
# versions of swig.
def check_swig_version():
    inStrm, outStrm = popen4("swig -version")
    for line in outStrm:
        SWIG_PATTERN = re.compile(r"\s*SWIG Version 1\.1+")
        m = SWIG_PATTERN.match(line)
	if m:
	     raise SwigVersionException("You need to be using Swig version 1.3 or greater")  

swigList = [ ['bonjour.i', 'bonjour_wrap.c'] ]
def run_swig(swigList):
    for s in swigList:
        cmd = ["swig"] + SWIG_ARGS + ["-o", "%s" % (s[1]), "%s" % (s[0])]
        print "Running swig with: |%s|" % cmd
        spawn(cmd, verbose=1)

class SwigVersionException(exceptions.Exception):
    pass





#
# Real processing begins here
#
if sys.platform == 'win32':
    SRC = 'c:\program files\\bonjour sdk'

# --------------------------------------------------
# Parse special command line options and remove them
# --------------------------------------------------
args = sys.argv[1:]
for arg in args:
    if string.find(arg, '--help') == 0:
        sys.argv.remove(arg)
        print HelpText
        assert 0, '--help called. Exiting.'
    if string.find(arg, '--debug') == 0:
	DEBUG = 1
	sys.argv.remove(arg)
    elif string.find(arg, '--src') == 0:
        SRC = string.split(arg, '=')[1]
        sys.argv.remove(arg)
        

SWIG_ARGS = [ "-python", "-outdir", "bonjour", "-new_repr", 
              "-I%s" % get_python_inc(plat_specific=0),
              ]
# --------------------------------------------------
# Set up compiler options
# --------------------------------------------------
if sys.platform == 'win32':
    incDirList = [os.path.join(os.path.abspath(SRC),'include')]
    print "incDirList = ", incDirList
    libDirList = [os.path.join(os.path.abspath(SRC), "lib") ]
    libList = [ "dnssd" ]
elif sys.platform == 'linux2':
    incDirList = []
    libDirList = []
    libList = ["dns_sd"]
elif sys.platform == 'freebsd5' or sys.platform == 'freebsd6':
    incDirList = [os.path.join(os.path.abspath('/usr'), 'local', 'include')] 
    libDirList = [os.path.join(os.path.abspath('/usr'), 'local', 'lib')]
    libList = ["dns_sd"]
elif sys.platform == 'darwin':
    incDirList = libDirList = libList = []

opts = get_config_vars()
#for k,v in opts.items():
#    print "K: %s V: %s" %(k,v)
    
if sys.platform == 'win32':
    cFlagsList = [ "/DWIN32" ]
    if DEBUG:
        cFlagsList.append('/DDEBUG')
    ldFlagsList = [ "/force:multiple" ]
else:
    if DEBUG:
        cflags = '-g -DDEBUG'
        cflags = cflags.replace("-O3", "")
        opts["OPT"] = cflags
                
    cFlagsList = []
    ldFlagsList = []
    
if not os.path.exists('bonjour'):
    os.mkdir('bonjour')

# --------------------------------------------------
# Run swig
# --------------------------------------------------
if RUN_SWIG:
    check_swig_version()
    run_swig(swigList)
    if os.path.exists('bonjour/__init__.py'):
        os.unlink('bonjour/__init__.py')
    os.rename('bonjour/bonjour.py','bonjour/__init__.py')

# --------------------------------------------------
# Build/install the module
# --------------------------------------------------
setup(name="PACKAGENAME", version="1.0",
      description="Apple Bonjour Library wrapper",
      author="Thomas D. Uram", author_email="turam@mcs.anl.gov",
      url="http://www.mcs.anl.gov/fl",
      ext_modules=[Extension("bonjour._bonjour", ["bonjour_wrap.c"],
                             include_dirs=incDirList,
                             library_dirs=libDirList,
                             extra_compile_args=cFlagsList,
                             extra_link_args=ldFlagsList,
                             libraries=libList)],
      py_modules=['bonjour.__init__'],
       )

os.unlink(os.path.join('bonjour','__init__.py'))
os.rmdir('bonjour')

    
print 'Build finished at: ', strftime("%a, %d %b %Y %H:%M:%S +0000", gmtime())
print 'Completed in', time() - begin_time, 'seconds'
