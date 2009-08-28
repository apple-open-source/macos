#! /usr/bin/python

import os
import pybsm
import unittest

class TestAuditon(unittest.TestCase):
    '''Simple tests of the auditon(2) syscall.'''
    def setUp(self):
        self.cond = pybsm.au_get_cond()
        self.policy = pybsm.au_get_policy()[0]
        self.smask = pybsm.au_get_kmask()['success']
        self.hiwater = pybsm.au_get_qctrl()['hiwater']
        self.filesz = pybsm.au_get_fsize()['filesz']
    def tearDown(self):
        pybsm.au_set_cond(self.cond)
        pybsm.au_set_policy(self.policy)
        pybsm.au_set_kmask(success=self.smask)
        pybsm.au_set_qctrl(hiwater=self.hiwater)
        pybsm.au_set_fsize(self.filesz)
    def test_set_cond(self):
        pybsm.au_set_cond('AUC_NOAUDIT')
        self.assertEqual(pybsm.au_get_cond(), 'AUC_NOAUDIT')
    def test_set_policy(self):
        pybsm.au_set_policy('AUDIT_AHLT')
        self.assertEqual(pybsm.au_get_policy()[0], 'AUDIT_AHLT')
    def test_set_qctrl(self):
        pybsm.au_set_qctrl(hiwater=110)
        self.assertEqual(pybsm.au_get_qctrl()['hiwater'], 110)
    def test_set_fsize(self):
        pybsm.au_set_fsize(long(1000000))
        self.assertEqual(pybsm.au_get_fsize()['filesz'], long(1000000))

class TestAuditpipeConfig(unittest.TestCase):
    '''Simple test of auditpipe configuration'''
    def setUp(self):
        self.ap = pybsm.io("/dev/auditpipe")
        self.qlimit = self.ap.au_pipe_get_config()['qlimit']
    def tearDown(self):
        self.ap.au_pipe_set_config(qlimit=self.qlimit)
        del self.ap
    def test_auditpipe_config(self):
        self.ap.au_pipe_set_config(qlimit=150)
        self.assertEqual(self.ap.au_pipe_get_config()['qlimit'], 150)

class TestAuditpipe(unittest.TestCase):
    '''Simple test of auditpipe and audit subsystem'''
    def setUp(self):
        os.system("/usr/sbin/audit -t > /dev/null")
        self.ap = pybsm.io("/dev/auditpipe")
    def tearDown(self):
        del self.ap
        os.system("/usr/sbin/audit -t > /dev/null")
    def test_audit(self):
        pid = os.fork()
        if pid == 0:
            os.system("/usr/sbin/auditd")
            os._exit(0)
        record = self.ap.au_read_rec()
        # assert that 3rd token in the record from the 'auditd' startup
        self.assertEqual(record[2]['text'], "auditd::Audit startup")
        

if __name__ == '__main__':
    if os.geteuid() != 0:
        print "**** Need to run test as root. ****\n"
        os._exit(1);
    unittest.main()
