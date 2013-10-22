
# This is a test script of IPC::LDT,
# using file handles to check the
# delay feature.


# load modules
use IPC::LDT;
use FileHandle;

# display number of test
print "1..1\n";

# build temporary filename
my $file="/tmp/.$$.ipc.ldt.tmp";

# write messages
{
 # open file
 open(O, ">$file") or die "[Fatal] Could not open $file for writing.\n";

 # build LDT object
 my $ldt=new IPC::LDT(handle=>*O) or die "[Fatal] Could not build LDT object.\n";

 # install delay filter
 $ldt->delay(sub {$_[0]->[0]%2});

 # send the messages
 $ldt->send($_) for 1..10;

 # stop delay and send delayed messages
 $ldt->undelay;

 # close the temporary file
 close(O);
}


# read messages
{
 # open file
 open(I, $file) or die "[Fatal] Could not open $file for reading.\n";

 # build LDT object
 my $ldt=new IPC::LDT(handle=>*I) or die "[Fatal] Could not build LDT object.\n";

 # read the messages
 my @read;
 $read[$_-1]=$ldt->receive for 1..10;

 # perform the checks
 print join('-', @read) eq '2-4-6-8-10-1-3-5-7-9' ? 'ok' : 'not ok', "\n";

 # close the temporary file
 close(I);
}

# clean up
unlink $file;
