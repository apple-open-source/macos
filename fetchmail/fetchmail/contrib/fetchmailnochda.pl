#!/usr/bin/perl

# User contribution to fetchmail by Torsten Mueller torsten@archesoft.de
# v1.1 22/may/2001

# the reason for this script is to check, if fetchmail (in daemon mode) works
# you should have perl and the perlmodule File::Compare installed
# File::Compare you can find at http://www.cpan.org/

# installation:
# edit the config part of this script
# create a cronjob , the time it should run should be higher than the pollintervall !!

# possible problems:
# you have set the cron intervall to short
# the script doesn't have permissions to write to directories or to execute fetchmail
# you didn't start fetchmail in daemon mode but use cron to fetch mail
# you can't read my english

# how does it work
# really simple, the script checks, if there was a change to the logfile of fetchmail
# to find this out, the script makes a backup of the original logfile and compares 
# the size of the original and the backup logfile
# i know it's a dirty way, but hey, it works ...
 
use File::Compare;

# config
# where lives fetchmail on your system
$fetchmail = '/usr/bin/fetchmail';
# where should be the logfile for fetchmail
$fetchmaillog = '/var/log/fetchmail.log';
# where could the script write the backup of the logfile
$fetchmailwatch = '/var/log/fetchmailwatch';
# after how many seconds fetchmail should get mail, the poll intervall
$fetchmailtime = '3600';
# which config file should fetchmail use for retrieval
$fetchmailconf = '/root/.fetchmailrc';
# where lives your cp program 
$copycp = 'cp';
#end config

if (!(-e "$fetchmaillog")) {
# es existiert keine logdatei von fetchmail
# there isn't a logfile of fetchmail
print "There seems to be a problem with the fetchmail daemon\n
I couldn't find a logfile of fetchmail.\n
I try to stop and to start fetchmail in daemon mode.\n
If you get this mail more then once, then check your system !\n
------------------------------------------------------------\n
Es ist ein Fehler aufgetreten bei der Ueberwachung des fetchmail Daemons\n
Es existiert keine Logdatei. Ich versuche jetzt fetchmail zu stoppen und neu zu \n
starten. Sollte das Problem nochmal auftreten, dann genaue Systeminspektion !\n
------------------------------------------------------------\n
Das fetchmail Ueberwachungsscript Copyright 2001 by T. Mueller torsten\@archesoft.de\n\n";

system "$fetchmail -q";
sleep 3 ;
system "$fetchmail -f $fetchmailconf -d $fetchmailtime -L $fetchmaillog";
sleep 2 ;

}

if (!(-e "$fetchmailwatch")) {
# die kopie der logdatei existiert nicht
# the copy of the original logfile doesn't exists
print "There seems to be a problem with the fetchmail daemon\n
I couldn't find the copy of the original logfile of fetchmail.\n
If this is this the first run of this script, then this is no problem!\n
If you get this mail more then once, then check your system !\n
------------------------------------------------------------\n
Es ist ein Fehler aufgetreten bei der Ueberwachung des fetchmail Daemons\n
Es existiert keine Kopie der Logdatei. Wenn das Script das erste Mal aufgerufen wurde,\n
dann ist dies kein Problem. Sollte dieses Problem nochmal auftreten, dann genaue Systeminspektion !\n
------------------------------------------------------------\n
Das fetchmail Ueberwachungsscript Copyright 2001 by T. Mueller torsten\@archesoft.de\n\n";
&copylog;
exit; }


$vergleich = compare("$fetchmaillog","$fetchmailwatch");

if ($vergleich == -1) {
# irgendein fehler ist aufgetreten
# unknown error
print "There seems to be a problem with the fetchmail daemon or this script\n
I don't know, why this error happens.
Please check the script and your system
------------------------------------------------------------\n
Es ist ein Fehler aufgetreten bei der Ueberwachung des fetchmail Daemons\n
Bitte die notwendigen Schritte unternehmen, z.B. Festplattenspeicherplatz pruefen\n
noch eine kommt.\n
------------------------------------------------------------\n
Das fetchmail Ueberwachungsscript Copyright 2001 by T. Mueller torsten\@archesoft.de\n\n";
}


if ($vergleich == 0) {
# dateien sind gleich also also eine aktion starten
# the copy and the original logfile have the same size
print "There seems to be a problem with the fetchmail daemon\n
The logfile seems the be the same as the last logfile i have seen.
That could mean, that fetchmail hangs, or permissionproblems or disk full.
I try to stop and to start fetchmail in daemon mode.\n
If you get this mail more then once, then check your system !\n
------------------------------------------------------------\n
Scheinbar gab es ein Problem mit dem Programm fetchmail\n
Die Logdatei war identisch mit der Logdatei beim  letzten Lauf diese Scriptes\n
Daraus schlussfolgere ich, dass nichts mehr geloggt wurde -> fetchmail hat ein Problem\n
Ich habe fetchmail versucht zu stoppen, und wieder neu zu starten.\n
Sollte diese Mail heute noch mehrfach erscheinen, dann ist eine genauere Inspektion\n
der Umstaende notwendig. Ist dies die erste Mail, dann einfach mal abwarten, ob\n
noch eine kommt.\n
------------------------------------------------------------\n
Das fetchmail Ueberwachungsscript Copyright 2001 by T. Mueller torsten\@archesoft.de\n\n";

system "$fetchmail -q";
sleep 3 ;
system "$fetchmail -f $fetchmailconf -d $fetchmailtime -L $fetchmaillog";
sleep 2 ;

}


&copylog;

sub copylog {
system "$copycp $fetchmaillog $fetchmailwatch";
}


