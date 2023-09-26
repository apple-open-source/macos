#!/usr/bin/env python3

# test_su_es_events.py is a script that is used to validate the new ess_notify_su
# functionality in su. The script works by create each of the situations that
# would trigger a ess_notify_su event. This script also has the side effect of creating
# an account called ess_notify_su_test_account. It attempts to clean it up at the end of
# the file but if the script unexpectely quits, this account will remain on the system.

# reminder: that you need to run the following on a newly built su for the process
# to have the right permissions
# sudo chown root:0 ./su
# sudo chmod u+s ./su

import random
import json
import string
import time
import os
import sys
import subprocess
from functools import partial
from contextlib import ExitStack


def rand_str(len):
    return "".join(random.choice(string.ascii_uppercase + string.digits) for _ in range(len))

TEST_USER = "ess_notify_su_test_account"
TEST_USER_PASSWORD = rand_str(30)
TEST_FAILURE = False

ESLOGGER_TIMEOUT_SECONDS = 30

def user_delete():
    return subprocess.run(
        f"sysadminctl -deleteUser {TEST_USER}",
        shell=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL
    ).returncode == 0


def user_create():
    user_delete()
    return subprocess.run(
        f"sysadminctl -addUser {TEST_USER} -password \"{TEST_USER_PASSWORD}\"",
        shell=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL
    ).returncode == 0


def wait_for_es_logger():
    for n in range(ESLOGGER_TIMEOUT_SECONDS * 10):
        if 0 == subprocess.run(
            'esctl diagnostics | grep -i $(codesign -dvvv /usr/bin/eslogger 2>&1 | grep CDHash= | cut -d "=" -f2)',
            shell=True,
            stderr=subprocess.DEVNULL,
            stdout=subprocess.DEVNULL
        ).returncode:
            return True
        time.sleep(0.1)
    return False


def poll_until_valid_json(json_file, path):
    for n in range(ESLOGGER_TIMEOUT_SECONDS * 10):
        json_file.flush()
        try:
            with open(path, "r") as file:
                text = file.read()
                return True, json.loads(text)
        except:
            time.sleep(0.1)
    return False, None


def ignoreException(fn):
    try:
        fn()
    except:
        pass


def run_test(test_name, su_fn, validate_fn, eslogger_wait=True):
    global TEST_FAILURE
    with ExitStack() as stack:
        print(f"[BEGIN] {test_name}")
        json_path = f"/tmp/{test_name}.json"
        with open(json_path, "w") as json_file:
            if eslogger_wait:
                # kill all other eslogger instaces so waiting for its CD Hash works.
                os.system("pkill eslogger")
            # start_new_session=True because of rdar://103058802 (Running eslogger from a bash/python will not generate events)
            proc = subprocess.Popen(
                ['/usr/bin/eslogger', 'su'], stdout=json_file, start_new_session=True)
            stack.callback(lambda: ignoreException(proc.terminate))
            if eslogger_wait and not wait_for_es_logger():
                print("Failed to create es_logger client")
                print(
                    f"[FAIL] {test_name}")
                TEST_FAILURE = True
                return False
            else:
                time.sleep(1)

            su_fn()
            succ, dict = poll_until_valid_json(json_file, json_path)
            if (succ and validate_fn(dict)):
                print(f"[PASS] {test_name}")
                return True
            else:
                print(f"[FAIL] {test_name}")
                TEST_FAILURE = True
                return False


def su_es_test_name_too_long(su_path):
    bigStr = "a"*200000
    subprocess.run([su_path, "-l", bigStr], start_new_session=True)


def su_es_test_name_too_long_validate(dict):
    return dict.get('event', {}).get('su', {}).get('failure_message') == "username too long"


def su_es_test_unknown_user(su_path):
    subprocess.run([su_path, "-l", rand_str(10)], start_new_session=True)


def su_es_test_unknown_user_validate(dict):
    return dict.get('event', {}).get('su', {}).get('failure_message') == "unknown subject"


def su_es_test_unable_to_determine_invoking_subject(su_path):
    os.setuid(1223)
    os.seteuid(1223)
    subprocess.run(su_path, start_new_session=True)


def su_es_test_unable_to_determine_invoking_subject_validate(dict):
    return dict.get('event', {}).get('su', {}).get('failure_message') == "unable to determine invoking subject"


def su_es_test_permission_denined(su_path):
    assert (os.path.exists(su_path))
    subprocess.run(
        f"sudo -u {TEST_USER} expect -c 'spawn {su_path}; expect \"Password:\"; send \"badpassword\r\"; interact'",
        shell=True
    )


def su_es_test_permission_denined_validate(dict):
    return dict.get('event', {}).get('su', {}).get('failure_message') == "Permission denied: bad su to target user"


def su_es_test_pam_acct_mgmt_deny(su_path):
    os.system(f"pwpolicy -u {TEST_USER} disableuser 2> /dev/null")
    subprocess.run([su_path, "-l", TEST_USER], start_new_session=True)
    os.system(f"pwpolicy -u {TEST_USER} enableuser 2> /dev/null")


def su_es_test_pam_acct_mgmt_deny_validate(dict):
    return dict.get('event', {}).get('su', {}).get('failure_message') == "pam_acct_mgmt: permission denied"


def su_es_test_success(su_path):
    subprocess.run([su_path, "-l", TEST_USER, "-c", "whoami"],
                   start_new_session=True)


def su_es_test_success_validate(dict):
    return dict.get('event', {}).get('su', {}).get('success') == True


if __name__ == '__main__':
    print('''==================================================
[TEST] su Endpoint Security Tests
==================================================''')
    if os.geteuid() != 0:
        print("Error: Please run as root")
        exit(1)
    if len(sys.argv) > 2:
        print("Error: too many arguments")
        exit(1)
    if len(sys.argv) != 2:
        print("Error: please provide path to su")
        exit(1)
    su_path = sys.argv[1]

    if (not os.path.exists(su_path)):
        print("The path provided does not exist")
        exit(1)

    with ExitStack() as stack:

        user_create()
        stack.callback(user_delete)

        run_test(
            "su_es_test_name_too_long",
            partial(su_es_test_name_too_long, su_path),
            su_es_test_name_too_long_validate
        )

        run_test(
            "su_es_test_unknown_user",
            partial(su_es_test_unknown_user, su_path),
            su_es_test_unknown_user_validate
        )

        run_test(
            "su_es_test_permission_denined",
            partial(su_es_test_permission_denined, su_path),
            su_es_test_permission_denined_validate
        )

        run_test(
            "su_es_test_pam_acct_mgmt_deny",
            partial(su_es_test_pam_acct_mgmt_deny, su_path),
            su_es_test_pam_acct_mgmt_deny_validate
        )

        run_test(
            "su_es_test_success",
            partial(su_es_test_success, su_path),
            su_es_test_success_validate
        )

        run_test(  # This does something hacky, run last
            "su_es_test_unable_to_determine_invoking_subject",
            partial(su_es_test_unable_to_determine_invoking_subject, su_path),
            su_es_test_unable_to_determine_invoking_subject_validate
        )
        print("--------------------------------------------------")
        if TEST_FAILURE:
            print("[SUMMARY] A test failure has occurred ❌")
            sys.exit(1)
        else:
            print("[SUMMARY] 🎉 All Tests Pass 🎉")
            sys.exit(0)
