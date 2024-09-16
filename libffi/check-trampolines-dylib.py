#!/usr/bin/python3

from __future__ import print_function

import argparse
import subprocess
import re
import sys

from macholib.MachO import MachO
from macholib.SymbolTable import SymbolTable
from macholib.ptypes import *
from macholib import mach_o
import macholib

class build_version_command(Structure):
    _fields_ = (
        ('platform', p_uint32),
        ('minos', p_uint32),
        ('sdk', p_uint32),
        ('ntools', p_uint32))

if 0x32 not in mach_o.LC_REGISTRY:
    mach_o.LC_REGISTRY[0x32] = build_version_command

parser = argparse.ArgumentParser()
parser.add_argument("dylib")
args = parser.parse_args()

macho = MachO(args.dylib)

syms = None

for header_index, header in enumerate(macho.headers):

    symtab = SymbolTable(macho, header=header)
    for nlist, name in symtab.nlists:
        if name == b'page_max_size':
            PAGE_MAX_SIZE = nlist.n_value
            break
    else:
        raise Exception("didn't find symbol named 'page_max_size'")

    for lc, cmd, data in header.commands:
        # mach_o.LC_SEGMENT_64 is only applicable for 64 bit address size. 
        # We should also check LC_SEGMENT for 32 bit.
        if lc.cmd == mach_o.LC_SEGMENT_64 or mach_o.LC_SEGMENT:
            if cmd.segname.rstrip(b'\x00') == b"__TEXT":
                if cmd.vmsize != 2 * PAGE_MAX_SIZE:
                    raise Exception("__TEXT segment size is wrong")
                if len(data) != 1:
                    raise Exception("__TEXT segment has more than one section")
                sect = data[0]
                if sect.addr != PAGE_MAX_SIZE:
                    raise Exception("__text section has wrong address")
                if sect.size != PAGE_MAX_SIZE:
                    raise Exception("__text section has wrong size")
                break
    else:
        raise Exception("no __TEXT segment!")

