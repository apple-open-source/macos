import argparse
import plistlib


def parse_args():
    parser = argparse.ArgumentParser(
        description='Create PGM plist based on existing BATS plist')
    parser.add_argument('orig_plist')
    parser.add_argument('pgm_plist')
    return parser.parse_args()


# Exclude tests with the following characteristic from being re-run under PGM:
# - T_META_NAMESPACE("pgm"): PGM implementation tests, name starts with "libmalloc.pgm"
# - T_META_ENVVAR("MallocProbGuard=X"): these tests already specify PGM state
# - T_META_TAG_XZONE_ONLY: tests specific characteristics of xzone only
def pgm_compatible(test):
    if test.get('TestName', '').startswith('libmalloc.pgm'):
        return False
    if has_env_var(test, 'MallocProbGuard'):
        return False
    tags = test.get('Tags', [])
    incompatible_tags = {'xzone_only'}
    if any((t in incompatible_tags) for t in tags):
        return False
    return True


def filter_tests(plist):
    plist['Tests'] = list(filter(pgm_compatible, plist['Tests']))


def is_prone_to_timeout(test):
    return test.get('TestName', '') in {
        'libmalloc.threaded_stress_fork',
        'libmalloc.threaded_stress_fork_small',
    }


def add_pgm_activation(plist):
    for test in plist['Tests']:
        # Add PGM env vars
        extend_env(test, ['MallocProbGuard=1'])
        if is_prone_to_timeout(test):
            test['Timeout'] = 10 * test['Timeout']
        else:
            extend_env(test, ['MallocProbGuardSampleRate=5'])

        # Adapt `TestName` and `CanonicalName`
        suffix = '.pgm'
        test['TestName'] = test.get('TestName', '') + suffix
        test['CanonicalName'] = test.get('CanonicalName', '') + suffix

        # Run perf tests, but skip comparison to baseline
        tags = test.get('Tags', [])
        if 'perf' in tags:
            tags.remove('perf')


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


def main():
    args = parse_args()

    with open(args.orig_plist, 'rb') as f:
        plist = plistlib.load(f)

    filter_tests(plist)
    add_pgm_activation(plist)

    with open(args.pgm_plist, 'wb') as f:
        plistlib.dump(plist, f, fmt=plistlib.FMT_BINARY)


if __name__ == '__main__':
    main()
