""" Extraordinarily crude run-oneself test

You'll need ``nose`` installed to run this.

Run all tests with ``nosetests bdist_mpkg`` from the root directory (containing
the ``setup.py`` file).
"""
from __future__ import with_statement

import sys
import os
import pwd
from os.path import dirname, join as pjoin, isfile, isdir
from subprocess import check_call
from shutil import rmtree

from ..tools import reown_paxboms, find_paxboms, unpax, ugrp_path

from ..tmpdirs import TemporaryDirectory

from nose import SkipTest
from nose.tools import assert_true, assert_equal

MY_PATH = dirname(__file__)
MY_PYTHON = sys.executable

def test_myself():
    # Build myself into a temporary directory
    os.chdir(pjoin(MY_PATH, '..', '..'))
    if not isfile('setup.py'):
        raise SkipTest('Not running from development directory')
    with TemporaryDirectory() as tmpdir:
        cmd = '%s setup.py bdist_mpkg --dist-dir=%s' % (MY_PYTHON, tmpdir)
        check_call(cmd, shell=True)
        tmpls = os.listdir(tmpdir)
        assert_equal(len(tmpls), 1)
        mpkg_dir = tmpls[0]
        assert_true(mpkg_dir.endswith('.mpkg'))
        assert_true(isdir(pjoin(tmpdir, mpkg_dir)))
        # Check zipping
        cmd += ' -z'
        check_call(cmd, shell=True)
        tmpls = sorted(os.listdir(tmpdir))
        assert_equal(len(tmpls), 2)
        assert_equal(tmpls[0], mpkg_dir)
        assert_true(tmpls[1].endswith('.zip'))
        assert_true(isfile(pjoin(tmpdir, tmpls[1])))
        # Try reperming
        mpkg_dir = pjoin(tmpdir, mpkg_dir)
        # Find paxboms
        paxboms = list(find_paxboms(mpkg_dir))
        assert_equal(len(paxboms), 2) # scripts and library
        # Extract them and check permissions
        paxdir = pjoin(tmpdir, 'pax_files')
        grps = []
        for pxbom in paxboms:
            rmtree(paxdir, ignore_errors=True)
            os.mkdir(paxdir)
            unpax(pxbom[0], paxdir)
            user, grp = ugrp_path(pjoin(paxdir, os.listdir(paxdir)[0]))
            assert_equal(user, pwd.getpwuid(os.getuid())[0])
            grps.append(grp)
        assert_equal([g for g in grps if g != grp], [])
        # Do paxbom permission change
        reown_paxboms(mpkg_dir, user, "everyone")
        for pxbom in paxboms:
            rmtree(paxdir, ignore_errors=True)
            os.mkdir(paxdir)
            unpax(pxbom[0], paxdir)
            user, grp = ugrp_path(pjoin(paxdir, os.listdir(paxdir)[0]))
            assert_equal(user, pwd.getpwuid(os.getuid())[0])
            assert_equal(grp, 'everyone')
