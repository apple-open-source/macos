import unittest

from CONSTANTS import FAIL_RC, SUCCESS_RC
from Test import TestCase, TestSuite


def pass_func():
    return SUCCESS_RC, "Passed"


def fail_func():
    return FAIL_RC, "Failed"


class TestCaseTests(unittest.TestCase):
    name = "name"

    def test_pass_the_test(self):
        test = TestCase(self.name, pass_func)
        self.assertEqual(test._pass(), "{} {}".format(
            test._end_token_pass, self.name))
        self.assertEqual(test.run(), SUCCESS_RC)

    def test_fail_the_test(self):
        test = TestCase(self.name, fail_func)
        self.assertEqual(test._fail("msg"), "{} {}".format(
            test._end_token_fail, "msg"))
        self.assertEqual(test.run(), FAIL_RC)


class TestSuiteTests(unittest.TestCase):
    name = "name"
    fail_test = TestCase(name, fail_func)
    pass_test = TestCase(name, pass_func)

    def test_fail_suite(self):
        tests = [self.fail_test, self.pass_test]
        suite = TestSuite(self.name, tests)
        self.assertEqual(suite.run(), FAIL_RC)

    def test_pass_suite(self):
        tests = [self.pass_test, self.pass_test]
        suite = TestSuite(self.name, tests)
        self.assertEqual(suite.run(), SUCCESS_RC)


if __name__ == '__main__':
    unittest.main()
