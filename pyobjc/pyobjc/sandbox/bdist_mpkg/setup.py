#!/usr/bin/env python

import ez_setup
ez_setup.use_setuptools()

from setuptools import setup, Extension

VERSION = '0.4.3'
DESCRIPTION = "Builds Mac OS X installer packages from distutils"
LONG_DESCRIPTION = """
bdist_mpkg is a distutils plugin that implements the bdist_mpkg command,
which builds a Mac OS X metapackage for use by Installer.app for easy GUI
installation of Python modules, much like bdist_wininst.

It also comes with a bdist_mpkg script, which is a setup.py front-end that
will allow you to easy build an installer metapackage from nearly any existing
package that uses distutils.
"""

CLASSIFIERS = filter(None, map(str.strip,
"""                 
Intended Audience :: Developers
License :: OSI Approved :: MIT License
Programming Language :: Python
Operating System :: MacOS :: MacOS X
Topic :: Software Development :: Libraries :: Python Modules
Topic :: Software Development :: Build Tools
""".splitlines()))

setup(
    name="bdist_mpkg",
    version=VERSION,
    description=DESCRIPTION,
    long_description=LONG_DESCRIPTION,
    classifiers=CLASSIFIERS,
    author="Bob Ippolito",
    author_email="bob@redivi.com",
    url="http://undefined.org/python/#bdist_mpkg",
    license="MIT License",
    packages=['bdist_mpkg'],
    platforms=['any'],
    zip_safe=True,
    entry_points={
        'distutils.commands': [
            'bdist_mpkg = bdist_mpkg.cmd_bdist_mpkg:bdist_mpkg',
        ],
        'console_scripts': [
            'bdist_mpkg = bdist_mpkg.script_bdist_mpkg:main',
        ],
    },
)
