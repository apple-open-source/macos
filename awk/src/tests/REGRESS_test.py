from glob import glob1
import unittest
from REGRESS import REGRESS_Base, REGRESS_t, REGRESS_p
from REGRESS_bugs_fixed import REGRESS_bug
from CONSTANTS import FAIL_RC, SUCCESS_RC


class REGRESS_base_tests(unittest.TestCase):
    base_tests = REGRESS_Base([], "suite_name", "installer")

    def test_test_failed_non_expected_file(self):
        test_name = "foo"
        out = self.base_tests._get_expected_ouput(test_name)
        self.assertEqual(out, None)
        rc, _ = self.base_tests._run_test(test_name)
        self.assertEqual(rc, FAIL_RC)

    def test_run_test_pass(self):
        test = "bar_test"
        rc, _ = self.base_tests._run_test(
            test, mock_actual="foo", mock_expected="foo")
        self.assertEqual(rc, SUCCESS_RC)

    def test_run_test_fail(self):
        test = "foo_test"
        rc, _ = self.base_tests._run_test(
            test, mock_expected="foo", mock_actual="notfoo")
        self.assertEqual(rc, FAIL_RC)


class REGRESS_t_tests(unittest.TestCase):
    t_tests = REGRESS_t()

    def test_init(self):
        total_tests = len(glob1("../testdir", "t.*"))
        self.assertEqual(len(self.t_tests.tests), total_tests)

    def test_get_expected_output(self):
        for test in self.t_tests.tests:
            output = self.t_tests._get_expected_ouput(test)
            self.assertNotEqual(output, None)


class REGRESS_p_tests(unittest.TestCase):
    p_tests = REGRESS_p()

    def test_init(self):
        total_tests = len(glob1("../testdir", "p.*"))
        self.assertEqual(len(self.p_tests.tests), total_tests)

    def test_get_expected_output(self):
        for test in self.p_tests.tests:
            output = self.p_tests._get_expected_ouput(test)
            self.assertNotEqual(output, None)


class REGRESS_bugs_tests(unittest.TestCase):
    b_tests = REGRESS_bug()
    total_b_tests = 22

    def test_init(self):
        total_tests = len(glob1("../bugs-fixed", "*.awk"))
        self.assertEqual(len(self.b_tests.tests), total_tests)


if __name__ == "__main__":
    unittest.main()
