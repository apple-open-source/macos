#!/usr/bin/env python
DESCRIP = 'Change ownership of files in mpkg archives'
EPILOG = \
"""Unpack archive files, change ownership, pack again

This may be useful for post-processing an mpkg directory.

For example, say you wanted to make the files in the directory belong to root;
then you could first build the package as non-root (say on buildbots), and then
fetch the package to a safe machine, and do::

    sudo reown_mpkg path_to_built.mpkg root admin

to fix up the permissions.
"""
from os.path import isdir

from argparse import (ArgumentParser, RawDescriptionHelpFormatter)

from .tools import reown_paxboms

def main():
    parser = ArgumentParser(description=DESCRIP,
                            epilog=EPILOG,
                            formatter_class=RawDescriptionHelpFormatter)
    parser.add_argument('mpkg_path', type=str,
                        help='mpkg directory')
    parser.add_argument('user', type=str,
                        help='username to become owner')
    parser.add_argument('group', type=str,
                        help='group to become owner')
    args = parser.parse_args()
    if not isdir(args.mpkg_path):
        raise RuntimeError('No directory "%s"' % args.mpkg_path)
    reown_paxboms(args.mpkg_path, args.user, args.group)


if __name__ == '__main__':
    main()
