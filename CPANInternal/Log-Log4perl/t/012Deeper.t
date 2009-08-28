# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'


use Log::Log4perl;
use Test::More;
use File::Spec;

our $LOG_DISPATCH_PRESENT = 0;

BEGIN { 
    eval { require Log::Dispatch; };
    if($@) {
       plan skip_all => "only with Log::Dispatch";
    } else {
       $LOG_DISPATCH_PRESENT = 1;
       plan tests => 3;
    }
};

my $WORK_DIR = "tmp";
if(-d "t") {
    $WORK_DIR = File::Spec->catfile(qw(t tmp));
}
unless (-e "$WORK_DIR"){
    mkdir("$WORK_DIR", 0755) || die "can't create $WORK_DIR ($!)";
}

my ($sec,$min,$hour,$mday,$mon,$year,$wday,$yday,$isdst) = localtime(time);
my $today = sprintf("%4.4d%2.2d%2.2d",$year+1900, $mon+1, $mday);
use vars qw($logfile1 $logfile6 $logfile7);
$logfile1 = File::Spec->catfile(qw(t tmp deeper1.log));
$logfile6 = File::Spec->catfile(qw(t tmp deeper6.log));
$logfile7 = File::Spec->catfile(qw(t tmp deeper7.log)); 
our @outfiles = ($logfile1, $logfile6, $logfile7);

foreach my $f (@outfiles){
    unlink $f if (-e $f);
}


my $config = <<EOL;
#specify LOGLEVEL, appender1, appender2, ...
log4j.category.plant     = INFO,  FileAppndr1
log4j.category.animal        = INFO,  FileAppndr1
log4j.category.animal.dog = DEBUG, FileAppndr1

log4j.oneMessagePerAppender = 1


# ---------------------------------------------
# FileAppndr1
log4j.appender.FileAppndr1        = org.apache.log4j.FileAppender
log4j.appender.FileAppndr1.File   = $logfile1

log4j.appender.FileAppndr1.layout = org.apache.log4j.PatternLayout
log4j.appender.FileAppndr1.layout.ConversionPattern=%d %4r [%t] %-5p %c %t - %m%n


# ---------------------------------------------------
#2nd set of tests,inheritance
log4j.category.a       = INFO, l2
log4j.category.a.b.c.d = WARN, l2

log4j.appender.l2        = org.apache.log4j.FileAppender
log4j.appender.l2.File   = $logfile6
log4j.appender.l2.layout = org.apache.log4j.PatternLayout
log4j.appender.l2.layout.ConversionPattern=%d %4r [%t] %-5p %c - %m%n


# --------------------------------------
#inheritance the other way
log4j.category.xa       = WARN, l3
log4j.category.xa.b.c.d = INFO, l3

log4j.appender.l3       = org.apache.log4j.FileAppender
log4j.appender.l3.File  = $logfile7
log4j.appender.l3.layout= org.apache.log4j.PatternLayout
log4j.appender.l3.layout.ConversionPattern=%d %4r 666  [%t] %-5p  %c - %m%n

EOL


Log::Log4perl->init(\$config);


# -----------------------------------------------------
# (1) shotgun test
#set to INFO

my $logger = Log::Log4perl->get_logger('plant');

#set to INFO
$logger->debug("debugging message 1 ");
$logger->info("info message 1 ");      
$logger->warn("warning message 1 ");   
$logger->fatal("fatal message 1 ");   

#set to DEBUG
my $doglogger = Log::Log4perl->get_logger('animal.dog');
$doglogger->debug("debugging message 2 ");
$doglogger->info("info message 2 ");
$doglogger->warn("warning message 2 ");
$doglogger->fatal("fatal message 2 ");

#set to INFO
my $animallogger = Log::Log4perl->get_logger('animal');
$animallogger->debug("debugging message 3 ");
$animallogger->info("info message 3 ");
$animallogger->warn("warning message 3 ");
$animallogger->fatal("fatal message 3 ");

#should default to animal::dog
my $deeptreelogger = Log::Log4perl->get_logger('animal.dog.leg.toenail');
$deeptreelogger->debug("debug message");
$animallogger->info("info message");
$deeptreelogger->warn("warning message");
$animallogger->fatal("fatal message");

my ($result, $expected);

{local $/ = undef;
 open (F, File::Spec->catfile(qw(t deeper1.expected))) || die $!;
 $expected = <F>;
 open (F, $logfile1) || die $!;
 $result = <F>;
 close F;
 $result =~ s/.+?] //g;
}

is ($result, $expected);


# ------------------------------------
# (6)   test inheritance
#a=INFO, a.b.c.d=WARN, a.b and a.b.c are undefined
my $la = Log::Log4perl->get_logger('a');
my $lab = Log::Log4perl->get_logger('a.b');
my $labc = Log::Log4perl->get_logger('a.b.c');
my $labcd = Log::Log4perl->get_logger('a.b.c.d');
my $labcde = Log::Log4perl->get_logger('a.b.c.d.e');

foreach my $l ($la, $lab, $labc, $labcd, $labcde){
   $l->debug("should not print");
}
foreach my $l ($la, $lab, $labc, $labcd, $labcde){
   $l->info("should print for a, a.b, a.b.c");
}
foreach my $l ($la, $lab, $labc, $labcd, $labcde){
   $l->warn("should print for a, a.b, a.b.c, a.b.c.d, a.b.c.d.e");
}
foreach my $l ($la, $lab, $labc, $labcd, $labcde){
   $l->fatal("should print for a, a.b, a.b.c, a.b.c.d, a.b.c.d.e");
}
{local $/ = undef;
 open (F, File::Spec->catfile(qw(t deeper6.expected)));
 $expected = <F>;
 open (F, $logfile6);
 $result = <F>;
 close F;
 $result =~ s/.+?] //g;
}

is($result, $expected);


# ------------------------------------
# (7)   test inheritance the other way
#xa=WARN, xa.b.c.d=INFO, xa.b and xa.b.c are undefined
my $xla = Log::Log4perl->get_logger('xa');
my $xlab = Log::Log4perl->get_logger('xa.b');
my $xlabc = Log::Log4perl->get_logger('xa.b.c');
my $xlabcd = Log::Log4perl->get_logger('xa.b.c.d');
my $xlabcde = Log::Log4perl->get_logger('xa.b.c.d.e');

foreach my $l ($xla, $xlab, $xlabc, $xlabcd, $xlabcde){
   $l->debug("should not print");
}
foreach my $l ($xla, $xlab, $xlabc, $xlabcd, $xlabcde){
   $l->info("should print for xa.b.c.d, xa.b.c.d.e");
}
foreach my $l ($xla, $xlab, $xlabc, $xlabcd, $xlabcde){
   $l->warn("should print for xa, xa.b, xa.b.c, xa.b.c.d, xa.b.c.d.e");
}
foreach my $l ($xla, $xlab, $xlabc, $xlabcd, $xlabcde){
   $l->fatal("should print for xa, xa.b, xa.b.c, xa.b.c.d, xa.b.c.d.e");
}
{local $/ = undef;
 open (F, File::Spec->catfile(qw(t deeper7.expected)));
 $expected = <F>;
 open (F, $logfile7);
 $result = <F>;
 close F;
 $result =~ s/.+?] //g;
}

is($result, $expected);


   
END{   
    foreach my $f (@outfiles){
        unlink $f if (-e $f);
    }
}
