
# This is a test script of IPC::LDT,
# using file handles to check the
# transfer of ASCII strings.


# load modules
use IPC::LDT;
use FileHandle;

# display number of test
print "1..2\n";

# build temporary filename
my $file="/tmp/.$$.ipc.ldt.tmp";

# store the messages to transfer
my $msg="This is a simple\nmultiline check message.";
my @msg=('This message', "contains\nof", 'several parts.');


# write messages
{
 # open file
 open(O, ">$file") or die "[Fatal] Could not open $file for writing.\n";

 # build LDT object
 my $ldt=new IPC::LDT(handle=>*O) or die "[Fatal] Could not build LDT object.\n";

 # send the messages
 $ldt->send($msg);
 $ldt->send(@msg);

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
 my $read1=$ldt->receive;
 my $read2=$ldt->receive;

 # perform the checks
 print $read1 eq $msg ? 'ok' : 'not ok', "\n";
 print $read2 eq join('', @msg) ? 'ok' : 'not ok', "\n";

 # close the temporary file
 close(I);
}

# clean up
unlink $file;
