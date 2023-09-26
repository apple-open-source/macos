import os
import sys
from CONSTANTS import FAIL_RC, SUCCESS_RC
from Test import TestCase, TestSuite
import utilites as util


class Test_Base():
    _test_dir = "testdir"
    _encoding_mode = "ISO-8859-1"
    tests = []
    disabled_tests = []

    def __init__(self) -> None:
        self.disabled_tests = self.get_all_disabled_test_from_argv()
        self.rc = SUCCESS_RC

    def get_all_disabled_test_from_argv(self):
        return sys.argv[1:]

    def list_all_tests(self, pattern):
        cmd = "cd ../{}; ls {}".format(self._test_dir, pattern)
        try:
            test_names_str_b = util.run_cmd_shell(cmd)
            return test_names_str_b.decode(self._encoding_mode).strip().split("\n")
        except:
            raise RuntimeError("Could not find {} tests!!!".format(pattern))

    def _run_tests(self, test_func, suite_name):
        # test_function should take test name (str) as only argument
        print("The test starts running ...")
        tests = []
        self._try_to_print_disabled_tests()
        for test in self.tests:
            if test not in self.disabled_tests:
                test_case = TestCase(test, test_func, argv=[test])
                tests.append(test_case)
        test_suite = TestSuite(suite_name, tests)
        test_suite.run()
        self.rc = test_suite.test_rc
        return self.rc

    def _try_to_print_disabled_tests(self):
        if self.disabled_tests:
            print("The following tests will be skipped:")
            print(self.disabled_tests)


class REGRESS_Base(Test_Base):
    _actual_mode = "1"
    _expected_ext = "expected."
    _actual_ext = "actual."
    _assert_dir = "assets"

    def __init__(self, tests, suite_name, installer) -> None:
        super().__init__()
        self.tests = tests
        self._suite_name = suite_name
        self._installer = installer

    def _create_actual_files(self):
        cmd = "/bin/sh {} {}".format(self._installer, self._actual_mode)
        print(util.run_cmd_shell(cmd).decode(self._encoding_mode))

    def run_tests(self):
        self._create_actual_files()
        self._run_tests(test_func=self._run_test, suite_name=self._suite_name)

    def _run_test(self, name, mock_expected=None, mock_actual=None):
        expected = self._get_expected_ouput(name, mock_expected=mock_expected)
        if not expected:
            return self._fail_test(name, "Could not load expected output!!!")

        actual = self._get_actual_output(name, mock_actual=mock_actual)
        if not actual:
            return self._fail_test(name, "Could not load actual output!!!")

        if actual != expected:
            error_msg = "\nActual: {}\nExpected: {}".format(actual, expected)
            return self._fail_test(name, error_msg)
        else:
            return self._pass_test(name)

    def _fail_test(self, name, error_msg):
        error = self._combine_name_with_msg(name, error_msg)
        return FAIL_RC, error

    def _pass_test(self, name):
        return SUCCESS_RC, name

    def _combine_name_with_msg(self, name, error_msg):
        return "{}: {}".format(name, error_msg)

    def _get_actual_output(self, name, mock_actual=None):
        if mock_actual:
            return mock_actual
        actual_file = self._actual_ext + name
        actual_file_path = os.path.join(self._assert_dir, actual_file)
        actual_output = self._get_output_from_file(actual_file_path)
        return actual_output

    def _get_expected_ouput(self, name, mock_expected=None):
        if mock_expected:
            return mock_expected

        expected_file = self._expected_ext + name
        expected_file_path = os.path.join(self._assert_dir, expected_file)
        expected_output = self._get_output_from_file(expected_file_path)
        return expected_output

    def _get_output_from_file(self, file_path):
        try:
            with open(file_path, encoding=self._encoding_mode) as f:
                return f.read()
        except:
            return None

    def get_rc(self):
        return self.rc


class REGRESS_t(REGRESS_Base):
    def __init__(self) -> None:
        tests = self.list_all_tests("t.*")
        suite_name = "t_tests"
        installer = "test_installer.t"
        super().__init__(tests, suite_name, installer)


class REGRESS_p(REGRESS_Base):
    def __init__(self) -> None:
        tests = self.list_all_tests("p.*")
        suite_name = "p_tests"
        installer = "test_installer.p"
        super().__init__(tests, suite_name, installer)
