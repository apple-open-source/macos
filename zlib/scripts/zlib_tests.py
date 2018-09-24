#!/usr/bin/python -u
# -*- coding: utf-8 -*-

import os
from bats.test.parambulator import Parambulator

class Test(Parambulator):
    def set_up(self):
        super(Test, self).set_up()

        # copy and decompress test files on the device
        filename = 'zlib_test_files.zip'
        tar_command = 'cd {}; tar -xzf zlib_test_files.zip'
	self.copy_to_device(filename, self.device_tmp_dir,
                            on_error="Failed to copy test files to device.")

        self.run_setup_on_device(tar_command.format(
                                 self.device_tmp_dir))

        if not self.is_watchos():
            filename = 'zlib_large_test_files.zip'
            tar_command = 'cd {}; tar -xzf zlib_large_test_files.zip'
            self.copy_to_device(filename, self.device_tmp_dir,
                                on_error="Failed to copy test files to device.")

            self.run_setup_on_device(tar_command.format(
                                     self.device_tmp_dir))

    def run_parambulator_iteration(self):
        results = {}

        # Run tests and gather results
        so, se, rc = self.run_test_on_device(
            'perl {}'.format(os.path.join(self.device_tmp_dir,
                                          'scripts/t_bats_zlib.pl')),
            as_root=True,
            # test will return non-zero if anything fails
            on_error="Test had non-zero exit status.")

        return results

Test.execute_tests()
