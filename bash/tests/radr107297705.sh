set -e
echo "shell is $$" >&2
# Start a background job which will continue our child in two seconds.
(sleep 2; kill -CONT $(cat radr107297705.pid)) &
echo "killer is $!" >&2
rm -f radr107297705.flag
# Start a job which stops itself, then succeeds when continued.  If
# the shell is well-behaved, it will wait until the job has terminated
# before evaluating the ||, and will not run the touch command.  If
# not, it will perceive the stopped job as uncleanly terminated and
# run the touch command.
set -m
sh -xc 'echo $$ >radr107297705.pid; kill -TSTP $$; exit 0' ||
	touch radr107297705.flag
set +m
# Wait for the killer job.
wait
echo "child was $(cat radr107297705.pid)" >&2
# Check for the presence of the flag.
if [ -f radr107297705.flag ] ; then
	echo "FAIL"
	exit 1
fi
echo "PASS"
