""" Test some tools
"""
from __future__ import with_statement

import os
from os.path import abspath, split as psplit, isfile, join as pjoin

from ..tools import pax, unpax, ugrp_path

from ..tmpdirs import InTemporaryDirectory

from nose.tools import assert_true, assert_equal

HERE, SELF = psplit(abspath(__file__))

def test_pax_unpax():
    with InTemporaryDirectory() as tmpdir:
        pax(HERE, tmpdir)
        pax_file = pjoin(tmpdir, 'Contents', 'Archive.pax.gz')
        assert_true(isfile(pax_file))
        os.mkdir('pax_contents')
        unpax(pax_file, 'pax_contents')
        assert_equal(set(os.listdir('pax_contents')),
                     set(os.listdir(HERE)))


def test_user_group():
    with InTemporaryDirectory() as tmpdir:
        open('test_file', 'wt').write('hello')
        assert_equal(ugrp_path(tmpdir), ugrp_path('test_file'))
