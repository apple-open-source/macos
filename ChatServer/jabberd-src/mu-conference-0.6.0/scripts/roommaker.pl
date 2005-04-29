#!/usr/bin/perl
#
# roommaker.pl: Predefine persistent rooms for MU-Conference
# Requires: Digest::SHA1, XML::Simple
#
use strict;
use Digest::SHA1 qw(sha1_hex);
use XML::Simple;

# Declare variables
my $uid;
my $gid;
my $check;
my $name;
my $server;
my $output;
my $roomcfg;
my $noticecfg;
my $ownerlist;
my $adminlist;
my $memberlist;
my $outcastlist;
my $roomsconfig;
my $FH;

#
# Get spool directory
print "Please enter spool directory path (e.g. /usr/local/jabber/spool): ";
my $spooldir = <>;
chomp $spooldir;

if( $spooldir eq "")
{
    print "No spool directory\n";
    exit;
}
elsif( ! -d $spooldir )
{
    print "Spool directory does not exist. Exiting \n";
    exit;
}

# Fix spooldir variable, if necessary
$spooldir =~ s/\/$//;

# Get uid/gid from spool
$uid = (stat($spooldir))[4];
$gid = (stat($spooldir))[5];
umask "0027";

#
# Get room jid
print "Please enter jid for the room: ";
my $jid = <>;
chomp $jid;

if( !($jid =~ /\w@\w/) )
{
    print "Bad JID - Exiting\n";
    exit;
}

($name, $server) = split(/@/, $jid);

my $hash = sha1_hex($jid);

#
# Check if directory exists
if( ! -d "$spooldir/$server/" )
{
    print "$spooldir/$server/ doesn't exist - Create? (Y/N) ";

    my $input = <>;
    
    if( $input =~ /^[Y|y]/ )
    {
	print "Creating Directory\n";

	mkdir("$spooldir/$server", 0777);
	chown $uid, $gid, "$spooldir/$server";
    }
    else
    {
	print "Unable to continue. Exiting\n";
	exit
    }
}

#
# Print Header
print "\nConfiguring room $jid\n";
print "Filename: $spooldir/$server/$hash.xml\n";

#
# Check if room already defined
if( -f "$spooldir/$server/$hash.xml")
{
    print "Room already defined. Exiting\n";
    exit;
}

$roomcfg->{xdbns} = "muc:room:config";

print "\nGeneral Options\n---\n";
$roomcfg->{name} = [getText("Room name", $name)];
$roomcfg->{secret} = [getText("Password", "")];
$roomcfg->{description} = [getText("Room description/MOTD", "")];
$roomcfg->{subject} = [getText("Room subject", "")];
$roomcfg->{creator} = [getText("Bare JID of room creator", "")];
$roomcfg->{public} = [getBoolean("Is room public", 0)];
$roomcfg->{maxusers} = [getValue("Maximum Users ", 0)];
$roomcfg->{persistent} = [1]; # Has to be persistent

print "\nPermission Options\n---\n";
$roomcfg->{visible} = [getBoolean("Allow non-admins to see real jids", 0)];
$roomcfg->{subjectlock} = [getBoolean("Can users change subject", 0)];
$roomcfg->{private} = [getBoolean("Allow users to IQ query other users", 0)];

print "\nLegacy Options:\n---\n";
$roomcfg->{legacy} = [getBoolean("Consider all clients legacy", 0)];
$noticecfg->{join} = [getText("Legacy join message", "")];
$noticecfg->{leave} = [getText("Legacy leave message", "")];
$noticecfg->{rename} = [getText("Legacy rename message", "")];
$roomcfg->{notice} = [$noticecfg];

print "\nModeration Options:\n---\n";
$roomcfg->{moderated} = [getBoolean("Is room moderated", 0)];

if($roomcfg->{moderated}[0] == 0)
{
    print "Skipping Moderation options\n";
    $roomcfg->{defaulttype} = [0];
    $roomcfg->{privmsg} = [0];
}
else
{
    $roomcfg->{defaulttype} = [getBoolean("Default entry type of participant", 0)];
    $roomcfg->{privmsg} = [getBoolean("Default entry type of participant", 0)];
}

print "\nMember-Only Options:\n---\n";
$roomcfg->{invitation} = [getBoolean("Make room member-only", 0)];

if($roomcfg->{invitation}[0] == 0)
{
    print "Skipping Moderation options\n";
    $roomcfg->{invites} = [0];
}
else
{
    $roomcfg->{invites} = [getBoolean("Allow members to send invites", 0)];
}

print "\nLogging Options:\n---\n";
$roomcfg->{logging} = [getBoolean("Enable native room logging", 0)];

if($roomcfg->{logging}[0] == 0)
{
    print "Skipping Logging options\n";
    $roomcfg->{logformat} = [0];
}
else
{
    $roomcfg->{logformat} = [getOption("Log Format\n0] Plain Text\n1] XML\n2] XHTML\n", 0)];
}

print "\nOwner List:\n---\n";
$ownerlist->{xdbns} = "muc:list:owner";
$ownerlist->{item} = [getList("JID of owner")];

print "\nAdmin List:\n---\n";
$adminlist->{xdbns} = "muc:list:admin";
$adminlist->{item} = [getList("JID of admin")];

print "\nMember List:\n---\n";
$memberlist->{xdbns} = "muc:list:member";
$memberlist->{item} = [getList("JID of member")];

print "\nOutcast List:\n---\n";
$outcastlist->{xdbns} = "muc:list:outcast";
$outcastlist->{item} = [getList("JID of outcast")];

$output->{room} = $roomcfg;
$output->{list} = [$ownerlist, $adminlist, $memberlist, $outcastlist];

print "\nWriting Room definition file\n";
open(DATA, ">$spooldir/$server/$hash.xml");
print DATA XMLout($output, rootname => "xdb");
close(DATA);

if( ! -f "$spooldir/$server/rooms.xml")
{
    print "Room registry not found. Creating\n";

    my $list;
    my $roomitem;
    $roomitem->{name} = $jid;
    $roomitem->{jid} = "$hash\@$server";
    $list->{item} = [$roomitem];
    $list->{xdbns} = "muc:room:list";
    $roomsconfig->{registered} = [$list];
}
else
{
    my $list;
    my $roomitem;

    $roomsconfig = XMLin("$spooldir/$server/rooms.xml");
    $roomitem->{name} = $jid;
    $roomitem->{jid} = "$hash\@$server";
    $roomsconfig->{registered}->{item} = [$roomsconfig->{registered}->{item}, $roomitem];
} 

print "\nWriting updated Room registry file\n";
open(DATA, ">$spooldir/$server/rooms.xml");
print DATA XMLout($roomsconfig, rootname => "xdb");
close(DATA);

exit;

#
#Functions
sub getText
{
    my $text = shift;
    my $default = shift;

    print "$text (text) [Default: $default]: ";
    my $value = <>;
    chomp $value;

    if($value eq "")
    {
	return $default;
    }
    else
    {
        return $value;
    }
}

sub getBoolean
{
    my $text = shift;
    my $default = shift;

    print "$text (0/1) [Default: $default]: ";
    my $value = <>;
    chomp $value;

    if($value eq "" or !( $value =~ /^[1|0]$/))
    {
	return $default;
    }
    else
    {
        return $value;
    }
}

sub getValue
{
    my $text = shift;
    my $default = shift;

    print "$text (value) [Default: $default]: ";
    my $value = <>;
    chomp $value;

    if($value eq "" or !( $value =~ /^(\d*)$/))
    {
	return $default;
    }
    else
    {
        return $value;
    }
}

sub getOption
{
    my $text = shift;
    my $default = shift;

    print "$text [Default: $default]: ";
    my $value = <>;
    chomp $value;

    if($value eq "" or !( $value =~ /^(\d*)$/))
    {
	return $default;
    }
    else
    {
        return $value;
    }
}

sub getList
{
    my $text = shift;
    my $data = 1;
    my @list;

    while($data)
    {
        print "$text (Empty line to exit): ";
        my $value = <>;
        chomp $value;

        if($value eq "")
        {
	    $data = 0;
        }
        else
        {
            my %users;
	    $users{jid} = $value;
	    push @list, \%users;
	}
    }

    return @list;
}
