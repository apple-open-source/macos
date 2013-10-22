
# This is a test script of IPC::LDT,
# using file handles to check the
# transfer of Perl data.


# load modules
use IPC::LDT;
use FileHandle;
use Data::Dumper;

# display number of test
print "1..4\n";

# build temporary filename
my $file="/tmp/.$$.ipc.ldt.tmp";

# init the data to transfer
my $scalar=50;
my @array=(3, 7, 15);
my %hash=(a=>'A', z=>'Z');
my $ref=\$IPC::LDT::VERSION;

# write message
{
 # open file
 open(O, ">$file") or die "[Fatal] Could not open $file for writing.\n";

 # build LDT object
 my $ldt=new IPC::LDT(handle=>*O, objectMode=>1) or die "[Fatal] Could not build LDT object.\n";

 # send data
 $ldt->send($scalar, \@array, \%hash, $ref);

 # close the temporary file
 close(O);
}


# read message
{
 # open file
 open(I, $file) or die "[Fatal] Could not open $file for reading.\n";

 # build LDT object
 my $ldt=new IPC::LDT(handle=>*I, objectMode=>1) or die "[Fatal] Could not build LDT object.\n";

 # read data
 my @data=$ldt->receive;

 # perform the checks
 print $data[0]==$scalar ? 'ok' : 'not ok', "\n";
 print Dumper(@{$data[1]}) eq Dumper(@array) ? 'ok' : 'not ok', "\n";
 print Dumper(%{$data[2]}) eq Dumper(%hash)  ? 'ok' : 'not ok', "\n";
 print ${$data[3]} eq $$ref ? 'ok' : 'not ok', "\n";

 # close the temporary file
 close(I);
}

# clean up
unlink $file;
