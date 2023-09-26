#!/usr/bin/env python3

import copy
import plistlib
import sys

def add_xzone_tests(bats_plist_path):
    with open(bats_plist_path, 'rb') as bats_plist_file:
        orig_bats_plist = plistlib.load(bats_plist_file)

    # Copy all the top-level keys as-is except for the Tests list
    new_bats_plist = { k: v for k, v in orig_bats_plist.items() if k != 'Tests' }

    tests = []
    for test in orig_bats_plist['Tests']:
        # Take the original test exactly as it was, unless it's xzone-only
        if 'Tags' not in test or 'xzone_only' not in test['Tags']:
            tests.append(test)

        if 'Tags' in test and ('xzone' in test['Tags'] or 'xzone_only' in test['Tags']):
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

            envvars = ['MallocSecureAllocator=1']
            if 'perf' not in xzone_test['Tags']:
                # This isn't a performance test, so we can use the debug variant
                # of the library for extra assert coverage
                envvars.append('DYLD_LIBRARY_PATH=/AppleInternal/Tests/libmalloc/assets')
                envvars.append('DYLD_IMAGE_SUFFIX=_testdebug')

                # However, using the debug variant does prevent leaks from
                # working, so don't bother trying
                envvars.append('DT_BYPASS_LEAKS_CHECK=1')

            if 'ShellEnv' in xzone_test:
                xzone_test['ShellEnv'].extend(envvars)
            else:
                xzone_test['ShellEnv'] = envvars

            tests.append(xzone_test)

    new_bats_plist['Tests'] = tests

    with open(bats_plist_path, 'wb') as bats_plist_file:
        plistlib.dump(new_bats_plist, bats_plist_file, fmt=plistlib.FMT_BINARY)

if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit('Expected bats plist as argument')

    add_xzone_tests(sys.argv[1])
