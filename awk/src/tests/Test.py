from CONSTANTS import SUCCESS_RC, FAIL_RC


class TestCase:
    _start_token = "[BEGIN]"
    _end_token_pass = "[PASS]"
    _end_token_fail = "[FAIL]"

    def __init__(self, name, func, argv=[]) -> None:
        self._name = name
        self._func = func
        self._argv = argv

    def run(self):
        self._start()
        rc, msg = self._func(*self._argv)
        if self._test_passed(rc):
            self._pass(msg)
        else:
            self._fail(msg)
        return rc

    def _test_passed(self, rc):
        return rc == SUCCESS_RC

    def _start(self):
        token = self._insert_name_to_token(self._start_token)
        print(token)

    def _pass(self, msg):
        token = self._insert_name_to_token(self._end_token_pass)
        print(token)
        print(msg)
        return token

    def _fail(self, msg):
        token = "{} {}".format(self._end_token_fail, msg)
        print(token)
        return token

    def _insert_name_to_token(self, token):
        return token + " " + self._name


class TestSuite:
    _start_token = "[TEST]"
    _end_token = "[SUMMARY]"

    def __init__(self, name, tests) -> None:
        '''
        name: str
        tests: [TestCase]
        '''
        self._name = name
        self._tests = tests
        self.test_rc = SUCCESS_RC

    def run(self):
        self._start()
        for test in self._tests:
            rc = test.run()
            if rc != SUCCESS_RC:
                self._fail()
        self._end()
        return self.test_rc

    def _start(self):
        token = "{} {}".format(self._start_token, self._name)
        print(token)

    def _end(self):
        print(self._end_token)

    def _fail(self):
        self.test_rc = FAIL_RC
