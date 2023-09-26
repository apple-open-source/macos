import subprocess
import sys


def run_cmd(cmd, cwd=None, decode_mode="utf-8"):
    args = cmd.split()
    p = subprocess.Popen(args=args, stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE, cwd=cwd)
    out, err = p.communicate()
    return out.decode(decode_mode), err.decode(decode_mode), p.returncode


def run_cmd_shell(cmd):
    return subprocess.check_output(cmd, shell=True)


def run_cmd_shell_decode(cmd, encoding="utf-8", verbose=False):
    so = run_cmd_shell(cmd).decode(encoding)
    if verbose:
        print(so)
    return so


def eprint(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs)
