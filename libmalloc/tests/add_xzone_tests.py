#!/usr/bin/env python3

import argparse
import copy
import plistlib
from os.path import exists

def create_xzone_test(test, nano_on_xzone=False, force_pgm=False):
    xzone_test = copy.deepcopy(test)

    extension = 'nano-on-xzone' if nano_on_xzone else 'xzone'
    if force_pgm:
        extension += '.pgm'

    if 'TestName' in xzone_test:
        orig_name = xzone_test['TestName']
        xzone_test['TestName'] = orig_name + '.' + extension

    if 'CanonicalName' in xzone_test:
        # The CanonicalName for darwintests has a .<arch> suffix that we
        # want to keep at the end, so insert our new component just
        # before that
        orig_name = xzone_test['CanonicalName']
        components = orig_name.split('.')
        if len(components) > 1:
            components.insert(-1, extension)
            new_name = '.'.join(components)
        else:
            new_name = orig_name + '.' + extension

        xzone_test['CanonicalName'] = new_name

    envvars = [
        'MallocAllowInternalSecurity=1',
        'MallocSecureAllocator=1',
    ]

    if nano_on_xzone:
        envvars.append('MallocSecureAllocatorNano=0')
        envvars.append('MallocNanoOnXzone=1')
    else:
        envvars.append('MallocSecureAllocatorNano=1')

    if force_pgm:
        # If we're forcing PGM, it should already be disabled in the test
        # we're starting with
        assert('MallocProbGuard=0' in xzone_test['ShellEnv'])
        xzone_test['ShellEnv'].remove('MallocProbGuard=0')
        envvars.append('MallocProbGuard=1')

    if 'perf' not in xzone_test['Tags'] and \
            'no_debug' not in xzone_test['Tags'] and \
            not nano_on_xzone:
        # This isn't a performance test or otherwise incompatible with
        # the debug variant of the library, so we can use it for extra
        # assert coverage.
        envvars.append('DYLD_IMAGE_SUFFIX=_debug')

    extend_env(xzone_test, envvars)

    # xzone tests aren't eligible for VM testing because
    # VMs report a single CPU cluster, which xzone malloc
    # doesn't support
    new_tags = []
    for tag in test["Tags"]:
        if tag == 'VM_PREFERRED':
            new_tags.append("VM_NOT_PREFERRED")
        else:
            new_tags.append(tag)

    xzone_test["Tags"] = new_tags

    return xzone_test

def add_xzone_tests(bats_plist_path, disable_xzone):
    with open(bats_plist_path, 'rb') as bats_plist_file:
        orig_bats_plist = plistlib.load(bats_plist_file)

    # Copy all the top-level keys as-is except for the Tests list
    new_bats_plist = { k: v for k, v in orig_bats_plist.items() if k != 'Tests' }

    tests = []
    for test in orig_bats_plist['Tests']:
        disable_pgm(test)
        # Keep the original test, unless it's xzone-only
        if 'Tags' not in test or 'xzone_only' not in test['Tags']:
            test_copy = copy.deepcopy(test)

            if 'Tags' not in test or 'no_allocator_override' not in test['Tags']:
                # Explicitly disable xzone malloc, so that the old allocator is
                # still tested on platforms that have xzone malloc by default
                envvars = [
                    'MallocAllowInternalSecurity=1',
                    'MallocSecureAllocator=0',
                ]
                extend_env(test_copy, envvars)

            tests.append(test_copy)

        if 'Tags' in test and not disable_xzone:
            if 'xzone' in test['Tags'] or 'xzone_only' in test['Tags']:
                # This test has been tagged to run with xzone malloc
                xzone_test = create_xzone_test(test)
                tests.append(xzone_test)

                if 'xzone_and_pgm' in test['Tags']:
                    xzone_and_pgm_test = create_xzone_test(test,
                            nano_on_xzone=False, force_pgm=True)
                    tests.append(xzone_and_pgm_test)

            if 'nano_on_xzone' in test['Tags']:
                nano_on_xzone_test = create_xzone_test(test, nano_on_xzone=True)
                tests.append(nano_on_xzone_test)

                if 'xzone_and_pgm' in test['Tags']:
                    pgm_on_nano_on_xzone_test = create_xzone_test(test,
                            nano_on_xzone=True, force_pgm=True)
                    tests.append(pgm_on_nano_on_xzone_test)

    new_bats_plist['Tests'] = tests

    with open(bats_plist_path, 'wb') as bats_plist_file:
        plistlib.dump(new_bats_plist, bats_plist_file, fmt=plistlib.FMT_BINARY)


# Note: The env is a list of `key=value`, not a dictionary
def has_env_var(test, env_var_name):
    env = test.get('ShellEnv', [])
    key = env_var_name + '='  # exact match required
    return any(v.startswith(key) for v in env)


def extend_env(test, env_vars):
    if 'ShellEnv' in test:
        test['ShellEnv'].extend(env_vars)
    else:
        test['ShellEnv'] = env_vars


# Disable PGM by setting `MallocProbGuard=0`, but respect existing uses
def disable_pgm(test):
    if not has_env_var(test, 'MallocProbGuard'):
        extend_env(test, ['MallocProbGuard=0'])


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
