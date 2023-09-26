import os
from REGRESS import Test_Base
from CONSTANTS import FAIL_RC
import utilites as utils


class REGRESS_bug(Test_Base):
    runner = "bugs_test_runner.sh"
    fail_token = "failed"
    _test_file_ext = ".awk"
    _out_file_ext = ".OUT"

    def __init__(self) -> None:
        super().__init__()
        self._test_dir = "bugs-fixed"
        self.tests = self.list_all_tests("*{}".format(self._test_file_ext))

    def run_tests(self):
        self._run_tests(test_func=self._run_test, suite_name="bug-fixed-tests")

    def _run_test(self, test):
        cmd = "/bin/sh {} {}".format(self.runner, test)
        out, err, r = utils.run_cmd(cmd)

        if err:
            return FAIL_RC, err

        if self.fail_token in out:
            error = self._get_error_from_output_file(test)
            error_msg = "{}\n{}".format(out, error)
            return FAIL_RC, error_msg

        return r, out

    def _get_error_from_output_file(self, test):
        try:
            out_file = test.replace(self._test_file_ext, self._out_file_ext)
            test_dir = "../{}".format(self._test_dir)
            out_file_path = os.path.join(test_dir, out_file)
            with open(out_file_path, encoding=self._encoding_mode) as f:
                return f.read()
        except:
            return "Additional Error: Couldn't log error from the {} file!!!".format(out_file_path)


if __name__ == "__main__":
    b_tests = REGRESS_bug()
    b_tests.run_tests()
    exit(b_tests.rc)
