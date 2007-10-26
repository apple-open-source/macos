# Check if warnings are issued for weirdo configurations

use warnings;
use strict;

use Test;
use Log::Log4perl;
use File::Spec;

my $WORK_DIR = "tmp";
if(-d "t") {
    $WORK_DIR = File::Spec->catfile(qw(t tmp));
}
unless (-e "$WORK_DIR"){
    mkdir("$WORK_DIR", 0755) || die "can't create $WORK_DIR ($!)";
}

my $TMP_FILE = File::Spec->catfile(qw(t tmp warnings));
$TMP_FILE = "tmp/warnings" if ! -d "t";

BEGIN { plan tests => 2 }
END   { close IN;
        unlink $TMP_FILE;
      }

ok(1); # Initialized ok

# Capture STDERR to a temporary file and a filehandle to read from it
open STDERR, ">$TMP_FILE";
open IN, "<$TMP_FILE" or die "Cannot open $TMP_FILE";
sub readwarn { return scalar <IN>; }

############################################################
# Get a logger and use it without having called init() first
############################################################
my $log = Log::Log4perl::get_logger("abc.def");
$log->debug("hey there");

my $warn = readwarn();
#print "'$warn'\n";

ok($warn, 'm#Forgot#');

__END__

############################################################
# Check for single \'s on line ends -- they need to be
# \\ for perl to recognize it. But how? Perl swallows it.
############################################################
my $conf = <<EOL;
log4j.rootLogger=DEBUG, A1
log4j.appender.A1=Log::Log4perl::Appender::TestBuffer
log4j.appender.A1.layout=org.apache.log4j.PatternLayout
log4j.appender.A1.layout.ConversionPattern=%-4r [%t] %-5p %c %t - %m%n
log4j.category.simplelayout.test=INFO, \
   myAppender
log4j.appender.myAppender        = Log::Log4perl::Appender::FileAppenderx
log4j.appender.myAppender.layout = Log::Log4perl::Layout::SimpleLayout
log4j.appender.myAppender.File   = abc
EOL

Log::Log4perl->init(\$conf);

my $err = readwarn();

ok($err, 'm#single \\#i');

print "$conf\n";
