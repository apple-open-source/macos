#!/usr/bin/ruby


OLDDATE   = '"01/01/10 13:00:00"'
USEDATE 	= '"01/01/20 12:00:00"'

def StartTest(test_description)
  print "\n" + "[TEST] " + test_description
end

def PassTest(why_pass)
  print "[PASS] " + why_pass
end

def FailTest(why_fail)
  print "[FAIL] " + why_fail
end

def pmset_schedule_old_expect_failure

  cmd 		= 'pmset schedule wake ' + USEDATE
	StartTest("Use pmset command-line to schedule a wake event in the past - it should fail.")

	print cmd + "\n"
	cmd_stdout = `#{cmd}`
	cmd_exitstatus = $CHILD_STATUS.to_i

	if 0 == cmd_exitstatus then
		FailTest("Exited cleanly")
	else
		PassTest("Exited with error " + cmd_exitstatus)
	end

end

def pmset_schedule_and_cancel

  cmd 		= 'pmset schedule wake ' + USEDATE
  cancelCmd 	= 'pmset schedule cancel wake ' + USEDATE

	StartTest("Use pmset command-line to schedule and cancel a wakeup in the future.")

	print cmd + "\n"
	cmd_stdout = `#{cmd}`
	cmd_exitstatus = $CHILD_STATUS.to_i

	if 0 == cmd_exitstatus then
		PassTest("Exited cleanly")
	else
		FailTest("Exited with error " + cmd_exitstatus)
	end

	print cancelCmd + "\n"
	cancel_stdout = `#{cancelCmd}`
	cancel_exitstatus = $CHILD_STATUS.to_i
	if 0 == cancel_exitstatus then
		PassTest("Exited cleanly")
	else
		FailTest("Exited with error " + cancel_exitstatus)
	end

end

pmset_schedule_old_expect_failure
pmset_schedule_and_cancel