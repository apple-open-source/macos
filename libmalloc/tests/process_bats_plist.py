#!/usr/bin/env python3

import argparse
import copy
import os
import plistlib
import sys

def write_plist(plist_path, content):
    with open(plist_path, 'wb') as plist_file:
        plistlib.dump(content, plist_file, fmt=plistlib.FMT_BINARY)

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

# Set MallocReportConfig=1 if not already present
def enable_report_config(test):
    if not has_env_var(test, 'MallocReportConfig'):
        extend_env(test, ['MallocReportConfig=1', 'MallocDebugReport=stderr'])

# Exclude tests with the following characteristic from being re-run under PGM:
# - T_META_NAMESPACE("pgm"): PGM implementation tests, name starts with "libmalloc.pgm"
# - T_META_ENVVAR("MallocProbGuard=X"): these tests already specify PGM state
def pgm_compatible(test):
    if test.get('TestName', '').startswith('libmalloc.pgm'):
        return False

    if has_env_var(test, 'MallocProbGuard'):
        return False

    return True

def create_allocator_test(orig_test, extension, extra_envvars=[],
        use_debug_dylib=False, remove_perf_tag=False):
    new_test = copy.deepcopy(orig_test)

    if 'TestName' in orig_test:
        orig_name = orig_test['TestName']
        new_test['TestName'] = orig_name + '.' + extension

    if 'CanonicalName' in orig_test:
        # The CanonicalName for darwintests has a .<arch> suffix that we
        # want to keep at the end, so insert our new component just
        # before that
        orig_name = orig_test['CanonicalName']
        components = orig_name.split('.')
        if len(components) > 1:
            components.insert(-1, extension)
            new_name = '.'.join(components)
        else:
            new_name = orig_name + '.' + extension

        new_test['CanonicalName'] = new_name

    envvars = [
        'MallocReportConfig=1',
        'MallocDebugReport=stderr',
        'MallocAllowInternalSecurity=1',
    ]

    envvars += extra_envvars

    if 'MallocProbGuard=1' not in extra_envvars and \
            not has_env_var(orig_test, 'MallocProbGuard'):
        envvars.append('MallocProbGuard=0')

    if use_debug_dylib and 'perf' not in orig_test['Tags'] and \
            'no_debug' not in orig_test['Tags']:
        # This isn't a performance test or otherwise incompatible with
        # the debug variant of the library, so we can use it for extra
        # assert coverage.
        envvars.append('DYLD_IMAGE_SUFFIX=_debug')

    extend_env(new_test, envvars)

    if remove_perf_tag:
        tags = new_test.get('Tags', [])
        if 'perf' in tags:
            tags.remove('perf')

    return new_test

def create_magazine_test(orig_test):
    return create_allocator_test(orig_test, 'magazine',
            extra_envvars=['MallocSecureAllocator=0'])

def create_xzone_test(orig_test, add_pgm=False):
    envvars = [
        'MallocSecureAllocator=1',
        'MallocSecureAllocatorNano=1',
    ]

    if add_pgm:
        if has_env_var(orig_test, 'MallocProbGuard'):
            sys.exit('Test %s already specifies MallocProbGuard' %
                    orig_test.get('TestName', '<unknown>'))
        envvars.append('MallocProbGuard=1')

    return create_allocator_test(orig_test, 'xzone', extra_envvars=envvars,
            use_debug_dylib=True)

def create_pgm_test(orig_test, timeout_risk):
    new_test = create_allocator_test(orig_test, 'pgm',
            extra_envvars=['MallocProbGuard=1'], remove_perf_tag=True)

    if timeout_risk:
        new_test['Timeout'] *= 10
    else:
        extend_env(new_test, ['MallocProbGuardSampleRate=5'])

    return new_test


TEST_TAG_ALL_ALLOCATORS = 'all_allocators'
TEST_TAG_NO_ALLOCATOR_OVERRIDE = 'no_allocator_override'
TEST_TAG_XZONE_ONLY = 'xzone_only'
TEST_TAG_MAGAZINE_ONLY = 'magazine_only'

def process_bats_plist(bats_plist_path, output_directory):
    with open(bats_plist_path, 'rb') as bats_plist_file:
        orig_bats_plist = plistlib.load(bats_plist_file)

    main_plist = copy.deepcopy(orig_bats_plist)
    pgm_plist = copy.deepcopy(orig_bats_plist)

    main_tests = []
    pgm_tests = []


    for orig_test in orig_bats_plist['Tests']:
        testname = orig_test.get('TestName', '<unknown>')

        if testname == 'libmalloc.xctests':
            main_tests.append(orig_test)
            continue

        if 'Tags' not in orig_test:
            sys.exit('No tags for test %s' % testname)

        tags = set(orig_test['Tags'])

        vm_tags = {
            'VM_PREFERRED',
            'VM_NOT_PREFERRED',
            'VM_NOT_ELIGIBLE',
        }

        if len(tags & vm_tags) != 1:
            sys.exit('Test %s must be tagged with exactly one of %s, found %s' %
                     (testname, str(vm_tags), str(tags & vm_tags)))

        allocator_tags = {
            TEST_TAG_ALL_ALLOCATORS,
            TEST_TAG_NO_ALLOCATOR_OVERRIDE,
            TEST_TAG_XZONE_ONLY,
            TEST_TAG_MAGAZINE_ONLY,
        }

        if len(tags & allocator_tags) != 1:
            sys.exit('Test %s must be tagged with exactly one of %s, found %s' %
                     (testname, str(allocator_tags), str(tags & allocator_tags)))

        test_allocator_tag = list(tags & allocator_tags)[0]

        if test_allocator_tag == TEST_TAG_NO_ALLOCATOR_OVERRIDE:
            # These tests want to run with whatever the ambient allocator
            # configuration is, either because they specifically want to check
            # that or because they're unit tests that don't care.  We make only
            # minimal changes to these tests.
            new_test = copy.deepcopy(orig_test)
            disable_pgm(new_test)
            enable_report_config(new_test)
            main_tests.append(new_test)

        if test_allocator_tag == TEST_TAG_ALL_ALLOCATORS and \
                pgm_compatible(orig_test):
            # Tests that want to run with all allocator configurations and do
            # not explicitly specify their PGM state should be re-run with PGM
            # on top of the default allocator in the PGM suite
            pgm_timeout_risk_tests = {
                'libmalloc.threaded_stress_fork',
                'libmalloc.threaded_stress_fork_small',
            }
            timeout_risk = testname in pgm_timeout_risk_tests

            pgm_test = create_pgm_test(orig_test, timeout_risk)
            pgm_tests.append(pgm_test)

        if test_allocator_tag in { \
                TEST_TAG_MAGAZINE_ONLY, TEST_TAG_ALL_ALLOCATORS }:
            magazine_test = create_magazine_test(orig_test)
            main_tests.append(magazine_test)

        if test_allocator_tag in { \
                TEST_TAG_XZONE_ONLY, TEST_TAG_ALL_ALLOCATORS }:
            # Don't add xzone tests for arm64_32 (or any other 32-bit slices,
            # which we'll have to check for here should they appear in the
            # future)
            if orig_test.get('Arch', '') == 'arm64_32':
                continue

            xzone_test = create_xzone_test(orig_test)
            main_tests.append(xzone_test)

            if 'xzone_and_pgm' in tags:
                xzone_and_pgm_test = create_xzone_test(orig_test, add_pgm=True)
                main_tests.append(xzone_and_pgm_test)


    main_plist['Tests'] = main_tests
    # Rewrite the main plist in place
    write_plist(bats_plist_path, main_plist)

    pgm_plist['Tests'] = pgm_tests
    pgm_plist_path = os.path.join(output_directory, 'libmalloc_pgm.plist')
    write_plist(pgm_plist_path, pgm_plist)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Processes libmalloc BATS plist')
    parser.add_argument('bats_plist')
    parser.add_argument('output_directory')
    parser.add_argument('-p', '--platform', help='Platform tests are being run on ()')
    args = parser.parse_args()

    process_bats_plist(args.bats_plist, args.output_directory)
