#adding filters to XML-DOM configs --kg

use Test::More;
use Log::Log4perl;
use strict;
use Data::Dumper;
use File::Spec;

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
        plan tests => 3;
    }
}

if ($no_XMLDOM){
    ok(1);
    exit(0);
}


#brute force testing here, not very granular, but it is thorough

eval {require Data::Dump};
my $dump_available;
if (! $@) {
    $dump_available = 1;
}
require File::Spec->catfile('t','compare.pl');

# *****************************************************
# first, test a very basic filter setup
# *****************************************************

my $xmlconfig = <<EOL;
<?xml version="1.0" encoding="UTF-8"?> 
<!DOCTYPE log4j:configuration SYSTEM "log4j.dtd">

<log4j:configuration xmlns:log4j="http://jakarta.apache.org/log4j/">
    
  <appender name="A1" class="Log::Log4perl::Appender::TestBuffer">
        <layout class="Log::Log4perl::Layout::SimpleLayout"/>
        <filter class="Log::Log4perl::Filter::Boolean">
            <param name="logic" value="!Match3 &amp;&amp; (Match1 || Match2)"/> 
        </filter>
  </appender>   
   
   <root>
           <priority value="info"/>
           <appender-ref ref="A1"/>
   </root>
   
</log4j:configuration>

EOL


#Log::Log4perl::init(\$config);

my $xmldata = Log::Log4perl::Config::config_read(\$xmlconfig);

my $propsconfig = <<EOL;
log4perl.category = INFO, A1

log4perl.appender.A1        = Log::Log4perl::Appender::TestBuffer
log4perl.appender.A1.Filter = Log::Log4perl::Filter::Boolean
log4perl.appender.A1.Filter.logic = !Match3 && (Match1 || Match2)
log4perl.appender.A1.layout = Log::Log4perl::Layout::SimpleLayout


EOL



my $propsdata = Log::Log4perl::Config::config_read(\$propsconfig);

ok(Compare($xmldata, $propsdata)) || 
        do {
          if ($dump_available) {
              print STDERR "got: ",Data::Dump::dump($xmldata),"\n";
              print STDERR "================\n";
              print STDERR "expected: ", Data::Dump::dump($propsdata),"\n";
          }
        };


# *****************************************************
# second, log4perl's boolean filters
# *****************************************************

$xmlconfig = <<EOL;
<?xml version="1.0" encoding="UTF-8"?> 
<!DOCTYPE log4perl:configuration SYSTEM "log4perl.dtd">

<log4perl:configuration xmlns:log4perl="http://log4perl.sourceforge.net/">

    
  <appender name="A1" class="Log::Log4perl::Appender::TestBuffer">
        <layout class="Log::Log4perl::Layout::SimpleLayout"/>
        <filter class="Log::Log4perl::Filter::Boolean">
            <param name="logic" value="!Match3 &amp;&amp; (Match1 || Match2)"/> 
        </filter>
  </appender>   
  
  <log4perl:filter name="Match1" value="sub { /let this through/ }" />
  
  <log4perl:filter name="Match2">sub { /and that, too/ }</log4perl:filter>
  
  <log4perl:filter name="Match3" class="Log::Log4perl::Filter::StringMatch">
    <param name="StringToMatch" value="suppress"/>
    <param name="AcceptOnMatch" value="true"/>
  </log4perl:filter>
  
  <log4perl:filter name="MyBoolean" class="Log::Log4perl::Filter::Boolean">
    <param name="logic" value="!Match3 &amp;&amp; (Match1 || Match2)"/>
  </log4perl:filter>
  
   
   <root>
           <priority value="info"/>
           <appender-ref ref="A1"/>
   </root>
   
</log4perl:configuration>
EOL


#Log::Log4perl::init(\$config);

$xmldata = Log::Log4perl::Config::config_read(\$xmlconfig);

$propsconfig = <<EOL;
log4perl.category = INFO, A1

log4perl.filter.Match1       = sub { /let this through/ }
log4perl.filter.Match2       = sub { /and that, too/ }
log4perl.filter.Match3       = Log::Log4perl::Filter::StringMatch
log4perl.filter.Match3.StringToMatch = suppress
log4perl.filter.Match3.AcceptOnMatch = true

log4perl.filter.MyBoolean       = Log::Log4perl::Filter::Boolean
log4perl.filter.MyBoolean.logic = !Match3 && (Match1 || Match2)

log4perl.appender.A1        = Log::Log4perl::Appender::TestBuffer
log4perl.appender.A1.Filter = Log::Log4perl::Filter::Boolean
log4perl.appender.A1.Filter.logic = !Match3 && (Match1 || Match2)
log4perl.appender.A1.layout = Log::Log4perl::Layout::SimpleLayout


EOL



$propsdata = Log::Log4perl::Config::config_read(\$propsconfig);

#brute force testing here, not very granular, but it is thorough


ok(Compare($xmldata, $propsdata)) || 
        do {
          if ($dump_available) {
              print STDERR "got: ",Data::Dump::dump($xmldata),"\n";
              print STDERR "================\n";
              print STDERR "expected: ", Data::Dump::dump($propsdata),"\n";
          }
        };




# *****************************************************
# third, level range filter, just for something different
# *****************************************************


$xmlconfig = <<EOL;
<?xml version="1.0" encoding="UTF-8"?> 
<!DOCTYPE log4perl:configuration SYSTEM "log4perl.dtd">

<log4perl:configuration xmlns:log4perl="http://log4perl.sourceforge.net/">
    
  <log4perl:appender name="A1" class="Log::Log4perl::Appender::TestBuffer">
        <layout class="Log::Log4perl::Layout::SimpleLayout"/>
        <filter-ref id="Range1"/>
  </log4perl:appender>   
  
  <log4perl:filter name="Range1" class="Log::Log4perl::Filter::LevelRange">
    <param name="LevelMin" value="info"/>
    <param name="LevelMax" value="warn"/>
    <param name="AcceptOnMatch" value="true"/>
  </log4perl:filter>
  
  <root>
     <priority value="debug"/>
     <appender-ref ref="A1"/>
   </root>
</log4perl:configuration>
EOL


#Log::Log4perl::init(\$config);

$xmldata = Log::Log4perl::Config::config_read(\$xmlconfig);

$propsconfig = <<EOL;
log4perl.category = DEBUG, A1
log4perl.filter.Range1      = Log::Log4perl::Filter::LevelRange
log4perl.filter.Range1.LevelMin = INFO
log4perl.filter.Range1.LevelMax = WARN
log4perl.filter.Range1.AcceptOnMatch = true
log4perl.appender.A1        = Log::Log4perl::Appender::TestBuffer
log4perl.appender.A1.Filter = Range1
log4perl.appender.A1.layout = Log::Log4perl::Layout::SimpleLayout


EOL

$propsdata = Log::Log4perl::Config::config_read(\$propsconfig);

#brute force testing here, not very granular, but it is thorough

ok(Compare($xmldata, $propsdata)) || 
        do {
          if ($dump_available) {
              print STDERR "got: ",Data::Dump::dump($xmldata),"\n";
              print STDERR "================\n";
              print STDERR "expected: ", Data::Dump::dump($propsdata),"\n";
          }
        };



