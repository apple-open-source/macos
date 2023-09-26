import os
from REGRESS import Test_Base
import utilites as utils
from CONSTANTS import *

# ATTENTION:
# The tests are designed to run in BATS.
# For local run, use REGRESS


class REGRESS_T(Test_Base):
    fail_tokens = ["fail", "BAD"]
    changed_name = {"arnold-data": "arnold-fixes.tar",
                    "beebe-data": "beebe.tar"}

    def __init__(self) -> None:
        super().__init__()
        self.tests = self.list_all_tests("T.*")

    def run_tests(self):
        self.set_up()
        self._run_tests(self._run_test, "T.tests")

    def _run_test(self, test):
        cmd = "/bin/sh {}".format(test)
        out, _, _ = utils.run_cmd(
            cmd, cwd="../testdir", decode_mode="iso-8859-1")
        for f_token in self.fail_tokens:
            if f_token in out:
                return FAIL_RC, out
        return SUCCESS_RC, out

    def set_up(self):
        self._verify_awk()
        self._verify_echo()
        self._rename_data_name()

    def _rename_data_name(self):
        # The tar file names changed during the installation, so
        # we need to change them back.
        test_dir_path = "../testdir"
        for changed, original in self.changed_name.items():
            changed_file = os.path.join(test_dir_path, changed)
            original_file = os.path.join(test_dir_path, original)
            self._try_renaming_data_name(changed_file, original_file)

    def _try_renaming_data_name(self, changed_file, original_file):
        try:
            if not os.path.isfile(original_file):
                os.rename(changed_file, original_file)
        except:
            raise RuntimeError(
                "Could not unchange the name of the testing data!!!")

    def _verify_echo(self):
        echo_copy = "../testdir/echo"
        if not os.path.isfile(echo_copy):
            raise RuntimeError("Failed to copy echo!!!")

    def _verify_awk(self):
        awk_copy = "../a.out"
        if not os.path.isfile(awk_copy):
            raise RuntimeError("Failed to copy awk!!!")


if __name__ == "__main__":
    T_tests = REGRESS_T()
    T_tests.run_tests()
    exit(T_tests.rc)
