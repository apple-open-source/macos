#!/usr/bin/env python3

import copy
import plistlib
import sys
import argparse
from os.path import exists

def add_xzone_tests(bats_plist_path, disable_xzone):
    with open(bats_plist_path, 'rb') as bats_plist_file:
        orig_bats_plist = plistlib.load(bats_plist_file)

    # Copy all the top-level keys as-is except for the Tests list
    new_bats_plist = { k: v for k, v in orig_bats_plist.items() if k != 'Tests' }

    tests = []
    for test in orig_bats_plist['Tests']:
        # Keep the original test, unless it's xzone-only
        if 'Tags' not in test or 'xzone_only' not in test['Tags']:
            test_copy = copy.deepcopy(test)
            # Explicitly disable xzone malloc, so that the old allocator is
            # still tested on platforms that have xzone malloc by default
            envvars = ['MallocSecureAllocator=0']
            if 'ShellEnv' in test_copy:
                test_copy['ShellEnv'].extend(envvars)
            else:
                test_copy['ShellEnv'] = envvars
            tests.append(test_copy)

        if 'Tags' in test and ('xzone' in test['Tags'] or 'xzone_only' in test['Tags']) and \
                not disable_xzone:
            # This test has been tagged to run with xzone malloc
            xzone_test = copy.deepcopy(test)

            if 'TestName' in xzone_test:
                orig_name = xzone_test['TestName']
                xzone_test['TestName'] = orig_name + '.xzone'

            if 'CanonicalName' in xzone_test:
                # The CanonicalName for darwintests has a .<arch> suffix that we
                # want to keep at the end, so insert our new component just
                # before that
                orig_name = xzone_test['CanonicalName']
                components = orig_name.split('.')
                if len(components) > 1:
                    components.insert(-1, 'xzone')
                    new_name = '.'.join(components)
                else:
                    new_name = orig_name + '.xzone'

                xzone_test['CanonicalName'] = new_name

            envvars = [
                'MallocSecureAllocator=1',
                'MallocSecureAllocatorNano=1',
            ]
            if 'perf' not in xzone_test['Tags'] and \
                    'no_debug' not in xzone_test['Tags'] and \
                    not xzone_test.get('TestName', '').startswith('libmalloc.pgm'):
                # This isn't a performance test or otherwise incompatible with
                # the debug variant of the library, so we can use it for extra
                # assert coverage.
                #
                # PGM tests can't use the debug variant until rdar://114584236
                # is addressed
                envvars.append('DYLD_IMAGE_SUFFIX=_debug')

            if 'ShellEnv' in xzone_test:
                xzone_test['ShellEnv'].extend(envvars)
            else:
                xzone_test['ShellEnv'] = envvars

            tests.append(xzone_test)

    new_bats_plist['Tests'] = tests

    with open(bats_plist_path, 'wb') as bats_plist_file:
        plistlib.dump(new_bats_plist, bats_plist_file, fmt=plistlib.FMT_BINARY)

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Adds xzone malloc tests to BATS plist')
    parser.add_argument('bats_plist')
    parser.add_argument('-p', '--platform', help='Platform tests are being run on ()')
    args = parser.parse_args()

    assert exists(args.bats_plist), 'Expected bats plist as argument'

    disable_xzone_tests = False
    if args.platform == 'WatchOS':
        # MallocSecureAllocator=1 is a no-op on arm64_32 (read: all watches),
        # so don't generate .xzone test variants on watchOS
        disable_xzone_tests = True

    add_xzone_tests(args.bats_plist, disable_xzone_tests)
