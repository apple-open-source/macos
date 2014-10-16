#!/usr/bin/env python

'''
xcodebuild seems to pass OTHER_LDFLAGS to both ld and libtool, but libtool
understands a small subset. This strips out arguments that libtool will choke
on.
'''

import sys, os

def strip(args, flag):
  try:
    args.remove(flag)
  except ValueError:
    pass

args = [ 'xcrun', 'libtool' ] + sys.argv[1:]

strip(args, '-fsanitize=address')
strip(args, '-Wl,-exported_symbol,___asan_mapping_offset')
strip(args, '-Wl,-exported_symbol,___asan_mapping_scale')

os.execv('/usr/bin/xcrun', args)
