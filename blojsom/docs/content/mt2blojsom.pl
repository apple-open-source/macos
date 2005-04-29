#!/usr/bin/perl

use Time::Local;

my $INPUT = $ARGV[0];
my $OUTPUT_DIR = $ARGV[1];
my $PREFIX = "mt_";
my $SUFFIX = ".textile";


open(FILE,"< $INPUT") || die ("Could not open input file $INPUT\n");

$i = 1;
$in_blody = 0;
my $body = "";
while ($line = <FILE>)
{
	chomp($line);
	if ($line =~ m/BODY:/)
	{
		$in_body = 1;
		print ("Entering body $i\n");
	}
	elsif ($line =~ m/TITLE:\s(.*)/)
	{
		$title = $1;
	}
	elsif ($line =~ m/CATEGORY:\s(.*)/)
	{
		$category = $1;
		$category =~ s/\s/_/g;
	}
	elsif ($line =~ m/DATE:\s(\d\d)\/(\d\d)\/(\d\d\d\d)\s(\d\d):(\d\d):(\d\d)\s(.*)/)
	{
		if ($7 eq "PM" && $4 < 12)
		{
			$hour = $4 + 12;
		}
		$ftime = timelocal($6,$5,$hour,$2,$1-1,$3);
	}
	elsif ($line =~ m/EXTENDED BODY:/)
	{
		$in_body = 0;
	}
	else
	{
		if ($in_body == 1)
		{
			if ($line =~ m/-----/)
			{
				$in_body = 0;

				print ("Exiting $i.txt\n");
				print ("Title : $title\n");
				print ("Body : $body\n");

				if ($body ne "")
				{
					mkdir("$OUTPUT_DIR/$category");
					$file = "$OUTPUT_DIR/$category/$PREFIX" . $i . $SUFFIX;
					open(OUTPUT,"> $file");
					print OUTPUT $title . "\r\n";
					print OUTPUT $body;
					close(OUTPUT);
					# Change file time
					utime($ftime, $ftime, $file);
				}

				$body = "";
				$title = "No Title";

				$i++;
			}
			else
			{
				if ($body eq "")
				{
					$body = $line;
				}
				else
				{
					$body = $body . "\r\n" . $line;
				}
			}
		}
	}
}

close(FILE);
