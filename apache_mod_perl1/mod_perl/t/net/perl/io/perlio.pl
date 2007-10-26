#!/user/local/bin/perl

use Config;

#we're in Apache::Registry
#our perl is configured use sfio so we can 
#print() to STDOUT
#and
#read() from STDIN

#we've also set (per-directory config):
#PerlSendHeader On
#PerlSetupEnv   On

my $r = shift;
my $sub = "test_$ENV{QUERY_STRING}";
if (defined &{$sub}) {
    &{$sub}($r);
}
else {
    print "Status: 200 Bottles of beer on the wall\n",
    "X-Perl-Version: $]\n";
    print "X-Perl-Script: perlio.pl\n";
    print "X-Message: hello\n";
    print "Content-type: text/plain\n\n";

    print "perlio test...\n";
    print "\$^X is $^X\n" if $^X;

    if($] >= 5.005 && $Config{usesfio} ne "true") {
	my $msg = "1234WRITEmethod";
	syswrite STDOUT, $msg, 5, 4;
	print " to STDOUT works with $] without sfio\n";
    }

    my $loc = $r->location;
    print "<Location $loc>\n";
    my(@args);

    if (@args = split(/\+/, $ENV{QUERY_STRING})) {
	print "ARGS: ",
	join(", ", map { $_ = qq{"$_"} } @args), "\n\n";
    } else {
	print "No command line arguments passed to script\n\n";
    }

    my($key,$val);
    while (($key,$val) = each %ENV) {
	print "$key=$val\n";
    }


    if ($ENV{CONTENT_LENGTH}) {
	$len = $ENV{CONTENT_LENGTH};
	read(STDIN, $content, $len);
	print "\nContent\n-------\n$content";
    }
}

sub test_1 {
    print "Content-type: text/html\n",
          "X-sub: " . "test_1\n";
    print "\r\n";
    print "1";
}

sub test_2 {
    my $msg = <<"EOF";
X-sub: test_2 
Content-type: text/html

2
EOF
    chomp $msg;
    print $msg;
}

sub test_3 {
    my $h = {
	"Content-type" => "text/plain",
	"X-sub" => "test_3",
    };
    for (keys %$h) {
	print "$_: $h->{$_}\r\n";
    }
    print "\r\n";
    print "3";
}

sub test_4 {
    my $h = {
	"Content-type" => "text/plain",
	"X-sub" => "test_4",
    };
    for (keys %$h) {
	print "$_", ": ", $h->{$_}, "\r\n";
#	print "$_", ": ", $h->{$_};
#	print "\r\n";
    }
    print "\r\n4";
}

sub test_5 {
    print <<EOF;
X-Message: parsethis
Content-type: text/html

A
B
C
D
EOF

}

sub test_syswrite_1 {
    test_syswrite(shift);
}

sub test_syswrite_2 {
    test_syswrite(shift,160);
}

sub test_syswrite_3 {
    test_syswrite(shift,80, 2000);
}

sub test_syswrite {
    my $r = shift;
    my $len = shift;
    my $offset = shift;
    my $msg = "";

#    my $m = "ENTERING test_syswrite ";
#    $m .= "LEN = $len " if $len;
#    $m .= "OFF = $offset" if $offset;
#    print STDERR $m, "\n";

    print "Status: 200 Bottles of beer on the wall\n",
    "X-Perl-Version: $]\n";
    print "X-Perl-Script: perlio.pl\n";
    print "X-Message: hello\n";
    print "Content-type: text/plain\n\n";

    for ('A'..'Z') {
	$msg .= $_ x 80;
    }
    my $bytes_sent = 
	defined($offset) ? syswrite STDOUT, $msg, $len, $offset :  
	 defined($len) ? syswrite STDOUT, $msg, $len : 
           syswrite STDOUT, $msg, length($msg);

    my $real_b = $r->bytes_sent;
    print "REAL Bytes sent = $real_b\n";
    die "Syswrite error. Bytes wrote=$bytes_sent. Real bytes sent = $real_b\n"
	unless $bytes_sent == $real_b;
}

sub test_syswrite_noheader {
    print STDERR "********* This is not a real error. Ignore. *********\n";
    my $msg = "1234WRITEmethod";
    syswrite STDOUT, $msg, 5, 4;
}





