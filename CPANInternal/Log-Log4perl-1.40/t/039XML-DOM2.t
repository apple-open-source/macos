
BEGIN { 
    if($ENV{INTERNAL_DEBUG}) {
        require Log::Log4perl::InternalDebug;
        Log::Log4perl::InternalDebug->enable();
    }
}

use Test::More;
use Log::Log4perl;
use strict;

our $no_XMLDOM;

BEGIN {
    my $dvrq = $Log::Log4perl::DOM_VERSION_REQUIRED;
    eval {
        require XML::DOM;
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
        plan tests => 4;
    }
}

if ($no_XMLDOM){
    ok(1);
    exit(0);
}


my $xmlconfig = <<'EOL';
<?xml version="1.0" encoding="UTF-8"?> 
<!DOCTYPE log4perl:configuration SYSTEM "log4perl.dtd">

<log4perl:configuration xmlns:log4perl="http://log4perl.sourceforge.net/"
    threshold="debug" oneMessagePerAppender="true">
    
<log4perl:appender name="jabbender" class="Log::Dispatch::Jabber">
          <param-nested name="login">
                <param name="hostname" value="a.jabber.server"/>
                <param name="password" value="12345"/>
                <param name="port" value="5222"/>
                <param name="resource" value="logger"/>
                <param name="username" value="bobjones"/>
         </param-nested>
         <param name="to" value="bob@a.jabber.server"/>
         <param-text name="to">mary@another.jabber.server</param-text>
          <layout class="Log::Log4perl::Layout::SimpleLayout"/>
         
</log4perl:appender>
<log4perl:appender name="DBAppndr2" class="Log::Log4perl::Appender::DBI">
          <param name="warp_message" value="0"/>
          <param name="datasource" value="DBI:CSV:f_dir=t/tmp"/>
          <param name="bufferSize" value="2"/>
          <param name="password" value="sub { $ENV{PWD} }"/>
           <param name="username" value="bobjones"/>
          
          <param-text name="sql">insert into log4perltest (loglevel, message, shortcaller, thingid, category, pkg, runtime1, runtime2) values (?,?,?,?,?,?,?,?)</param-text> 
           <param-nested name="params">
                <param name="1" value="%p"/>
                <param name="3" value="%5.5l"/>
                <param name="5" value="%c"/>
                <param name="6" value="%C"/>
           </param-nested>
                
           <layout class="Log::Log4perl::Layout::NoopLayout"/>
         
</log4perl:appender>
<category name="animal.dog">
           <priority value="info"/>
           <appender-ref ref="jabbender"/>
</category>

<PatternLayout>
    <cspec name="G"><![CDATA[sub { return "UID $< GID $("; }]]></cspec>
</PatternLayout>


</log4perl:configuration>
EOL


my $xmldata = Log::Log4perl::Config::config_read(\$xmlconfig);

my $propsconfig = <<'EOL';

log4j.category.animal.dog   = INFO, jabbender
log4j.threshold = DEBUG

log4j.oneMessagePerAppender=1

log4j.PatternLayout.cspec.G=sub { return "UID $< GID $("; }

log4j.appender.jabbender          = Log::Dispatch::Jabber
log4j.appender.jabbender.layout   = Log::Log4perl::Layout::SimpleLayout
log4j.appender.jabbender.login.hostname = a.jabber.server
log4j.appender.jabbender.login.port = 5222
log4j.appender.jabbender.login.username = bobjones
log4j.appender.jabbender.login.password = 12345
log4j.appender.jabbender.login.resource = logger
log4j.appender.jabbender.to = bob@a.jabber.server
log4j.appender.jabbender.to = mary@another.jabber.server

log4j.appender.DBAppndr2             = Log::Log4perl::Appender::DBI
log4j.appender.DBAppndr2.username  = bobjones
log4j.appender.DBAppndr2.datasource = DBI:CSV:f_dir=t/tmp
log4j.appender.DBAppndr2.password = sub { $ENV{PWD} }
log4j.appender.DBAppndr2.sql = insert into log4perltest (loglevel, message, shortcaller, thingid, category, pkg, runtime1, runtime2) values (?,?,?,?,?,?,?,?)
log4j.appender.DBAppndr2.params.1 = %p    
log4j.appender.DBAppndr2.params.3 = %5.5l
log4j.appender.DBAppndr2.params.5 = %c
log4j.appender.DBAppndr2.params.6 = %C

log4j.appender.DBAppndr2.bufferSize=2
log4j.appender.DBAppndr2.warp_message=0
    
#noop layout to pass it through
log4j.appender.DBAppndr2.layout    = Log::Log4perl::Layout::NoopLayout


EOL



my $propsdata = Log::Log4perl::Config::config_read(\$propsconfig);

#brute force testing here, not very granular, but it is thorough

eval {require Data::Dump};
my $dump_available;
if (! $@) {
    $dump_available = 1;
}


require 't/compare.pl';

ok(Compare($xmldata, $propsdata)) || 
        do {
          if ($dump_available) {
              print STDERR "got: ",Data::Dump::dump($xmldata),"\n================\n";
              print STDERR "expected: ", Data::Dump::dump($propsdata),"\n";
          }
        };

# ------------------------------------------------
#ok, let's get more hairy, make-believe

$xmlconfig = <<'EOL';
<?xml version="1.0" encoding="UTF-8"?> 
<!DOCTYPE log4perl:configuration SYSTEM "log4perl.dtd">

<log4perl:configuration xmlns:log4perl="http://log4perl.sourceforge.net/">

<log4perl:appender name="A1" class="Log::Dispatch::Jabber">
          <param-nested name="A">
                <param-text name="1">fffff</param-text>
                <param name="list" value="11111"/>
                <param name="list" value="22222"/>
                <param-nested name="subnest">
                    <param-text name="a">hhhhh</param-text>
                    <param name="list" value="aaaaa"/>
                    <param name="list" value="bbbbb"/>
                </param-nested>
         </param-nested>
         <param-text name="to">mary@another.jabber.server</param-text>
          <layout class="Log::Log4perl::Layout::SimpleLayout"/>
</log4perl:appender>

</log4perl:configuration>

EOL

$propsconfig = <<'EOL';

log4j.appender.A1= Log::Dispatch::Jabber
log4j.appender.A1.A.1=fffff
log4j.appender.A1.A.list=11111
log4j.appender.A1.A.list=22222
log4j.appender.A1.A.subnest.a=hhhhh
log4j.appender.A1.A.subnest.list=aaaaa
log4j.appender.A1.A.subnest.list=bbbbb
log4j.appender.A1.to=mary@another.jabber.server
log4j.appender.A1.layout=Log::Log4perl::Layout::SimpleLayout
EOL

$xmldata = Log::Log4perl::Config::config_read(\$xmlconfig);
$propsdata = Log::Log4perl::Config::config_read(\$propsconfig);

ok(Compare($xmldata, $propsdata)) || 
        do {
          if ($dump_available) {
              print STDERR "got: ",Data::Dump::dump($xmldata),"\n================\n";
              print STDERR "expected: ", Data::Dump::dump($propsdata),"\n";
          }
        };


# ------------------------------------------------
#now testing things like cspecs, code refs

$xmlconfig = <<'EOL';
<?xml version="1.0" encoding="UTF-8"?> 
<!DOCTYPE log4perl:configuration SYSTEM "log4perl.dtd">

<log4perl:configuration xmlns:log4perl="http://log4perl.sourceforge.net/">



<log4perl:appender name="appndr1" class="Log::Log4perl::Appender::TestBuffer">
    <log4perl:layout class="org.apache.log4j.PatternLayout">
        <param name="ConversionPattern" value = "%K xx %G %U"/>
        <cspec name="K">
            sub { return sprintf "%1x", $$}
        </cspec>
        <cspec name="G">
            sub {return 'thisistheGcspec'}
        </cspec>
    </log4perl:layout>
</log4perl:appender>

<category name="plant">
        <priority value="debug"/>  
        <appender-ref ref="appndr1"/>
</category>

<PatternLayout>
    <cspec name="U"><![CDATA[
        sub {                       return "UID $< GID $("; }                          
    ]]></cspec>
</PatternLayout>



</log4perl:configuration>


EOL


$propsconfig = <<'EOL';
log4j.category.plant    = DEBUG, appndr1

log4j.PatternLayout.cspec.U =       \
        sub {                       \
            return "UID $< GID $("; \
        }                           \

log4j.appender.appndr1        = Log::Log4perl::Appender::TestBuffer
log4j.appender.appndr1.layout = org.apache.log4j.PatternLayout
log4j.appender.appndr1.layout.ConversionPattern = %K xx %G %U

log4j.appender.appndr1.layout.cspec.K = sub { return sprintf "%1x", $$}

log4j.appender.appndr1.layout.cspec.G = sub {return 'thisistheGcspec'}
EOL

$xmldata = Log::Log4perl::Config::config_read(\$xmlconfig);
$propsdata = Log::Log4perl::Config::config_read(\$propsconfig);

ok(Compare($xmldata, $propsdata)) || 
        do {
          if ($dump_available) {
              print STDERR "got: ",Data::Dump::dump($xmldata),"\n================\n";
              print STDERR "expected: ", Data::Dump::dump($propsdata),"\n";
          }
        };



#now we test variable substitution
#brute force again
my $varsubstconfig = <<'EOL';
<?xml version="1.0" encoding="UTF-8"?> 
<!DOCTYPE log4perl:configuration SYSTEM "log4perl.dtd">

<log4perl:configuration xmlns:log4perl="http://log4perl.sourceforge.net/"
    threshold="debug" oneMessagePerAppender="${onemsgperappnder}">
    
<log4perl:appender name="jabbender" class="${jabberclass}">
          <param-nested name="${paramnestedname}">
                <param name="${hostname}" value="${hostnameval}"/>
                <param name="${password}" value="${passwordval}"/>
                <param name="port" value="5222"/>
                <param name="resource" value="logger"/>
                <param name="username" value="bobjones"/>
         </param-nested>
         <param name="to" value="bob@a.jabber.server"/>
         <param-text name="to">${topcdata}</param-text>
          <layout class="Log::Log4perl::Layout::SimpleLayout"/>
         
</log4perl:appender>
<log4perl:appender name="DBAppndr2" class="Log::Log4perl::Appender::DBI">
          <param name="warp_message" value="0"/>
          <param name="datasource" value="DBI:CSV:f_dir=t/tmp"/>
          <param name="bufferSize" value="2"/>
          <param name="password" value="sub { $ENV{PWD} }"/>
           <param name="username" value="bobjones"/>
          
          <param-text name="sql">insert into ${tablename} (loglevel, message, shortcaller, thingid, category, pkg, runtime1, runtime2) values (?,?,?,?,?,?,?,?)</param-text> 
           <param-nested name="params">
                <param name="1" value="%p"/>
                <param name="3" value="%5.5l"/>
                <param name="5" value="%c"/>
                <param name="6" value="%C"/>
           </param-nested>
                
           <layout class="Log::Log4perl::Layout::NoopLayout"/>
         
</log4perl:appender>
<category name="animal.dog">
           <priority value="info"/>
           <appender-ref ref="jabbender"/>
</category>

<PatternLayout>
    <cspec name="${cspecname}"><![CDATA[sub { ${perlcode} }]]></cspec>
</PatternLayout>


</log4perl:configuration>
EOL

$ENV{onemsgperappnder} = 'true';
$ENV{jabberclass} = 'Log::Dispatch::Jabber';
$ENV{paramnestedname} = 'login';
$ENV{hostname} = 'hostname';
$ENV{hostnameval} = 'a.jabber.server';
$ENV{password} = 'password';
$ENV{passwordval} = '12345';
$ENV{topcdata} = 'mary@another.jabber.server';
$ENV{tablename} = 'log4perltest';
$ENV{cspecname} = 'G';
$ENV{perlcode} = 'return "UID $< GID $(";';

my $varsubstdata = Log::Log4perl::Config::config_read(\$varsubstconfig);



ok(Compare($xmldata, $propsdata)) || 
        do {
          if ($dump_available) {
              print STDERR "got: ",Data::Dump::dump($xmldata),"\n================\n";
              print STDERR "expected: ", Data::Dump::dump($propsdata),"\n";
          }
        };

