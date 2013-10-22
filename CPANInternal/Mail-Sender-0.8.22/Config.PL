# Never modify Config.PPM, always Config.PL
# (Whenever you generate a distribution Config.PPM is overwriten by Config.PL)

# just to tell make we are done with configuring
open CFG, '>Config';print CFG "\n";close CFG;

use Cwd;
use FileHandle;
use Socket;

if ($ENV{PERL_MM_USE_DEFAULT} || !-t STDIN) { exit(); }

print "\n\n
################# Default new object settings for Mail::Sender #################
\n";

sub Ignore {
    print "\nDo you want to change the setting or use this one in spite of the errors?\n\t(C)hange / (U)se : ";
    local $_;
    while (<STDIN>) {
        return 1 if /^u$/i;
        return if /^c$/i;
        print "\t(C)hange / (U)se : ";
    }
    exit;
}


if ($0 =~ /\.PPM$/i) {

print "Running from PPM...\n\n";

chdir '..';

    # find Mail/Sender.pm
    foreach $dir (@INC) {
        if (-e $dir.'/Mail/Sender.pm') {
            $configfile = $dir.'/Mail/Sender.config';
            last;
        }
    }
    unless ($configfile) {
        print STDERR "The module Sender.pm is not found. Are you realy running the script via PPM install ???\n";
        <STDIN>;
        exit(1);
    }
    if (-e $configfile) {
        print "You have already specified some defaults for Mail::Sender,
They are saved in $configfile.
If you go on you will OVERWRITE the whole file!
You'd better edit the file by hand.\n";
        $new=' new and OVERWRITE old';
    }

} else { # called directly
    die "Do not execute this script directly, run:\n\tperl Makefile.PL
\tmake\n\tmake test\n\tmake install\n
If you are using ActivePerl under Windows use 'nmake' instead of 'make'.
nmake.exe may be found at ftp://ftp.microsoft.com/Softlib/MSLFILES/nmake15.exe\n"
     unless -d 'blib/lib/Mail';
    {
     my $dir;
     foreach $dir (@INC) {
        if (-e $dir.'/Mail/Sender.config') {
            print "You have already specified some defaults for Mail::Sender,
    They are saved in $dir/Mail/Sender.config.
    If you go on you will OVERWRITE the whole file!
    You'd better edit the file by hand.\n";
            $new=' new and OVERWRITE old';
            last;
        }
     }
    }
}

print "\nSpecify$new defaults for Mail::Sender? (y/N) ";
$res = <STDIN>;
exit unless $res =~ /^y$/i;

if (! $configfile) {
 print "\n\t... will be saved to .../lib/Mail/Sender.config\n";
 $configfile = 'blib/lib/Mail/Sender.config';
} else {
 print "\n\t... will be saved to $configfile\n";
}

while (1) {
 print "\nDefault SMTP server (hostname or IP address)\n\n\t: ";
 $smtp=<STDIN>;chomp $smtp;
 last unless $smtp;

 print "\n\t...Trying $smtp...";

 my $proto = (getprotobyname('tcp'))[2];
 my $port  = getservbyname('smtp', 'tcp');

 $smtp =~ s/^\s+//g; # remove spaces around $smtp
 $smtp =~ s/\s+$//g;

 my $smtpaddr = inet_aton($smtp);

 unless ($smtpaddr) {
  print "FAILED\n\t...cannot find server '$smtp' !!!\n";
  if (Ignore()) {last} else {next};
  next;
 }

 my $s = &FileHandle::new(FileHandle);

 if (!socket($s, AF_INET, SOCK_STREAM, $proto)) {
  die <<"*END*";
Call to socket() failed. You either do not have enough permissions
or something really strange happened. Until you fix this neither Mail::Sender
nor any other network related module will work. :-((
*END*
  if (Ignore()) {last} else {next};
 }

 if (!connect($s, sockaddr_in($port, $smtpaddr))) {
  print "FAILED\n\t...the server is either down or doesn't accept connections\n\ton port 25(SMTP).";
  if (Ignore()) {last} else {next};
 }

 my($oldfh) = select($s); $| = 1; select($oldfh);

 $_ = <$s>;
 if (/^[45]\d*\s*(.*)$/) {
  close $s;
  print "FAILED\n\t...the server replied '$1'.\n";
  if (Ignore()) {last} else {next};
 }

 print $s "helo localhost\r\n";
 $_ = <$s>;
 if (/^[45]\d*\s*(.*)$/) {
  close $s;
  print <<"*END*";
To my greeting the server replied '$1'.
It probably meens that you will have to specify the client name
when connecting to this server. You should specify the default
client name later during this configuration, otherwise you will have
to include parameter client=> in every "new Mail::Sender" call.
*END*

  if (Ignore()) {last} else {next};
 }

 print $s "quit\r\n";
 print "successful :-)\n";
 last;

}

$default{smtp} = "'$smtp'";

print <<"*END*";

*********************************************************************
**** ALL of the following Defaults are interpreted as perl code *****
*********************************************************************

*********************************************************************
Default FROM value (must be perl code / ENTER for none):

\tExamples:
\t   *   'user\@yourdomain.com'
\t   *   getlogin() . '\@yourdomain.com'
\t   *   'Your Name <yourname\@yourdomain.com>'\n
*END*
print "\t: ";
$default{from}=<STDIN>;
for ($default{from}) {
	chomp;
	$_ = "'$_'" if /^[\w\d.-]+@(?:[\w\d-]+\.)*[\w\d]+$/;
}
delete $default{from} unless $default{from};

print "\n*********************************************************************\n";
print "Default for REPLY-TO field (must be perl code / ENTER for none):\n\n\t: ";
$default{replyto}=<STDIN>;
for ($default{replyto}) {
	chomp;
	$_ = "'$_'" if /^[\w\d.-]+@(?:[\w\d-]+\.)*[\w\d]+$/;
}
delete $default{replyto} unless $default{replyto};

print "\n*********************************************************************\n";
print "Default for CC field (must be perl code / ENTER for none):\n\n\t: ";
$default{cc}=<STDIN>;
for ($default{cc}) {
	chomp;
	$_ = "'$_'" if /^[\w\d.-]+@(?:[\w\d-]+\.)*[\w\d]+$/;
}
delete $default{cc} unless $default{cc};

print "\n*********************************************************************\n";
print "Default for BCC field (must be perl code / ENTER for none):\n\n\t: ";
$default{bcc}=<STDIN>;
for ($default{bcc}) {
	chomp;
	$_ = "'$_'" if /^[\w\d.-]+@(?:[\w\d-]+\.)*[\w\d]+$/;
}
delete $default{bcc} unless $default{bcc};

print "\n*********************************************************************\n";
print "Default name of the client MACHINE used when connecting\nto the SMTP server (must be perl code / ENTER for none):\n\n\t: ";
$default{client}=<STDIN>;
chomp $default{client};delete $default{client} unless $default{client};

print "\n*********************************************************************\n";
print "Default additional headers (must be perl code / ENTER for none):\n\n\t: ";
$default{headers}=<STDIN>;
chomp $default{headers};delete $default{headers} unless $default{headers};

print "\n*********************************************************************\n";
print "Default encoding of message bodies (N)one, (Q)uoted-printable, (B)ase64:\n\n\t: ";
while (<STDIN>) {
 chomp;
 /^q$/i and $default{encoding} = "'Quoted-printable'" and last;
 /^b$/i and $default{encoding} = "'Base64'" and last;
 (/^n$/i or /^$/) and last;
}

print "\n*********************************************************************\n";
print "Default charset of message bodies (must be perl code / ENTER for none):\n\n\t: ";
$default{charset}=<STDIN>;
chomp $default{charset};
unless ($default{charset}) {
 delete $default{charset}
} else {
 $default{charset} =~ /^[\w\d_-]+$/ and $default{charset} = "'$default{charset}'";
}

print "\n*********************************************************************\n";
print "*********************************************************************\n";

#sleep 20;

open CFG, '>'.$configfile;
select CFG;
print "### Config file for Mail::Sender\n### generated by Config.PL (".scalar(localtime).")\n\n";

print "%default = (\n";

foreach (keys %default) {
    print "\t$_ => $default{$_},\n";
}

print ");\n\n";

print <<'*END*';
# The contents of this variable will be inserted into the headers of every
# mail message sent by Mail::Sender.
#
#  $Mail::Sender::SITE_HEADERS = "X-Sender: ".getlogin()."\@mysite.com";


# This function will be evaluated for every mail message before contacting the server.
# You may change all the options of the message and even cancel the message.
#
# sub SiteHook {
# }


1;

*END*

select STDOUT;
close CFG;

print "The defaults were saved into $configfile
Please review the code in there if 'make test' or
every 'use Mail::Sender;' command fails.";

1;
