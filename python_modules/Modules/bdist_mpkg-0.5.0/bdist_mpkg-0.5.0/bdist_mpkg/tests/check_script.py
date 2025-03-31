#!/usr/bin/env python
""" Script requires interactive sudo password

Checks that installer installs
"""

import sys
from os import chdir
from os.path import isfile, isdir, dirname
from glob import glob
from distutils import sysconfig

from subprocess import check_call
from functools import partial

from bdist_mpkg.tools import is_framework_python

from nose import SkipTest
from nose.tools import assert_true, assert_false, assert_equal

call = partial(check_call, shell=True)

VOL_NAME = 'check_install'
IMG_PATH = '/Volumes/' + VOL_NAME

MYDIR = dirname(__file__)
chdir(MYDIR + '/../..')
if not isfile('setup.py'):
    raise SkipTest(
""" These tests run from the development repository, or an unpacked sdist.
They won't run from an installed copy""")


def install2dmg(dmg_path, opts_str=''):
    # Make, get mpkg installer in dist
    call('python setup.py bdist_mpkg ' + opts_str)
    post_mpkgs = glob('dist/*.mpkg')
    assert_equal(len(post_mpkgs), 1)
    new_mpkg = post_mpkgs.pop()
    # Make disk image to install to
    call('hdiutil create -size 5m -fs HFS+ -volname %s %s'
        % (VOL_NAME, dmg_path))
    # Mount disk image
    call('hdiutil mount ' + dmg_path)
    # Install
    call('sudo installer -pkg %s -target %s' %
        (new_mpkg, IMG_PATH))

# Nuclear clean
call('git clean -fxd')
# Build mpkg and install to dmg
install2dmg('build/check_install.dmg')
# Check
assert_true(isdir("%s%s/bdist_mpkg"
                  % (IMG_PATH, sysconfig.get_python_lib())))
if is_framework_python():
    assert_true(isfile("%s/usr/local/bin/bdist_mpkg" % (IMG_PATH,)))
else:
    assert_true(isfile("%s%s/bin/bdist_mpkg" % (IMG_PATH, sys.exec_prefix)))
call('hdiutil unmount ' + IMG_PATH)
# Check not frameworking
install2dmg('build/check_nf_install.dmg', '--local-scripts')
# Check
assert_true(isdir("%s%s/bdist_mpkg"
                  % (IMG_PATH, sysconfig.get_python_lib())))
assert_false(isfile("%s/usr/local/bin/bdist_mpkg" % (IMG_PATH,)))
assert_true(isfile("%s%s/bin/bdist_mpkg" % (IMG_PATH, sys.exec_prefix)))
call('hdiutil unmount ' + IMG_PATH)
