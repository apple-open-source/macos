
# This is a test script of IPC::LDT,
# using file handles to check the
# switching between ASCII and object mode.
# (This is just a combination of ascii.t and data.t.)


# load modules
use IPC::LDT;
use FileHandle;
use Data::Dumper;

# display number of test
print "1..12\n";

# build temporary filename
my $file="/tmp/.$$.ipc.ldt.tmp";

# init the data to transfer
my $msg="This is a simple\nmultiline check message.";
my @msg=('This message', "contains\nof", 'several parts.');
my $scalar=50;
my @array=(3, 7, 15);
my %hash=(a=>'A', z=>'Z');
my $ref=\$IPC::LDT::VERSION;

# write message
{
 # open file
 open(O, ">$file") or die "[Fatal] Could not open $file for writing.\n";

 # build LDT object
 my $ldt=new IPC::LDT(handle=>*O) or die "[Fatal] Could not build LDT object.\n";

 # send ASCII messages
 $ldt->send($msg);
 $ldt->send(@msg);

 # switch to object mode
 $ldt->setObjectMode;

 # send data
 $ldt->send($scalar, \@array, \%hash, $ref);

 # switch to ASCII mode
 $ldt->setAsciiMode;

 # send ASCII messages again
 $ldt->send($msg);
 $ldt->send(@msg);

 # switch to object mode
 $ldt->setObjectMode;

 # send data again
 $ldt->send($scalar, \@array, \%hash, $ref);

 # close the temporary file
 close(O);
}


# read message
{
 # open file
 open(I, $file) or die "[Fatal] Could not open $file for reading.\n";

 # build LDT object
 my $ldt=new IPC::LDT(handle=>*I) or die "[Fatal] Could not build LDT object.\n";

 # read the messages
 my $read1=$ldt->receive;
 my $read2=$ldt->receive;

 # switch to object mode
 $ldt->setObjectMode;

 # read data
 my @data1=$ldt->receive;

 # switch to ASCII mode
 $ldt->setAsciiMode;

 # read the messages again
 my $read3=$ldt->receive;
 my $read4=$ldt->receive;

 # switch to object mode
 $ldt->setObjectMode;

 # read data again
 my @data2=$ldt->receive;

 # perform the ASCII checks
 print $read1 eq $msg ? 'ok' : 'not ok', "\n";
 print $read2 eq join('', @msg) ? 'ok' : 'not ok', "\n";

 print $read3 eq $msg ? 'ok' : 'not ok', "\n";
 print $read4 eq join('', @msg) ? 'ok' : 'not ok', "\n";

 # perform the data checks
 print $data1[0]==$scalar ? 'ok' : 'not ok', "\n";
 print Dumper(@{$data1[1]}) eq Dumper(@array) ? 'ok' : 'not ok', "\n";
 print Dumper(%{$data1[2]}) eq Dumper(%hash)  ? 'ok' : 'not ok', "\n";
 print ${$data1[3]} eq $$ref ? 'ok' : 'not ok', "\n";

 print $data2[0]==$scalar ? 'ok' : 'not ok', "\n";
 print Dumper(@{$data2[1]}) eq Dumper(@array) ? 'ok' : 'not ok', "\n";
 print Dumper(%{$data2[2]}) eq Dumper(%hash)  ? 'ok' : 'not ok', "\n";
 print ${$data2[3]} eq $$ref ? 'ok' : 'not ok', "\n";

 # close the temporary file
 close(I);
}

# clean up
unlink $file;
