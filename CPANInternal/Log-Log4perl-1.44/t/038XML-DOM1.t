
BEGIN { 
    if($ENV{INTERNAL_DEBUG}) {
        require Log::Log4perl::InternalDebug;
        Log::Log4perl::InternalDebug->enable();
    }
}

use Test::More;
use Log::Log4perl;
use strict;
use warnings;
use Data::Dumper;
use File::Spec;
$SIG{__WARN__} = sub { die @_; };

our $no_XMLDOM;

BEGIN {
    my $dvrq = $Log::Log4perl::DOM_VERSION_REQUIRED;

    eval {
        require XML::DOM;
        XML::DOM->VERSION($dvrq);
        my $dver = XML::DOM->VERSION($dvrq);
        require XML::Parser;
        my $pver = XML::Parser->VERSION;
        if ($pver >= 2.32 && $dver <= 1.42){
            print STDERR "Your version of XML::DOM ($dver) is incompatible with your version of XML::Parser ($pver).  You should upgrade your XML::DOM to 1.43 or greater.\n";
            die 'skip tests';
        }

    };
    if ($@) {
        plan skip_all => "only with XML::DOM > $dvrq";
    }else{
        plan tests => 2;
    }
}

if ($no_XMLDOM){
    ok(1);
    exit(0);
}


my $xmlconfig = <<EOL;
<?xml version="1.0" encoding="UTF-8"?> 
<!DOCTYPE log4j:configuration SYSTEM "log4j.dtd">

<log4j:configuration xmlns:log4j="http://jakarta.apache.org/log4j/"
    threshold="debug">
  
  <appender name="A1" class="Log::Log4perl::Appender::TestBuffer">
        <layout class="Log::Log4perl::Layout::SimpleLayout"/>
  </appender>   
  <appender name="A2" class="Log::Log4perl::Appender::TestBuffer">
        <layout class="Log::Log4perl::Layout::SimpleLayout"/>   
  </appender>   
  <appender name="BUF0" class="Log::Log4perl::Appender::TestBuffer">
        <layout class="Log::Log4perl::Layout::SimpleLayout"/>   
        <param name="Threshold" value="error"/>
  </appender>   
  <appender name="FileAppndr1" class="org.apache.log4j.FileAppender">
        <layout class="Log::Log4perl::Layout::PatternLayout">
                <param name="ConversionPattern" 
                                      value="%d %4r [%t] %-5p %c %t - %m%n"/>
        </layout>
        <param name="File" value="t/tmp/DOMtest"/>
        <param name="Append" value="false"/>                
   </appender>
   
   <category name="a.b.c.d" additivity="false">
           <level value="warn"/>  <!-- note lowercase! -->
           <appender-ref ref="A1"/>
           
   </category>
   <category name="a.b">
           <priority value="info"/>  
           <appender-ref ref="A1"/>
   </category>
   <category name="animal.dog">
           <priority value="info"/>
           <appender-ref ref="FileAppndr1"/>
           <appender-ref ref="A2"/>
   </category>
   <category name="animal">
           <priority value="info"/>
           <appender-ref ref="FileAppndr1"/>
   </category>
   <category name="xa.b.c.d">
           <priority value="info"/>
           <appender-ref ref="A2"/>
   </category>
   <category name="xa.b">
           <priority value="warn"/>
           <appender-ref ref="A2"/>
   </category>
   
   <root>
           <priority value="warn"/>
           <appender-ref ref="FileAppndr1"/>
   </root>
   

</log4j:configuration>

EOL


#Log::Log4perl::init(\$config);

my $xmldata = Log::Log4perl::Config::config_read(\$xmlconfig);

my $propsconfig = <<EOL;
log4j.appender.A1 = Log::Log4perl::Appender::TestBuffer
log4j.appender.A1.layout = Log::Log4perl::Layout::SimpleLayout

log4j.appender.A2 = Log::Log4perl::Appender::TestBuffer
log4j.appender.A2.layout = Log::Log4perl::Layout::SimpleLayout

log4j.appender.BUF0 = Log::Log4perl::Appender::TestBuffer
log4j.appender.BUF0.layout = Log::Log4perl::Layout::SimpleLayout
log4j.appender.BUF0.Threshold = ERROR

log4j.appender.FileAppndr1 = org.apache.log4j.FileAppender
log4j.appender.FileAppndr1.layout = Log::Log4perl::Layout::PatternLayout
log4j.appender.FileAppndr1.layout.ConversionPattern = %d %4r [%t] %-5p %c %t - %m%n
log4j.appender.FileAppndr1.File = t/tmp/DOMtest
log4j.appender.FileAppndr1.Append = false

log4j.category.a.b.c.d = WARN, A1
log4j.category.a.b = INFO, A1

log4j.category.xa.b.c.d = INFO, A2
log4j.category.xa.b = WARN, A2

log4j.category.animal = INFO, FileAppndr1
log4j.category.animal.dog = INFO, FileAppndr1,A2

log4j.category = WARN, FileAppndr1

log4j.threshold = DEBUG

log4j.additivity.a.b.c.d = 0

EOL



my $propsdata = Log::Log4perl::Config::config_read(\$propsconfig);

#brute force testing here, not very granular, but it is thorough

eval {require Data::Dump};
my $dump_available;
if (! $@) {
    $dump_available = 1;
}


require File::Spec->catfile('t','compare.pl');

ok(Compare($xmldata, $propsdata)) || 
        do {
          if ($dump_available) {
              print STDERR "got: ",Data::Dump::dump($xmldata),"\n";
              print STDERR "================\n";
              print STDERR "expected: ", Data::Dump::dump($propsdata),"\n";
          }
        };


# =======================================================\
# test variable substitutions
# more brute force

$xmlconfig = <<'EOL';
<?xml version="1.0" encoding="UTF-8"?> 
<!DOCTYPE log4j:configuration SYSTEM "log4j.dtd">

<log4j:configuration xmlns:log4j="http://jakarta.apache.org/log4j/"
    threshold="${rootthreshold}">
  
  <appender name="${A1}" class="${testbfr}">
        <layout class="${simplelayout}"/>
  </appender>   
  <appender name="${A2}" class="${testbfr}">
        <layout class="Log::Log4perl::Layout::SimpleLayout"/>   
  </appender>   
  <appender name="BUF0" class="Log::Log4perl::Appender::TestBuffer">
        <layout class="Log::Log4perl::Layout::SimpleLayout"/>   
        <param name="${appthreshold}" value="${appthreshlevel}"/>
  </appender>   
  <appender name="FileAppndr1" class="org.apache.log4j.FileAppender">
        <layout class="Log::Log4perl::Layout::PatternLayout">
                <param name="${convpatt}" 
                                      value="${thepatt}"/>
        </layout>
        <param name="${pfile}" value="${pfileval}"/>
        <param name="Append" value="false"/>                
   </appender>
   
   <category name="${abcd}" additivity="${abcd_add}">
           <level value="${abcd_level}"/>  <!-- note lowercase! -->
           <appender-ref ref="A1"/>
           
   </category>
   <category name="a.b">
           <priority value="info"/>  
           <appender-ref ref="A1"/>
   </category>
   <category name="animal.dog">
           <priority value="info"/>
           <appender-ref ref="FileAppndr1"/>
           <appender-ref ref="A2"/>
   </category>
   <category name="animal">
           <priority value="info"/>
           <appender-ref ref="FileAppndr1"/>
   </category>
   <category name="xa.b.c.d">
           <priority value="info"/>
           <appender-ref ref="A2"/>
   </category>
   <category name="xa.b">
           <priority value="warn"/>
           <appender-ref ref="A2"/>
   </category>
   
   <root>
           <priority value="warn"/>
           <appender-ref ref="FileAppndr1"/>
   </root>
   

</log4j:configuration>

EOL


$ENV{rootthreshold} = 'debug';
$ENV{A1} = 'A1';
$ENV{A2} = 'A2';
$ENV{testbfr} = 'Log::Log4perl::Appender::TestBuffer';
$ENV{simplelayout} = 'Log::Log4perl::Layout::SimpleLayout';
$ENV{appthreshold} = 'Threshold';
$ENV{appthreshlevel} = 'error';
$ENV{convpatt} = 'ConversionPattern';
$ENV{thepatt} = '%d %4r [%t] %-5p %c %t - %m%n';
$ENV{pfile} = 'File';
$ENV{pfileval} = 't/tmp/DOMtest';
$ENV{abcd} = 'a.b.c.d';
$ENV{abcd_add} = 'false';
$ENV{abcd_level} = 'warn';
$ENV{a1_appenderref} = 'A1';

my $varsubsdata = Log::Log4perl::Config::config_read(\$xmlconfig);

ok(Compare($varsubsdata, $xmldata)) || 
        do {
          if ($dump_available) {
              print STDERR "got: ",Data::Dump::dump($varsubsdata),"\n";
              print STDERR "================\n";
              print STDERR "expected: ", Data::Dump::dump($xmldata),"\n";
          }
        };

#<param name="Threshold" value="error"/>
$xmlconfig = <<EOL;
<?xml version="1.0" encoding="utf-8"?> 
<log4perl:configuration xmlns:log4perl="http://log4perl.sourceforge.net/" threshold="debug" oneMessagePerAppender="true"> 
<appender name="AppGeneralScreen" class="Log::Log4perl::Appender::Screen"> 
<layout class="Log::Log4perl::Layout::SimpleLayout"/> 
</appender> 
<root> 
<priority value="WARN" /> 
<appender-ref ref="AppGeneralScreen" /> 
</root> 
</log4perl:configuration> 
EOL

Log::Log4perl::init( \$xmlconfig ); 
my $logger = Log::Log4perl->get_logger(); 
 
$logger->info("Info"); 
$logger->debug("Debug"); 
