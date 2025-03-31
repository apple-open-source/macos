#!/usr/bin/env python

from setuptools import setup

import os

# BEFORE importing distutils, remove MANIFEST. distutils doesn't properly
# update it when the contents of directories change.
if os.path.exists('MANIFEST'): os.remove('MANIFEST')

# Get version and release info, which is all stored in bdist_mpkg/info.py
ver_file = os.path.join('bdist_mpkg', 'info.py')
VARS = {}
exec(open(ver_file).read(), VARS)

setup(
    name="bdist_mpkg",
    version=VARS['__version__'],
    description=VARS['DESCRIPTION'],
    long_description=VARS['LONG_DESCRIPTION'],
    classifiers=VARS['CLASSIFIERS'],
    author="Bob Ippolito",
    author_email="pythonmac-sig@python.org",
    url="https://github.com/matthew-brett/bdist_mpkg",
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
            'reown_mpkg = bdist_mpkg.script_reown_mpkg:main',
        ],
    },
)
