#!/usr/bin/perl -w
# Copyright 2009 Apple Inc. All rights reserved.
# Migrate a 10.5,10.6,10.7 configuration to 10.7

use Getopt::Std;
use File::Copy;
use File::Basename;
use XML::Simple;
use strict;

my $DEBUG = 0;
my $SERVERMGR_JABBER_CGI_PATH = "/usr/share/servermgrd/cgi-bin/servermgr_jabber";
my $MKTEMP_PATH = "/usr/bin/mktemp";
my $logPath = "/Library/Logs/Migration/jabbermigrator.log";
my $MKDIR_PATH = "/bin/mkdir";
my $PLISTBUDDY = "/usr/libexec/PlistBuddy";
my $CERT_PATH = "/etc/certificates";

sub usage {
    print "This script handles configuration migration to 10.7 server.\n";
    print "It uses servermgr_jabber to perform the necessary changes to jabberd2 config files.\n\n";
    print "Usage:  $0 [-d] [-c path] [-s version]\n";
    print "Flags:\n";
    print " -d: Debug mode.\n";
    print " -c <path>: Source jabberd config file to use. Should be a \"com.apple.ichatserver.plist\" for\n";
    print "            10.5, 10.6, 10.7 (REQUIRED)\n";
    print " -s <source version>: Source OS version. \"10.5\", \"10.6\" and \"10.7\" supported. (REQUIRED)\n";
    print " -?, -h: Show usage info.\n";
}

sub log_message
{
    open(LOGFILE, ">>$logPath") || die "$0: cannot open $logPath: $!";
    my $time = localtime();
    print LOGFILE "$time: ". basename($0). ": @_\n";
    print "@_\n" if $DEBUG;
    close(LOGFILE);
}

sub bail
{
    &log_message(@_);
    &log_message("Aborting!");
    exit 1;
}


############ MAIN
if (! -d dirname($logPath) ) {
    my $logDir = dirname($logPath);
    qx{ $MKDIR_PATH -p $logDir };
    if ($? != 0) {
        &log_message("\"$MKDIR_PATH -p $logDir\" returned failure status $?");
    }
}

my %opts;
getopts('ds:c:?h', \%opts);

if (defined $opts{'?'} || defined $opts{'h'}) {
    &usage;
    exit 0;
}

if (defined $opts{'d'}) {
    $DEBUG = 1;
}

my $src_ver;
if (defined $opts{'s'}) {
    my $tmp_src_ver = $opts{'s'};
    if ($opts{'s'} =~ /^10\.5/) {
        &log_message("Treating source OS as 10.5.x");
        $src_ver = "10.5";
    } elsif ($opts{'s'} =~ /^10\.6/) {
        &log_message("Treating source OS as 10.6.x");
        $src_ver = "10.6";
    } elsif ($opts{'s'} =~ /^10\.7/) {
        &log_message("Treating source OS as 10.7.x");
        $src_ver = "10.7";
    } else {
        &bail("Unrecognized source OS version specified, don't know how to parse the source config file. Aborting.\n");
    }
} else {
    &bail("Source OS version was not specified, aborting.\n");
}

my $source_config = "";
if (defined $opts{'c'} && $opts{'c'} ne "") {
    $source_config = $opts{'c'};
} else {
    &bail("You must specify the source configuration file to use, aborting");
}

open(IN, "<$source_config") || &bail("ERROR: Cannot open source config $source_config: $!");
my @lines = <IN>;
chomp(@lines);
close(IN);

my $tmpname;
for (my $i = 0; $i < 5; $i++) {
    $tmpname = qx{ $MKTEMP_PATH /tmp/jabber_migration.XXXXXXXXXXXXXXXXXXXXXXXX };
    chomp($tmpname);
    if (-e $tmpname) {
        last;
    }
    if ($i == 4) {
        &bail("ERROR: Cannot create temporary file: $tmpname");
    }
}

my $res;
&log_message("Importing settings from file: $source_config");
my $xs1 = XML::Simple->new (KeepRoot => 1);
my $doc = $xs1->XMLin($source_config);
my @keys;
for (my $i = 0; (defined $doc->{plist}->{dict}->{key}->[$i]); $i++) {
    push(@keys, $doc->{plist}->{dict}->{key}->[$i]);
    if ($DEBUG) { print "found key: ".$doc->{plist}->{dict}->{key}->[$i]."\n"; }
}

open(OUT, ">$tmpname") || &bail("ERROR: Could not open file $tmpname: $!");
print OUT <<"EOF";
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple Computer//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
<key>command</key>
<string>writeSettings</string>
<key>configuration</key>
<dict>
EOF

# Gather certificate file data for later use
my @cert_files;
my $ret = opendir(CERTS_DIR, $CERT_PATH);
if (! $ret) {
    &log_message("ERROR: unable to read dir: $CERT_PATH");
} else {
    @cert_files = readdir(CERTS_DIR);
    closedir(CERTS_DIR);
    chomp(@cert_files);
}
my $prospective_chainfile = "";
my $configured_chainfile = 0;

NEXTKEY: foreach my $key (@keys) {
    $ret = qx { $PLISTBUDDY -x -c "Print :$key:" "$source_config" };
    if ($DEBUG) { print "DEBUG: $PLISTBUDDY output: $ret\n"; }
    chomp($ret);
    my @lines = split("\n", $ret);
    if ($ret eq "" || $#lines < 4) {
        &log_message("ERROR: PlistBuddy problem getting value for key \"$key\" from file \"$source_config\", it returned:\n$ret\n");
        next;
    }          
    # Magic numbers used assume this output from PlistBuddy:
    # <?xml version="1.0" encoding="UTF-8"?>
    # <!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
    # <plist version="1.0">
    # <string>example</string>
    # </plist>

	# We do not want to carry over the paths for data - they should already be defined for the target system
	# and may be changed manually post-migration if desired
    if ($key eq "dataLocation" ||
                $key eq "jabberdDatabasePath" ||
                $key eq "savedChatsLocation") {
		next;
	}

    # For 10.5 sources, certificates were named differently than 10.6+.
    # 10.5: mycert.crtkey
    # 10.6: mycert.6E60F5812D4DE84616E92E1ACB9F0B35ED6A2229.concat.pem
    # 10.5: mycert.chcrt
    # 10.6: mycert.6E60F5812D4DE84616E92E1ACB9F0B35ED6A2229.chain.pem
    # We need to use the migrated certificates in the new configuration.
    # SSLCAFile (chain file) notes:
    #   If the source has sslCAFile defined, AND it has been moved to a new filename, use the new file.
    #   If the source has sslCAFile defined, AND and there's no new migrated version, use the original sslCAFile value.
    #   If the source has no sslCAFile defined, but has a valid sslKeyFile, then look for a chain file
    #      that has been created during migration based on the old sslKeyFile.
    #   (10.5 systems that have an sslKeyFile but no corresponding chain file are going to 
    #   have a new chain file generated for them during migration, so lets use it.)
    my $cert_prefix;
    my $cert;
    if ($key eq "sslKeyFile") {
        if ($lines[3] =~ /^<string>(.*)\.crtkey<\/string>$/) {
            $cert_prefix = $1;
            $cert_prefix =~ s/.*[\/](.*)/$1/;
            foreach $cert (@cert_files) {
                if ($cert =~ /^$cert_prefix.([A-F0-9]+).concat.pem$/) {
                    print OUT "\t\t<key>$key</key>\n";
                    print OUT "\t\t<string>$CERT_PATH/$cert</string>\n";
                    $prospective_chainfile = "$CERT_PATH/$cert_prefix.$1.chain.pem";
                    next NEXTKEY;
                }
            }
        }
    } elsif ($key eq "sslCAFile") {
       if ($lines[3] =~ /^<string>(.*)\.chcrt<\/string>$/) {
            $cert_prefix = $1;
            $cert_prefix =~ s/.*[\/](.*)/$1/;
            foreach $cert (@cert_files) {
                if ($cert =~ /^$cert_prefix.([A-F0-9]+).chain.pem$/) {
                    print OUT "\t\t<key>sslCAFile</key>\n";
                    print OUT "\t\t<string>$CERT_PATH/$cert</string>\n";
                    $configured_chainfile = 1;
                    next NEXTKEY;
                }
            }
        } elsif ($lines[3] !~ /^<string><\/string>$/) {
            $configured_chainfile = 1;
        }
    }

    print OUT "\t\t<key>$key</key>\n";
    for (my $i = 3; $i < $#lines; $i++) {
        print OUT "\t\t$lines[$i]\n";
    }
} ## NEXTKEY

if (! $configured_chainfile && $prospective_chainfile ne "" && -e $prospective_chainfile) {
    print OUT "\t\t<key>sslCAFile</key>\n";
    print OUT "\t\t<string>$prospective_chainfile</string>\n";
}

print OUT <<"EOF";
</dict>
</dict>
</plist>
EOF

&log_message("Importing settings from file: $source_config");
$res = qx{ $SERVERMGR_JABBER_CGI_PATH < $tmpname };
unlink($tmpname);

&log_message("New servermgr_jabber settings:\n$res");
&log_message("Upgrade completed successfully.");
