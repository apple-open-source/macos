#!/usr/bin/perl -w
# Copyright 2001 John Summerfield, summer@summer.ami.com.au
# GPL 2 applies.
#
($flags, $links, $owner, $gowner, $size, $month, $day, $timeOrDate, $name, $junk, $junk2, $junk1) ='';
$RemoteHost="ftp.ccil.org";
$LocalDir="/home/u03/incoming/";
$FilePattern="/pub/esr/fetchmail/fetchmail\*src.rpm";
$GrepArgs="fetchmail-[5-9]";
$no=0;
$TempFile=`mktemp /var/tmp/getfetchmail.XXXXXX`;
@files=`echo dir $FilePattern | ftp $RemoteHost  | egrep $GrepArgs`;
chomp @files;
open(FTP, "| ftp -d -v $RemoteHost | egrep '^213|MDTM'  >$TempFile");
foreach $L (@files)
{
	++$no;
	$L =~ s/  */,/g;
	($flags, $links, $owner, $gowner, $size, $month, $day, $timeOrDate, $name, $junk) = split /,/,$L;
	next unless substr($timeOrDate,2,1) eq ':';
	print FTP "modtime $name\n";
#	last if $no > 4;
}
close FTP;

$SavedTime=0;
$time=1;
$SavedName='';
open (FILES,$TempFile);
while ($rec = <FILES>)
{
	chomp $rec;
	($junk1, $junk2, $filename) = split / /,$rec if substr($rec,0,4) eq '--->';
	$time = substr($rec,4) if substr($rec,0,3) eq '213';
	if (($time > $SavedTime) && (substr($rec,0,3) eq '213'))
	{
		$SavedTime=$time;
		$SavedName=$filename;
	}
}
close FILES;
$LocalName = $SavedName; $LocalName =~ s=.*/==;
$LocalName = $LocalDir . $LocalName;
$Y=substr($SavedTime,0,4);
$M=substr($SavedTime,4,2);
$D=substr($SavedTime,6,2);
$h=substr($SavedTime,8,2);
$m=substr($SavedTime,10,2);
$s=substr($SavedTime,12,2);
print "I should get $SavedName and store it in $LocalName\n";
open(SH,"|/bin/bash");
print SH <<zz
set -x
echo get $SavedName $LocalName \| ftp $RemoteHost 
rpm -K $LocalName \|\| exit $?
touch -t $Y$M$D$h$m.$s  $LocalName
rpm --rebuild $LocalName
zz
;
close SH;

